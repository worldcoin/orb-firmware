#pragma once

#include "stdint.h"

/**
 * @brief Power supply the super capacitors
 * @return
 */
int
boot_turn_on_super_cap_charger(void);

/**
 * @brief Enable PVCC supply
 *
 * @retval RET_ERROR_INVALID_STATE if PVCC regulator isn't ready
 * @retval RET_SUCCESS if PVCC regulator is successfully enabled
 * @retval RET_ERROR_INTERNAL if there is an internal error enabling the
 * regulator
 */
int
boot_turn_on_pvcc(void);

/**
 * @brief Turn on the Jetson by initiating the power sequence
 *
 * @retval RET_ERROR_INVALID_STATE if any of the power supplies is not ready
 * @retval RET_SUCCESS if Jetson is successfully powered on
 */
int
boot_turn_on_jetson(void);

/**
 * @brief Reboot the system, which likely lead to the Orb being turned off
 *
 * @details This function unblocks a low-priority thread to reboot
 *          after the specified delay.
 * @param delay_s Delay in seconds before rebooting
 * @retval RET_SUCCESS if reboot is scheduled
 * @retval RET_ERROR_INVALID_STATE if already in progress
 */
int
reboot(uint32_t delay_s);
