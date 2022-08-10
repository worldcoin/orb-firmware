#ifndef FRONT_UNIT_RGB_LEDS_H
#define FRONT_UNIT_RGB_LEDS_H

#include "mcu_messaging.pb.h"
#include <errors.h>

#define FULL_RING_DEGREES (360)

ret_code_t
front_leds_init(void);

ret_code_t
front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern,
                       uint32_t start_angle, int32_t angle_length,
                       RgbColor *color, uint32_t pulsing_period_ms,
                       float pulsing_scale);

void
front_leds_set_brightness(uint32_t brightness); // 0 - 255

#endif // FRONT_UNIT_RGB_LEDS_H
