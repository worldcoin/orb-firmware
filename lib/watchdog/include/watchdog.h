#pragma once

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
