#ifndef FAN_H
#define FAN_H

#include <errors.h>
#include <stdint.h>

void
fan_set_max_speed(void);

void
fan_set_speed_by_percentage(uint32_t percentage);

// 0..65535 mapped into fan speed settings from [0 .. max]
void
fan_set_speed_by_value(uint16_t value);

// Get speed as duty cycle in ns
uint32_t
fan_get_speed_setting(void);

#endif // FAN_H
