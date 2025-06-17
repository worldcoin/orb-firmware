#pragma once

#include <errors.h>
#include <stdint.h>

/**
 * Initialize the fan tach subsystem
 * @retval RET_SUCCESS if successful
 * @retval RET_ERROR_INTERNAL if failed to configure pins and timers
 */
ret_code_t
fan_tach_init(void);

/**
 * Read the actual speed of the main fan in RPM
 * @return speed in rotations per minute if successful, otherwise UINT32_MAX
 */
uint32_t
fan_tach_get_main_speed(void);

#ifdef CONFIG_BOARD_PEARL_MAIN
/**
 * Read the actual speed of the aux fan in RPM
 * @return speed in rotations per minute if successful, otherwise UINT32_MAX
 */
uint32_t
fan_tach_get_aux_speed(void);
#endif
