#include "app_watchdog.h"

/**
 * @brief Watchdog Pre-Feed Callback Function
 *
 * This callback function is called from the watchdog feed function to check
 * the health of application specific system conditions to decide whether
 * or not to kick or feed the watchdog to prevent a system reset
 *
 * @param void This function takes no parameters
 * @return Boolean value of system health
 *          - true if watchdog should be fed/kicked
 *          - false if watchdog should not be fed/kicked
 *
 * @note NA
 * @see NA
 */
bool app_watchdog_callback(void) {
    // Stub returning true for now, will be filled out later
    return true;
}