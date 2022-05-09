#ifndef FAN_H
#define FAN_H

#include <errors.h>
#include <stdint.h>

void
fan_set_speed(uint32_t percentage);

// Get speed as a percentage
uint8_t
fan_get_speed(void);

#endif // FAN_H
