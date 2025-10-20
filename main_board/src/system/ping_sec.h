#pragma once
#include "zephyr/sys_clock.h"

/**
 * Ping the security MCU once
 * @param arg optional, shell pointer in case of shell usage, keep NULL
 * otherwise
 * @param sem_timeout timeout for semaphore take
 * @return RET_SUCCESS on success, error code otherwise
 */
int
ping_sec(const void *arg, const k_timeout_t sem_timeout);

/**
 * Start periodic pinging of the security MCU every 30 seconds
 * @return 0 on success, negative error code otherwise
 */
int
ping_sec_init(void);

/**
 * Stop periodic pinging of the security MCU
 * @return 0 on success, negative error code otherwise
 */
int
ping_sec_cancel(void);
