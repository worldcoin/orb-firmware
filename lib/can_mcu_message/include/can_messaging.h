#ifndef ORB_MCU_MAIN_APP_CANBUS_H
#define ORB_MCU_MAIN_APP_CANBUS_H

#include "errors.h"
#include "mcu_messaging.pb.h"
#include <stddef.h>

enum can_type_e {
    CAN_RAW,
    CAN_ISOTP,
};

/// ISO-TP addressing scheme
/// 11-bit standard ID
/// | 10     | 9       | 8        |    [4-7]  |   [0-3]  |
/// | ------ | ------- | -------- | --------- | -------- |
/// | is_app | is_dest | is_isotp | source ID | dest ID  |
#define CAN_ADDR_IS_ISOTP             (1 << 8)
#define CAN_ADDR_IS_DEST              (1 << 9)
#define CAN_ADDR_SOURCE_ID_POS        (4)
#define CAN_ADDR_IS_ISOTP_DESTINATION (CAN_ADDR_IS_ISOTP | CAN_ADDR_IS_DEST)
#define CAN_ADDR_IS_ISOTP_SOURCE      (CAN_ADDR_IS_ISOTP)

/// helpers to create source and destination CAN standard ID
#define CAN_ISOTP_STDID_DESTINATION(src, dest)                                 \
    (CAN_ADDR_IS_ISOTP_DESTINATION | src << CAN_ADDR_SOURCE_ID_POS | dest)
#define CAN_ISOTP_STDID_SOURCE(src, dest)                                      \
    (CAN_ADDR_IS_ISOTP_SOURCE | src << CAN_ADDR_SOURCE_ID_POS | dest)

/// Send new message
/// \param message
/// \return RET_SUCCESS on success, error code otherwise
ret_code_t
can_messaging_async_tx(McuMessage *message);

/// Send new message using ISO-TP transport
/// \param message
/// \return RET_SUCCESS on success, error code otherwise
ret_code_t
can_isotp_messaging_async_tx(McuMessage *message);

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
