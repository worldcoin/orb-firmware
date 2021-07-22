//
// Created by Cyril on 22/07/2021.
//

#ifndef ORB_FIRMWARE_ORB_COMMON_DIAG_H
#define ORB_FIRMWARE_ORB_COMMON_DIAG_H

/// @brief  Possible STM32 system reset causes
typedef enum reset_cause_e
{
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_LOW_POWER_RESET,
    RESET_CAUSE_WINDOW_WATCHDOG_RESET,
    RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET,
    RESET_CAUSE_SOFTWARE_RESET,
    RESET_CAUSE_POWER_ON_POWER_DOWN_RESET,
    RESET_CAUSE_EXTERNAL_RESET_PIN_RESET,
    RESET_CAUSE_BROWNOUT_RESET,
} diag_reset_cause_t;

diag_reset_cause_t
reset_cause_get(void);

#ifdef DEBUG
const char *
diag_reset_cause_get_name(diag_reset_cause_t reset_cause);
#endif

#endif //ORB_FIRMWARE_ORB_COMMON_DIAG_H
