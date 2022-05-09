#ifndef ORB_MCU_MAIN_APP_CANBUS_H
#define ORB_MCU_MAIN_APP_CANBUS_H

#include "errors.h"
#include "mcu_messaging.pb.h"
#include <stddef.h>

/// Send new message
/// \param message
/// \return RET_SUCCESS on success, error code otherwise
ret_code_t
can_messaging_async_tx(McuMessage *message);

/// Send CAN message and wait for completion (1-second timeout)
/// ⚠️ Cannot be used in ISR context
/// \param message to be sent
/// \return
/// * RET_SUCCESS on success
/// * RET_ERROR_INVALID_STATE if run from ISR
/// * RET_ERROR_INVALID_PARAM if error encoding the message
/// * RET_ERROR_INTERNAL for CAN errors
ret_code_t
can_messaging_blocking_tx(McuMessage *message);

/// Init CAN message module
/// Pass a function to handle incoming messages
/// \param in_handler Function that will handle incoming messages
/// \return
ret_code_t
can_messaging_init(void (*in_handler)(McuMessage *msg));

#endif // ORB_MCU_MAIN_APP_CANBUS_H
