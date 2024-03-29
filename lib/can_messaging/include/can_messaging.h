#pragma once

#include "errors.h"
#include <zephyr/drivers/can.h>

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

/**
 * Send new message using CAN-FD
 * @param message
 * @return RET_SUCCESS on success, error code otherwise
 **/
ret_code_t
can_messaging_async_tx(const can_message_t *message);

/**
 * Send new message using ISO-TP transport
 * @param message
 * @return RET_SUCCESS on success, error code otherwise
 **/
ret_code_t
can_isotp_messaging_async_tx(const can_message_t *message);

/**
 * Send CAN message and wait for completion (1-second timeout)
 * ⚠️ Cannot be used in ISR context
 * @param message to be sent
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if run from ISR
 * @retval RET_ERROR_INVALID_STATE if can device is not active
 * @retval RET_ERROR_INVALID_PARAM if error encoding the message
 * @retval RET_ERROR_INTERNAL for CAN errors
 **/
ret_code_t
can_messaging_blocking_tx(const can_message_t *message);

/**
 * Stop CAN device
 * Hard reset on failure
 * @return -EALREADY or 0 on success
 */
ret_code_t
can_messaging_suspend(void);

/**
 * Start CAN device
 * Queues are going to be reset in a separate workqueue
 * Hard reset on failure
 * @return -EALREADY or 0 on success
 */
ret_code_t
can_messaging_resume(void);

/**
 * Init CAN message module
 * Pass a function to handle incoming messages
 * @param in_handler Function that will handle incoming messages
 * @retval RET_ERROR_NOT_INITIALIZED if CAN device not found (no definition in
 *device tree)
 * @retval RET_ERROR_INTERNAL if unable to configure CAN & CAN ISO-TP handlers
 *for TX and RX
 * @return value from @c can_set_mode()
 **/
ret_code_t can_messaging_init(ret_code_t (*in_handler)(can_message_t *message));
