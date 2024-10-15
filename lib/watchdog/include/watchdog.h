#pragma once

#include <stdbool.h>
/**
 * Setup watchdog & spawn low-priority thread to reload the watchdog
 *
 * @param callback Function to be called periodically to check if watchdog
 *                 should be fed. Should return true to feed watchdog
 *                  or false to not feed.
 * @retval 0 on success
 * @retval RET_ERROR_NOT_INITIALIZED on failure: watchdog peripheral not ready
 * or unable to install timeout
 * @retval RET_ERROR_ALREADY_INITIALIZED if already initialized
 */
int
watchdog_init(bool (*callback)(void));
