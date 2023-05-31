#pragma once

#include <stdbool.h>
#include <stddef.h>

/// Register the print function for any logs going through the CAN backend
/// If this function is not used, logs won't be sent over CAN
/// The printing function must provide an async and synchronous/interrupt less
/// (blocking) way to send the logs over CAN (blocking mode used if error is
/// being handled)
/// \param print Pointer to function
void
log_backend_can_register_print(void (*print)(const char *log, size_t size,
                                             bool blocking));
