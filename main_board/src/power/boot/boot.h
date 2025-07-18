#pragma once

#include "errors.h"
#include "mcu.pb.h"
#include "stdint.h"

/**
 * @brief Power supply the super capacitors
 * @return RET_SUCCESS if super capacitors are successfully powered on, return
 * value of @c gpio_pin_set otherwise
 */
int
boot_turn_on_super_cap_charger(void);

/**
 * @brief Enable PVCC supply
 *
 * @retval RET_SUCCESS if PVCC regulator is successfully enabled
 * @retval RET_ERROR_INVALID_STATE if PVCC regulator isn't ready
 * @retval RET_ERROR_INTERNAL if there is an internal error enabling the
 * regulator
 */
int
boot_turn_on_pvcc(void);

/**
 * @brief Disable PVCC supply
 *
 * @retval RET_SUCCESS if PVCC regulator is successfully disabled
 * @retval RET_ERROR_INVALID_STATE if PVCC regulator isn't ready
 * @retval RET_ERROR_INTERNAL if there is an internal error disabling the
 * regulator
 */
int
boot_turn_off_pvcc(void);

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

/**
 * @brief Turn on vbat, 5v & 3v3 lines on the board
 *
 * @details This function turns on vbat, 5v & 3v3 supply on the board
 *     which powers most of the modules (Wifi/Bluetooth, GNSS, etc.)
 */
void
power_vbat_5v_3v3_supplies_on(void);

/**
 * @brief Turn off vbat, 5v & 3v3 lines on the board
 *
 * @details This function turns off vbat, 5v & 3v3 supply on the board
 *     which powers most of the modules (Wifi/Bluetooth, GNSS, etc.)
 *     and wait for 1 second to let the modules to be fully powered off / reset
 *     (such as the WiFi chip which takes a while to reset due to some caps)
 */
void
power_vbat_5v_3v3_supplies_off(void);

int
power_cycle_supply(const orb_mcu_main_PowerCycle_Line line,
                   const uint32_t duration_off_ms);
