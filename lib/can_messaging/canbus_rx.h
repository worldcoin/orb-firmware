#pragma once

#include "can_messaging.h"

/**
 * Initialize CAN RX handling
 * Spawns a thread that handles incoming CAN(-FD) messages
 * @param in_handler callback to be called when a new message is received
 * @retval RET_SUCCESS on success, error code otherwise
 * @retval RET_ERROR_INVALID_PARAM in_handler is NULL
 * @retval RET_ERROR_NOT_FOUND CAN device not found (not in device tree)
 * @retval RET_ERROR_BUSY CAN device is not ready
 */
ret_code_t canbus_rx_init(ret_code_t (*in_handler)(can_message_t *message));

/**
 * Initialize CAN ISO-TP RX thread
 * Spawns a thread that handles incoming CAN ISO-TP messages
 * @param in_handler callback to be called when a new message is received
 * @retval RET_SUCCESS on success, error code otherwise
 * @retval RET_ERROR_INVALID_PARAM in_handler is NULL
 * @retval RET_ERROR_NOT_FOUND CAN device not found (not in device tree)
 * @retval RET_ERROR_BUSY CAN device is not ready
 */
ret_code_t
    canbus_isotp_rx_init(ret_code_t (*in_handler)(can_message_t *message));
