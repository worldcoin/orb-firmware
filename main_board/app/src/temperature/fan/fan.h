#ifndef FAN_H
#define FAN_H

#include <errors.h>
#include <stdint.h>

#define FAN_INITIAL_SPEED_PERCENT 1

void
fan_set_max_speed(void);

void
fan_set_speed_by_percentage(uint32_t percentage);

// 0..65535 mapped into fan speed settings from [0 .. max]
void
fan_set_speed_by_value(uint16_t value);

// Get speed as raw value from 0 to UINT16_MAX
uint32_t
fan_get_speed_setting(void);

int
fan_init(void);

#endif // FAN_H
