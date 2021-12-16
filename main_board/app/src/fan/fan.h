#ifndef FAN_H
#define FAN_H

#include <errors.h>
#include <stdint.h>

void
fan_set_speed(uint32_t percentage);

ret_code_t
fan_init(void);

#endif // FAN_H
