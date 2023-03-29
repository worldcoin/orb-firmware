#ifndef RUNNER_H
#define RUNNER_H

#include <errors.h>
#include <stdint.h>

/// Get number of successful run jobs
uint32_t
runner_successful_jobs_count(void);

/// Queue new message handling
ret_code_t
runner_handle_new_can(void *msg);

#if CONFIG_ORB_LIB_UART_MESSAGING
#include <uart_messaging.h>

/// Queue new message sent using UART
/// \param msg
void
runner_handle_new_uart(void *msg);
#endif

#endif // RUNNER_H
