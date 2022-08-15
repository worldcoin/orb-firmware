#include "pubsub.h"
#include "mcu_messaging.pb.h"
#include <app_assert.h>
#include <assert.h>
#include <can_messaging.h>
#include <pb_encode.h>
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

void
publish_start(void)
{
    sending = true;
}

int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr)
{
    int err_code = RET_SUCCESS;

    if (!sending) {
        return RET_ERROR_OFFLINE;
    }

    // make sure payload is smaller than McuToJetson payload size
    if (size > STRUCT_MEMBER_SIZE_BYTES(McuToJetson, payload)) {
        return RET_ERROR_INVALID_PARAM;
    }

    // static structs, don't take caller stack
    static uint8_t buffer[McuMessage_size];
    static McuMessage message = {.version = Version_VERSION_0,
                                 .which_message = McuMessage_m_message_tag,
                                 .message.m_message = {0}};

    int ret = k_mutex_lock(&new_pub_mutex, K_MSEC(5));
    if (ret == 0) {
        // copy payload
        message.message.m_message.which_payload = which_payload;
        memcpy(&message.message.m_message.payload, payload, size);

        // encode full McuMessage
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &message,
                                    PB_ENCODE_DELIMITED);

        can_message_t to_send = {
            .destination = remote_addr,
            .bytes = buffer,
            .size = stream.bytes_written,
        };

        // send
        if (encoded) {
            LOG_DBG(
                "⬆️ Sending %s message to remote 0x%03x with payload ID "
                "%02d",
                (remote_addr & CAN_ADDR_IS_ISOTP ? "ISO-TP" : "CAN"),
                to_send.destination, which_payload);
            if (remote_addr & CAN_ADDR_IS_ISOTP) {
                err_code = can_isotp_messaging_async_tx(&to_send);
                ASSERT_SOFT(err_code);
            } else {
                err_code = can_messaging_async_tx(&to_send);
                ASSERT_SOFT(err_code);
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
