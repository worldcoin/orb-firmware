#include "pubsub.h"
#include "mcu_messaging.pb.h"
#include <can_messaging.h>
#include <pb_encode.h>
#include <utils.h>
#include <zephyr.h>

#include <app_assert.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(pubsub);

K_MUTEX_DEFINE(new_pub_mutex);

int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr)
{
    int err_code = RET_SUCCESS;

    // make sure payload is smaller than McuToJetson payload size
    if (size > STRUCT_MEMBER_SIZE_BYTES(McuToJetson, payload)) {
        return RET_ERROR_INVALID_PARAM;
    }

    // static structs, don't take caller stack
    static can_message_t to_send = {0};
    static McuMessage message = {.version = Version_VERSION_0,
                                 .which_message = McuMessage_m_message_tag,
                                 .message.m_message = {0}};

    int ret = k_mutex_lock(&new_pub_mutex, K_MSEC(5));
    if (ret == 0) {
        // reset struct before filling it
        memset(&message.message.m_message.payload, 0,
               STRUCT_MEMBER_SIZE_BYTES(McuToJetson, payload));

        // copy payload
        message.message.m_message.which_payload = which_payload;
        memcpy(&message.message.m_message.payload, payload, size);

        // encode full McuMessage
        pb_ostream_t stream =
            pb_ostream_from_buffer(to_send.bytes, sizeof(to_send.bytes));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &message,
                                    PB_ENCODE_DELIMITED);
        to_send.size = stream.bytes_written;
        to_send.destination = remote_addr;

        // send
        if (encoded) {
            if (remote_addr & CAN_ADDR_IS_ISOTP) {
                LOG_DBG("ISO-TP");
                err_code = can_isotp_messaging_async_tx(&to_send);
                ASSERT_SOFT(err_code);
            } else {
                LOG_DBG("CAN");
                err_code = can_messaging_async_tx(&to_send);
                ASSERT_SOFT(err_code);
            }
        }

        k_mutex_unlock(&new_pub_mutex);
    } else {
        err_code = RET_ERROR_BUSY;
    }

    return err_code;
}
