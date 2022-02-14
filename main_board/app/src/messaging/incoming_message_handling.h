#ifndef INCOMING_MESSAGE_HANDLING_H
#define INCOMING_MESSAGE_HANDLING_H

#include <mcu_messaging.pb.h>

void
incoming_message_handle(McuMessage *msg);

void
incoming_message_ack(Ack_ErrorCode error, uint32_t ack_number);

uint32_t
incoming_message_acked_counter(void);

#endif // INCOMING_MESSAGE_HANDLING_H
