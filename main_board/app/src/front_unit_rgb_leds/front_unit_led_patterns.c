#include <drivers/led_strip.h>
#include <random/rand32.h>
#include <string.h>
#include <zephyr.h>

#define NUM_CENTER_LEDS 9

static struct led_rgb leds[233];

void
front_unit_rgb_led_off(const struct device *led_strip, struct k_sem *sem)
{
    memset(leds, 0, sizeof leds);
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}

void
front_unit_rgb_led_random_loop(const struct device *led_strip,
                               uint8_t intensity, struct k_sem *sem)
{
    for (;;) {
        if (intensity > 0) {
            for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
                leds[i].r = sys_rand32_get() % intensity;
                leds[i].g = sys_rand32_get() % intensity;
                leds[i].b = sys_rand32_get() % intensity;
            }
            if (k_sem_take(sem, K_MSEC(50)) == 0) {
                return;
            }
            led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
        } else {
            front_unit_rgb_led_off(led_strip, sem);
            return;
        }
    }
}

void
front_unit_rgb_led_white(const struct device *led_strip, uint8_t intensity,
                         struct k_sem *sem)
{
    memset(leds, intensity, sizeof leds);
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}

void
front_unit_rgb_led_white_no_center(const struct device *led_strip,
                                   uint8_t intensity, struct k_sem *sem)
{
    memset(leds, 0, (sizeof *leds) * NUM_CENTER_LEDS);
    memset(leds + NUM_CENTER_LEDS, intensity,
           sizeof leds - ((sizeof *leds) * NUM_CENTER_LEDS));
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}

void
front_unit_rgb_led_white_only_center(const struct device *led_strip,
                                     uint8_t intensity, struct k_sem *sem)
{
    memset(leds, intensity, (sizeof *leds) * NUM_CENTER_LEDS);
    memset(leds + NUM_CENTER_LEDS, 0,
           sizeof leds - ((sizeof *leds) * NUM_CENTER_LEDS));
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}

void
front_unit_rgb_led_red(const struct device *led_strip, uint8_t intensity,
                       struct k_sem *sem)
{
    for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
        leds[i].r = intensity;
        leds[i].g = 0;
        leds[i].b = 0;
    }
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}

void
front_unit_rgb_led_green(const struct device *led_strip, uint8_t intensity,
                         struct k_sem *sem)
{
    for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
        leds[i].r = 0;
        leds[i].g = intensity;
        leds[i].b = 0;
    }
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}

void
front_unit_rgb_led_blue(const struct device *led_strip, uint8_t intensity,
                        struct k_sem *sem)
{
    for (int i = 0; i < ARRAY_SIZE(leds); ++i) {
        leds[i].r = 0;
        leds[i].g = 0;
        leds[i].b = intensity;
    }
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    k_sem_take(sem, K_FOREVER);
}
