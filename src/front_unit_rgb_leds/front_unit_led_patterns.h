#ifndef FRONT_UNIT_LED_PATTERNS_H
#define FRONT_UNIT_LED_PATTERNS_H

// Intensity: 0 - 0xff to control brightness

void
front_unit_rgb_led_ramp_up_and_down_loop(const struct device *led_strip,
                                         uint8_t intensity);
void
front_unit_rgb_led_random_loop(const struct device *led_strip,
                               uint8_t intensity);
void
front_unit_rgb_led_white(const struct device *led_strip, uint8_t intensity);
void
front_unit_rgb_led_off(const struct device *led_strip);

#endif // FRONT_UNIT_LED_PATTERNS_H
