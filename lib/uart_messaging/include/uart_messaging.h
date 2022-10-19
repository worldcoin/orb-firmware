#ifndef ORB_LIB_UART_MESSAGING_H
#define ORB_LIB_UART_MESSAGING_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *bytes;
    size_t size;
} uart_message_t;

/// Init UART message module
/// Pass a function to handle incoming messages
/// \param in_handler Function that will handle incoming messages
/// \return
int
uart_messaging_init(void (*in_handler)(void *msg));

#endif // ORB_LIB_UART_MESSAGING_H
