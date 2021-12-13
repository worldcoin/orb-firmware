#ifndef FAN_H
#define FAN_H

#include <stdint.h>

void
fan_set_speed(uint32_t percentage);

int
fan_init(void);

#endif // FAN_H
