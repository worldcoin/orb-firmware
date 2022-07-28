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

    // directly fill the McuMessage because we need one in blocking mode
    static McuMessage log_msg = {.which_message = McuMessage_m_message_tag,
                                 .message.m_message.which_payload =
                                     McuToJetson_log_tag,
                                 .message.m_message.payload.log.log = {0}};

    memset(log_msg.message.m_message.payload.log.log, 0,
           sizeof(log_msg.message.m_message.payload.log.log));
    memcpy(log_msg.message.m_message.payload.log.log, data, size);

    if (!blocking) {
        // use the pubsub API
        publish_new(&log_msg.message.m_message.payload.log,
                    sizeof(log_msg.message.m_message.payload.log),
                    McuToJetson_log_tag, CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    } else {
        // send in blocking mode by directly using the CAN API
        uint8_t buffer[CAN_FRAME_MAX_SIZE];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &log_msg,
                                    PB_ENCODE_DELIMITED);

        can_message_t to_send = {.destination =
                                     CONFIG_CAN_ADDRESS_DEFAULT_REMOTE,
                                 .bytes = buffer,
                                 .size = stream.bytes_written};

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
