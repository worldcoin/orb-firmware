#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize the logging modules
 *
 * This function initializes logging, with either:
 *  - CAN backend used with Zephyr logging
 *  - printk messages redirected to CAN bus
 *
 * @param print Function pointer to wrap and send the log message over CAN
 * @return RET_SUCCESS
 */
int
logs_init(void (*print)(const char *log, size_t size, bool blocking));
