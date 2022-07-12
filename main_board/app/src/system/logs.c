#include "logs.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <errors.h>
#include <log_backend_can.h>
#include <pb_encode.h>
#include <utils.h>

void
print_log_can(const char *data, size_t size, bool blocking)
{
    if (size > STRUCT_MEMBER_ARRAY_SIZE(Log, log)) {
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
        return;
    }
    Log log = {0};
    memcpy(log.log, data, size);

    if (!blocking) {
        // use the publish API
        publish_new(&log, sizeof(log), McuToJetson_log_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    } else {
        // send in blocking mode by directly using the CAN API
        McuMessage log_msg = {.which_message = McuMessage_m_message_tag,
                              .message.m_message.which_payload =
                                  McuToJetson_log_tag,
                              .message.m_message.payload.log = log};

        can_message_t to_send;
        pb_ostream_t stream =
            pb_ostream_from_buffer(to_send.bytes, sizeof(to_send.bytes));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &log_msg,
                                    PB_ENCODE_DELIMITED);
        to_send.size = stream.bytes_written;
        to_send.destination = CONFIG_CAN_ADDRESS_DEFAULT_REMOTE;

        if (encoded) {
            (void)can_messaging_blocking_tx(&to_send);
        }
    }
}

int
logs_init(void)
{
#ifdef CONFIG_ORB_LIB_LOG_BACKEND_CAN
    log_backend_can_register_print(print_log_can);
#endif

    return RET_SUCCESS;
}
