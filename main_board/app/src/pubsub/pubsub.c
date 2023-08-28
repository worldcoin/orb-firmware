#include "pubsub.h"
#include "app_config.h"
#include "logs_can.h"
#include "mcu_messaging.pb.h"
#include "system/diag.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <pb_encode.h>
#include <storage.h>
#include <utils.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(pubsub, CONFIG_PUBSUB_LOG_LEVEL);

// Check that CONFIG_CAN_ISOTP_MAX_SIZE_BYTES is large enough
// print friendly message if CONFIG_CAN_ISOTP_MAX_SIZE_BYTES can be reduced
#ifdef CONFIG_CAN_ISOTP_MAX_SIZE_BYTES
BUILD_ASSERT(
    CONFIG_CAN_ISOTP_MAX_SIZE_BYTES >= McuMessage_size,
    "CONFIG_CAN_ISOTP_MAX_SIZE_BYTES must be at least McuMessage_size");
#if CONFIG_CAN_ISOTP_MAX_SIZE_BYTES > McuMessage_size
#pragma message                                                                       \
    "You can reduce CONFIG_CAN_ISOTP_MAX_SIZE_BYTES to McuMessage_size = " STRINGIFY( \
        McuMessage_size)
#endif
#endif

K_SEM_DEFINE(pub_buffers_sem, 1, 1);

static K_THREAD_STACK_DEFINE(pub_stored_stack_area,
                             THREAD_STACK_SIZE_PUB_STORED);
static struct k_thread pub_stored_thread_data;

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

const struct sub_message_s sub_prios[] = {
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
    [McuToJetson_battery_diag_common_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_tof_1d_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_gnss_partial_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_front_als_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_hardware_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_hardware_diag_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_battery_reset_reason_tag] = {.priority = SUB_PRIO_STORE},
    [McuToJetson_battery_diag_safety_tag] = {.priority = SUB_PRIO_DISCARD},
    [McuToJetson_battery_diag_permanent_fail_tag] = {.priority =
                                                         SUB_PRIO_DISCARD},
    [McuToJetson_battery_info_hw_fw_tag] = {.priority = SUB_PRIO_STORE},
    [McuToJetson_battery_info_max_values_tag] = {.priority = SUB_PRIO_STORE},
    [McuToJetson_battery_info_soc_and_statistics_tag] = {.priority =
                                                             SUB_PRIO_STORE},
};

/* ISO-TP addresses + one CAN-FD address */
static uint32_t active_remotes[(CONFIG_CAN_ISOTP_REMOTE_APP_COUNT + 1) + 1] = {
    0};

bool
publish_is_started(uint32_t remote)
{
    for (size_t i = 0; i < ARRAY_SIZE(active_remotes); i++) {
        if (active_remotes[i] == remote) {
            return true;
        }
    }
    return false;
}

static void
pub_stored_thread()
{
    int err_code;
    struct pub_entry_s record;

    diag_sync(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

    while (true) {
        size_t size = sizeof(record);
        err_code = storage_peek((char *)&record, &size);
        switch (err_code) {
        case RET_SUCCESS:
            // do nothing
            break;
        case RET_ERROR_NOT_FOUND:
            LOG_INF("Done flushing stored messages");
            // no more records, terminate thread
            return;
        case RET_ERROR_NO_MEM:
        case RET_ERROR_INVALID_STATE:
        default:
            LOG_ERR("Discarding stored record, err %u", err_code);
            storage_free();
            continue;
        }

        if (!publish_is_started(record.destination)) {
            // storage is a fifo so come back later
            return;
        }

        can_message_t to_send = {
            .destination = record.destination,
            .bytes = record.data,
            .size = size,
        };

        if (to_send.destination & CAN_ADDR_IS_ISOTP) {
            err_code = can_isotp_messaging_async_tx(&to_send);
        } else {
            err_code = can_messaging_async_tx(&to_send);
        }

        if (err_code) {
            LOG_WRN(
                "Queued stored %s message for sending to remote 0x%03x; "
                "ret %d",
                (to_send.destination & CAN_ADDR_IS_ISOTP ? "ISO-TP" : "CAN"),
                to_send.destination, err_code);
        } else {
            LOG_DBG(
                "Queued stored %s message for sending to remote 0x%03x; "
                "ret %d",
                (to_send.destination & CAN_ADDR_IS_ISOTP ? "ISO-TP" : "CAN"),
                to_send.destination, err_code);
        }

        switch (err_code) {
        case RET_SUCCESS:
        case RET_ERROR_INVALID_PARAM: // record cannot be sent, free it
            err_code = storage_free();
            ASSERT_SOFT(err_code);
            break;
        case RET_ERROR_INVALID_STATE:
        case RET_ERROR_BUSY:
        case RET_ERROR_NO_MEM:
            // come back later
            return;
        default:
            LOG_WRN("Unhandled %d", err_code);
            break;
        }

        // throttle the sending of statuses to avoid flooding the CAN bus
        // and CAN controller
        k_msleep(10);
    }
}

// This function may only be called from one thread
int
subscribe_add(uint32_t remote_addr)
{
    static bool started_once = false;

    bool added = false;
    for (size_t i = 0; i < ARRAY_SIZE(active_remotes); i++) {
        if (active_remotes[i] == 0 || active_remotes[i] == remote_addr) {
            if (active_remotes[i] == 0) {
                LOG_INF("Added subscriber 0x%03x", remote_addr);
            }
            active_remotes[i] = remote_addr;
            added = true;
            break;
        }
    }
    if (!added) {
        ASSERT_SOFT(RET_ERROR_NO_MEM);
        return RET_ERROR_NO_MEM;
    }

    if ((started_once == false ||
         (k_thread_join(&pub_stored_thread_data, K_NO_WAIT) == 0)) &&
        (storage_has_data() || diag_has_data())) {
        k_thread_create(&pub_stored_thread_data, pub_stored_stack_area,
                        K_THREAD_STACK_SIZEOF(pub_stored_stack_area),
                        pub_stored_thread, NULL, NULL, NULL,
                        THREAD_PRIORITY_PUB_STORED, 0, K_NO_WAIT);
        k_thread_name_set(&pub_stored_thread_data, "pub_stored");
        started_once = true;
    }

    return RET_SUCCESS;
}

static int
publish(void *payload, size_t size, uint32_t which_payload,
        uint32_t remote_addr, bool store)
{
    int err_code = RET_SUCCESS;

    // ensure:
    // - payload is smaller than McuToJetson payload size
    // - which_payload is supported
    if (which_payload >= ARRAY_SIZE(sub_prios) ||
        size > STRUCT_MEMBER_SIZE_BYTES(McuToJetson, payload)) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (!store && !publish_is_started(remote_addr) &&
        sub_prios[which_payload].priority == SUB_PRIO_DISCARD) {
        return RET_ERROR_OFFLINE;
    }

    // static structs, don't take caller stack
    static struct pub_entry_s entry;
    static McuMessage message = {.version = Version_VERSION_0,
                                 .which_message = McuMessage_m_message_tag,
                                 .message.m_message = {0}};

    // no wait if ISR
    k_timeout_t timeout = k_is_in_isr() ? K_NO_WAIT : K_MSEC(5);
    int ret = k_sem_take(&pub_buffers_sem, timeout);
    if (ret == 0) {
        // copy payload
        message.message.m_message.which_payload = which_payload;
        memset(&message.message.m_message.payload, 0,
               sizeof(message.message.m_message.payload));
        memcpy(&message.message.m_message.payload, payload, size);

        // encode full McuMessage
        pb_ostream_t stream =
            pb_ostream_from_buffer(entry.data, sizeof(entry.data));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &message,
                                    PB_ENCODE_DELIMITED);

        if (encoded) {
            if (store ||
                (!publish_is_started(remote_addr) &&
                 sub_prios[which_payload].priority == SUB_PRIO_STORE)) {
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

                // error code to warn caller that the message
                // hasn't been published in case it wasn't aimed to be stored
                if (!store) {
                    err_code = RET_ERROR_OFFLINE;
                }
            } else {
                // !store && SUB_PRIO_TRY_SENDING

                can_message_t to_send = {
                    .destination = remote_addr,
                    .bytes = entry.data,
                    .size = stream.bytes_written,
                };

                LOG_DBG("⬆️ Sending %s message to remote 0x%03x with payload "
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
            LOG_ERR("PB encoding failed: %s", stream.errmsg);
            return RET_ERROR_INTERNAL;
        }

        k_sem_give(&pub_buffers_sem);
    } else {
        err_code = RET_ERROR_BUSY;
    }

    return err_code;
}

int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr)
{
    return publish(payload, size, which_payload, remote_addr, false);
}

int
publish_store(void *payload, size_t size, uint32_t which_payload,
              uint32_t remote_addr)
{
    return publish(payload, size, which_payload, remote_addr, true);
}
