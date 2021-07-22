//
// Created by Cyril on 22/07/2021.
//

#include <board.h>
#include "diag.h"

/// @brief      Obtain the STM32 system reset cause
/// A Power-On Reset (POR), Power-Down Reset (PDR), OR Brownout Reset (BOR) will set \c RCC_FLAG_BORRST
/// So the Brownout Reset flag, `RCC_FLAG_BORRST`, is checked *after* first checking the
/// `RCC_FLAG_PORRST` flag in order to ensure first that the reset cause is NOT a POR/PDR reset.
/// @param      None
/// @return     The system reset cause
diag_reset_cause_t
reset_cause_get(void)
{
    diag_reset_cause_t reset_cause;

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))
    {
        reset_cause = RESET_CAUSE_LOW_POWER_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
    {
        reset_cause = RESET_CAUSE_WINDOW_WATCHDOG_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        reset_cause = RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
    {
        reset_cause =
            RESET_CAUSE_SOFTWARE_RESET; // This reset is induced by calling the ARM CMSIS `NVIC_SystemReset()` function!
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
    {
        reset_cause = RESET_CAUSE_POWER_ON_POWER_DOWN_RESET;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
    {
        reset_cause = RESET_CAUSE_EXTERNAL_RESET_PIN_RESET;
    }
//         Needs to come *after* checking the `RCC_FLAG_PORRST` flag in order to ensure first that the reset cause is
//         NOT a POR/PDR reset. See note below.
//    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))
//    {
//        reset_cause = RESET_CAUSE_BROWNOUT_RESET;
//    }
    else
    {
        reset_cause = RESET_CAUSE_UNKNOWN;
    }

    // Clear all the reset flags or else they will remain set during future resets until system power is fully removed.
    __HAL_RCC_CLEAR_RESET_FLAGS();

    return reset_cause;
}

#ifdef DEBUG

/// @brief      Obtain the system reset cause as an ASCII-printable name string from a reset cause type
/// @param[in]  reset_cause     The previously-obtained system reset cause
/// @return     A null-terminated ASCII name string describing the system reset cause
const char *
diag_reset_cause_get_name(diag_reset_cause_t reset_cause)
{
    const char *reset_cause_name = "TBD";

    switch (reset_cause)
    {
        case RESET_CAUSE_UNKNOWN:
        {
            reset_cause_name = "UNKNOWN";
        }
            break;
        case RESET_CAUSE_LOW_POWER_RESET:
        {
            reset_cause_name = "LOW_POWER_RESET";
        }
            break;
        case RESET_CAUSE_WINDOW_WATCHDOG_RESET:
        {
            reset_cause_name = "WINDOW_WATCHDOG_RESET";
        }
            break;
        case RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET:
        {
            reset_cause_name = "INDEPENDENT_WATCHDOG_RESET";
        }
            break;
        case RESET_CAUSE_SOFTWARE_RESET:
        {
            reset_cause_name = "SOFTWARE_RESET";
        }
            break;
        case RESET_CAUSE_POWER_ON_POWER_DOWN_RESET:
        {
            reset_cause_name = "POWER-ON_RESET (POR) / POWER-DOWN_RESET (PDR)";
        }
            break;
        case RESET_CAUSE_EXTERNAL_RESET_PIN_RESET:
        {
            reset_cause_name = "EXTERNAL_RESET_PIN_RESET";
        }
            break;
        case RESET_CAUSE_BROWNOUT_RESET:
        {
            reset_cause_name = "BROWNOUT_RESET (BOR)";
        }
            break;
    }

    return reset_cause_name;
}

#endif