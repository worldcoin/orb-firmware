#include "pubsub.h"
#include "app_config.h"
#include "mcu_messaging.pb.h"
#include <app_assert.h>
#include <assert.h>
#include <can_messaging.h>
#include <pb_encode.h>
#include <storage.h>
#include <utils.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(pubsub, CONFIG_PUBSUB_LOG_LEVEL);

// Check that CONFIG_CAN_ISOTP_MAX_SIZE_BYTES is large enough
// print friendly message if CONFIG_CAN_ISOTP_MAX_SIZE_BYTES can be reduced
#ifdef CONFIG_CAN_ISOTP_MAX_SIZE_BYTES
static_assert(
    CONFIG_CAN_ISOTP_MAX_SIZE_BYTES >= McuMessage_size,
    "CONFIG_CAN_ISOTP_MAX_SIZE_BYTES must be at least McuMessage_size");
#if CONFIG_CAN_ISOTP_MAX_SIZE_BYTES > McuMessage_size
#pragma message                                                                       \
    "You can reduce CONFIG_CAN_ISOTP_MAX_SIZE_BYTES to McuMessage_size = " STRINGIFY( \
        McuMessage_size)
#endif
#endif

K_MUTEX_DEFINE(new_pub_mutex);

static bool sending = false;
static k_tid_t pub_stored_tid = NULL;

static K_THREAD_STACK_DEFINE(pub_stored_stack_area,
                             THREAD_STACK_SIZE_PUB_STORED);
static struct k_thread pub_stored_thread_data;

#if JetsonToMcu_size >= JetsonToSec_size &&                                    \
    JetsonToMcu_size >= McuToJetson_size &&                                    \
    JetsonToMcu_size >= SecToJetson_size
#define MCU_MESSAGE_ENCODED_WRAPPER_SIZE (McuMessage_size - JetsonToMcu_size)
#else
// A payload is larger than JetsonToMcu which does not allow to find the number
// of bytes needed to wrap the payload into an McuMessage. Please use another
// field to calculate the number of bytes needed to wrap the payload.
#error Unable to calculate bytes needed to wrap a payload into an McuMessage.
#endif

/// Structure holding a message to store when sending isn't enabled
/// `data` contains an `McuToJetson` into an `McuMessage`.
/// The McuMessage is encoded (serialized).
///
/// \note Up to 5 bytes are used for wrapping the `McuToJetson` into an
/// `McuMessage`
struct pub_entry_s {
    uint32_t destination;
    uint8_t data[McuToJetson_size + MCU_MESSAGE_ENCODED_WRAPPER_SIZE];
};

enum sub_priority_e {
    SUB_PRIO_STORE = 0,   // store message and send it later
    SUB_PRIO_TRY_SENDING, // try sending anyway, message is queued in the tx
                          // queue
    SUB_PRIO_DISCARD,     // discard the message
};

/// Subscriptions
/// With priorities in case sending isn't available
struct sub_message_s {
    enum sub_priority_e priority;
};

struct sub_message_s sub_prios[] = {
    [McuToJetson_ack_tag] = {.priority = SUB_PRIO_TRY_SENDING},
    [McuToJetson_power_button_tag] = {.priority = SUB_PRIO_TRY_SENDING},
    [McuToJetson_battery_voltage_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_battery_capacity_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_gnss_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_versions_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_temperature_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_fan_status_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_imu_data_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_voltage_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_log_tag] = {.priority = SUB_PRIO_STORE},
    [McuToJetson_motor_range_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_fatal_error_tag] = {.priority = SUB_PRIO_STORE},
    [McuToJetson_battery_is_charging_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_battery_diag_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_tof_1d_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_gnss_partial_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_front_als_tag] = {.priority = SUB_PRIO_DISCARD},
};

static void
pub_stored_thread()
{
    int err_code;
    struct pub_entry_s record;

    LOG_INF("Flushing stored messages");

    while (pub_stored_tid != NULL) {
        size_t size = sizeof(record);
        err_code = storage_peek((char *)&record, &size);
        switch (err_code) {
        case RET_SUCCESS:
            // do nothing
            break;
        case RET_ERROR_NOT_FOUND:
            // no more records, terminate thread
            pub_stored_tid = NULL;
            continue;
        case RET_ERROR_NO_MEM:
        case RET_ERROR_INVALID_STATE:
        default:
            LOG_ERR("Discarding stored record, err %u", err_code);
            storage_free();
            continue;
        }

        can_message_t to_send = {
            .destination = record.destination,
            .bytes = record.data,
            .size = size,
        };

        int ret = k_mutex_lock(&new_pub_mutex, K_MSEC(5));
        if (ret == 0) {
            LOG_DBG(
                "⬆️ Sending stored %s message to remote 0x%03x",
                (to_send.destination & CAN_ADDR_IS_ISOTP ? "ISO-TP" : "CAN"),
                to_send.destination);

            if (to_send.destination & CAN_ADDR_IS_ISOTP) {
                err_code = can_isotp_messaging_async_tx(&to_send);
            } else {
                err_code = can_messaging_async_tx(&to_send);
            }

            k_mutex_unlock(&new_pub_mutex);

            if (err_code) {
                // come back later
                pub_stored_tid = NULL;
            } else {
                err_code = storage_free();
                ASSERT_SOFT(err_code);
            }
        }
    }
}

void
publish_start(void)
{
    sending = true;

    if (pub_stored_tid == NULL && storage_has_data()) {
        pub_stored_tid = k_thread_create(
            &pub_stored_thread_data, pub_stored_stack_area,
            K_THREAD_STACK_SIZEOF(pub_stored_stack_area), pub_stored_thread,
            NULL, NULL, NULL, THREAD_PRIORITY_PUB_STORED, 0, K_NO_WAIT);
        k_thread_name_set(&pub_stored_thread_data, "pub_stored");
    }
}

int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr)
{
    int err_code = RET_SUCCESS;

    // ensure:
    // - payload is smaller than McuToJetson payload size
    // - which_payload is supported
    if (which_payload > ARRAY_SIZE(sub_prios) ||
        size > STRUCT_MEMBER_SIZE_BYTES(McuToJetson, payload)) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (!sending && sub_prios[which_payload].priority == SUB_PRIO_DISCARD) {
        return RET_ERROR_OFFLINE;
    }

    // static structs, don't take caller stack
    static struct pub_entry_s entry;
    static McuMessage message = {.version = Version_VERSION_0,
                                 .which_message = McuMessage_m_message_tag,
                                 .message.m_message = {0}};

    int ret = k_mutex_lock(&new_pub_mutex, K_MSEC(5));
    if (ret == 0) {
        // copy payload
        message.message.m_message.which_payload = which_payload;
        memcpy(&message.message.m_message.payload, payload, size);

        // encode full McuMessage
        pb_ostream_t stream =
            pb_ostream_from_buffer(entry.data, sizeof(entry.data));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &message,
                                    PB_ENCODE_DELIMITED);

        if (encoded) {
            if (!sending &&
                sub_prios[which_payload].priority == SUB_PRIO_STORE) {
                entry.destination = remote_addr;

                // store message to be sent later
                err_code =
                    storage_push((char *)&entry, stream.bytes_written +
                                                     sizeof(entry.destination));
                if (err_code) {
                    LOG_INF("Unable to store message: %d", err_code);
                } else {
                    LOG_INF("Stored payload %u", which_payload);
                }

                err_code = RET_ERROR_OFFLINE;
            } else {
                // sending || (!sending && SUB_PRIO_TRY_SENDING)

                can_message_t to_send = {
                    .destination = remote_addr,
                    .bytes = entry.data,
                    .size = stream.bytes_written,
                };

                LOG_DBG(
                    "⬆️ Sending %s message to remote 0x%03x with payload "
                    "ID %02d",
                    (remote_addr & CAN_ADDR_IS_ISOTP ? "ISO-TP" : "CAN"),
                    to_send.destination, which_payload);
                if (remote_addr & CAN_ADDR_IS_ISOTP) {
                    err_code = can_isotp_messaging_async_tx(&to_send);
                } else {
                    err_code = can_messaging_async_tx(&to_send);
                }
            }
        } else {
            LOG_ERR("PB encoding failed");
        }

        k_mutex_unlock(&new_pub_mutex);
    } else {
        err_code = RET_ERROR_BUSY;
    }

    return err_code;
}
