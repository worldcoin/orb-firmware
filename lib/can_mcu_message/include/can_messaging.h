#ifndef ORB_MCU_MAIN_APP_CANBUS_H
#define ORB_MCU_MAIN_APP_CANBUS_H

#include "errors.h"
#include "mcu_messaging.pb.h"
#include <stddef.h>

ret_code_t
can_messaging_push_tx(McuMessage *message);

/// Init CAN message module
/// Pass a function to handle incoming messages
/// \param in_handler Function that will handle incoming messages
/// \return
ret_code_t
can_messaging_init(void (*in_handler)(McuMessage *msg));

#endif // ORB_MCU_MAIN_APP_CANBUS_H
