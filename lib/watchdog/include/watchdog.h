#pragma once

#include <stdbool.h>

/**
 * @brief Perform checks to determine if watchdog should be fed
 *
 * Weakly defined function in `watchdog.c` that can be overridden by the user
 */
bool
watchdog_perform_checks(void);

/**
 * @brief Stop feeding the watchdog
 */
void
watchdog_stop_feed(void);

#if !defined(CONFIG_ORB_LIB_WATCHDOG_SYS_INIT)

/**
 * Setup watchdog & spawn low-priority thread to reload the watchdog
 *
 * @retval 0 on success
 * @retval RET_ERROR_NOT_INITIALIZED on failure: watchdog peripheral not ready
 * or unable to install timeout
 * @retval RET_ERROR_ALREADY_INITIALIZED if already initialized
 */
int
watchdog_init(void);

#endif
