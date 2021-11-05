#include <drivers/led_strip.h>
#include <random/rand32.h>
#include <string.h>
#include <zephyr.h>

static struct led_rgb leds[60];

void
front_unit_rgb_led_ramp_up_and_down_loop(const struct device *led_strip,
                                         uint8_t intensity)
{
    for (;;) {
        memset(leds, 0, sizeof leds);
        led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));

        for (int i = 4; i < ARRAY_SIZE(leds); ++i) {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;

            switch (i % 3) {
            case 0:
                leds[i].r = intensity;
                break;
            case 1:
                leds[i].g = intensity;
                break;
            case 2:
                leds[i].b = intensity;
                break;
            }
        }

        for (int i = 1; i <= ARRAY_SIZE(leds); ++i) {
            led_strip_update_rgb(led_strip, leds, i);
            k_msleep(10);
        }

        k_msleep(50);

        for (int i = 59; i >= 0; --i) {
            leds[i].r = 0;
            leds[i].g = 0;
            leds[i].b = 0;
            led_strip_update_rgb(led_strip, leds, i + 1);
            k_msleep(10);
        }
    }
}

void
front_unit_rgb_led_random_loop(const struct device *led_strip,
                               uint8_t intensity)
{
    for (;;) {
        for (int i = 4; i < ARRAY_SIZE(leds); ++i) {
            leds[i].r = sys_rand32_get() % intensity;
            leds[i].g = sys_rand32_get() % intensity;
            leds[i].b = sys_rand32_get() % intensity;
        }
        k_msleep(50);
        led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    }
}

void
front_unit_rgb_led_white(const struct device *led_strip, uint8_t intensity)
{
    memset(leds, intensity, sizeof leds);
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
}

void
front_unit_rgb_led_off(const struct device *led_strip)
{
    memset(leds, 0, sizeof leds);
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
}
