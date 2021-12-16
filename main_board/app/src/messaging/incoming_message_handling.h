#ifndef INCOMING_MESSAGE_HANDLING_H
#define INCOMING_MESSAGE_HANDLING_H

#include <mcu_messaging.pb.h>

void
handle_incoming_message(McuMessage *msg);

#endif // INCOMING_MESSAGE_HANDLING_H
