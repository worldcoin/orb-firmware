#pragma once

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

/**
 * New UART message
 * In order to keep message-passing as fast as possible between threads,
 * pointers to large memory buffers are used in this structure.
 * ⚠️ Addresses points to DMA-allocated buffers which might be overwritten
 *    if not processed in time.
 **/
typedef struct {
    const uint8_t *buffer_addr; //!< address to circular buffer
    const size_t buffer_size;   //!< size of circular buffer
    size_t start_idx;           //!< payload index in circular buffer
    size_t length;              //!< payload length
} uart_message_t;

#ifdef CONFIG_PM
/**
 * @brief Suspend UART messaging
 * For low power modes, the UART messaging module can be suspended.
 *
 * @retval RET_SUCCESS
 * @retval RET_ERROR_INVALID_STATE
 **/
int
uart_messaging_suspend(void);

/**
 * @brief Resume UART messaging
 * Resume UART messaging after a suspend to bring back UART messaging to normal
 * operation after it has been suspended.
 *
 * @retval RET_SUCCESS
 * @retval RET_ERROR_INVALID_STATE
 **/
int
uart_messaging_resume(void);
#endif

/**
 * Init UART message module
 * Pass a function to handle incoming messages
 *
 * @param in_handler Function that will handle incoming messages
 *
 * @retval RET_ERROR_INVALID_PARAM if in_handler is NULL
 * @retval RET_ERROR_NOT_INITIALIZED if UART device is not initialized
 * @retval RET_ERROR_INVALID_STATE if UART device is not ready
 * @retval RET_ERROR_INTERNAL if UART device cannot be configured
 * @retval RET_SUCCESS on success
 **/
int uart_messaging_init(ret_code_t (*in_handler)(uart_message_t *msg));
