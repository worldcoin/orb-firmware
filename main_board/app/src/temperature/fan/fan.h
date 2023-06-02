#pragma once

#include <errors.h>
#include <stdint.h>

#define FAN_INITIAL_SPEED_PERCENT 1

/**
 * Set fan speed to maximum speed (100% of its capability)
 *
 * @note The capability depends on the Orb version.
 */
void
fan_set_max_speed(void);

/**
 * Set fan speed to a percentage of its capability
 *
 * @note The capability depends on the Orb version.
 */
void
fan_set_speed_by_percentage(uint32_t percentage);

//

/**
 * Set fan speed to a raw value from 0 to UINT16_MAX
 * (0 to 65535) mapped into fan speed settings from [0 .. max capability]
 */
void
fan_set_speed_by_value(uint16_t value);

/**
 * Get fan speed as raw value
 *
 * @return fan speed as raw value from 0 to UINT16_MAX
 */
uint16_t
fan_get_speed_setting(void);

/**
 * Initialize fan
 *
 * @retval 0 on success
 * @retval RET_ERROR_INTERNAL if fan cannot be initialized (e.g. PWM controller
 * and/or GPIO device is not ready)
 */
int
fan_init(void);
