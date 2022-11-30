#ifndef ORB_LIB_UART_MESSAGING_H
#define ORB_LIB_UART_MESSAGING_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

/// New UART message, pointers taken directly from DMA-allocated buffers
typedef struct {
    const uint8_t *buffer_addr; //!< address to circular buffer
    const size_t buffer_size;   //!< size of circular buffer
    size_t start_idx;           //!< payload index in circular buffer
    size_t length;              //!< payload length
} uart_message_t;

/// Init UART message module
/// Pass a function to handle incoming messages
/// \param in_handler Function that will handle incoming messages
/// \return
int
uart_messaging_init(void (*in_handler)(void *msg));

#endif // ORB_LIB_UART_MESSAGING_H
