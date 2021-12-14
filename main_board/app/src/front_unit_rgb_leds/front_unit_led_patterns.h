#ifndef FRONT_UNIT_LED_PATTERNS_H
#define FRONT_UNIT_LED_PATTERNS_H

#include <zephyr.h>

// Intensity: 0 - 0xff to control brightness

void
front_unit_rgb_led_random_loop(const struct device *led_strip,
                               uint8_t intensity, struct k_sem *sem);
void
front_unit_rgb_led_white(const struct device *led_strip, uint8_t intensity,
                         struct k_sem *sem);
void
front_unit_rgb_led_off(const struct device *led_strip, struct k_sem *sem);
void
front_unit_rgb_led_white_no_center(const struct device *led_strip,
                                   uint8_t intensity, struct k_sem *sem);
void
front_unit_rgb_led_white_only_center(const struct device *led_strip,
                                     uint8_t intensity, struct k_sem *sem);
void
front_unit_rgb_led_red(const struct device *led_strip, uint8_t intensity,
                       struct k_sem *sem);
void
front_unit_rgb_led_green(const struct device *led_strip, uint8_t intensity,
                         struct k_sem *sem);
void
front_unit_rgb_led_blue(const struct device *led_strip, uint8_t intensity,
                        struct k_sem *sem);

#endif // FRONT_UNIT_LED_PATTERNS_H
