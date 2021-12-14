#ifndef FRONT_UNIT_RGB_LEDS_H
#define FRONT_UNIT_RGB_LEDS_H

#include "mcu_messaging.pb.h"

int
front_unit_rgb_leds_init(void);

void
front_unit_rgb_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern);

void
front_unit_rgb_leds_set_brightness(uint32_t brightness); // 0 - 255

#endif // FRONT_UNIT_RGB_LEDS_H
