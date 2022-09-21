#ifndef ORB_LIB_CAN_MESSAGING_H
#define ORB_LIB_CAN_MESSAGING_H

#include "errors.h"
#include <drivers/can.h>
#include <stddef.h>
#include <stdint.h>

/// Maximum CAN frame size depends on CAN driver configuration
#define CAN_FRAME_MAX_SIZE (CAN_MAX_DLEN)

/// ISO-TP addressing scheme
/// 11-bit standard ID
/// | 10     | 9       | 8        |    [4-7]  |   [0-3]  |
/// | ------ | ------- | -------- | --------- | -------- |
/// | rsrvd  | is_dest | is_isotp | source ID | dest ID  |
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

/// CAN message holder
typedef struct {
    uint32_t destination; // CAN ID the message is sent to
    uint8_t *bytes;       // pointer to the CAN message payload
    size_t size;          // actual number of bytes used in the `bytes` member
} can_message_t;

/// Send new message using CAN-FD
/// \param message
/// \return RET_SUCCESS on success, error code otherwise
ret_code_t
can_messaging_async_tx(const can_message_t *message);

/// Send new message using ISO-TP transport
/// \param message
/// \return RET_SUCCESS on success, error code otherwise
ret_code_t
can_isotp_messaging_async_tx(const can_message_t *message);

/// Send CAN message and wait for completion (1-second timeout)
/// ⚠️ Cannot be used in ISR context
/// \param message to be sent
/// \return
/// * RET_SUCCESS on success
/// * RET_ERROR_INVALID_STATE if run from ISR
/// * RET_ERROR_INVALID_PARAM if error encoding the message
/// * RET_ERROR_INTERNAL for CAN errors
ret_code_t
can_messaging_blocking_tx(const can_message_t *message);

/// Reset CAN TX queues, keep RX threads running
/// Can be used in ISR context
/// \return RET_SUCCESS on success, error code otherwise
ret_code_t
can_messaging_reset_async(void);

/// Init CAN message module
/// Pass a function to handle incoming messages
/// \param in_handler Function that will handle incoming messages
/// \return
ret_code_t
can_messaging_init(void (*in_handler)(can_message_t *msg));

#endif // ORB_LIB_CAN_MESSAGING_H
