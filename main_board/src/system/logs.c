#include "logs.h"
#include "mcu_messaging_main.pb.h"
#include "pubsub/pubsub.h"
#include <can_messaging.h>
#include <pb_encode.h>
#include <utils.h>

void
logs_can(const char *data, size_t size, bool blocking)
{
    if (size > STRUCT_MEMBER_ARRAY_SIZE(Log, log)) {
        return;
    }

    // directly fill the McuMessage because we need one in blocking mode
    static McuMessage log_msg = {.which_message = McuMessage_m_message_tag,
                                 .message.m_message.which_payload =
                                     McuToJetson_log_tag,
                                 .message.m_message.payload.log.log = {0}};

    memcpy(log_msg.message.m_message.payload.log.log, data, size);
    memset(&log_msg.message.m_message.payload.log.log[size], 0,
           (sizeof(log_msg.message.m_message.payload.log.log) - size));

    if (!blocking) {
        // use the pubsub API
        publish_new(&log_msg.message.m_message.payload.log, size,
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
