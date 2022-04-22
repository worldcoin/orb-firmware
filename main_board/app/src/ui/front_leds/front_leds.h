#ifndef FRONT_UNIT_RGB_LEDS_H
#define FRONT_UNIT_RGB_LEDS_H

#include "mcu_messaging.pb.h"
#include <errors.h>

ret_code_t
front_leds_init(void);

void
front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern,
                       uint32_t start_angle, int32_t angle_length,
                       RgbColor *color);

void
front_leds_set_brightness(uint32_t brightness); // 0 - 255

#endif // FRONT_UNIT_RGB_LEDS_H
