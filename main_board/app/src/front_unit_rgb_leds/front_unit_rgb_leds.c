#include "front_unit_rgb_leds.h"
#include "front_unit_led_patterns.h"
#include <app_config.h>
#include <device.h>
#include <drivers/led_strip.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(front_unit_rgb_leds);

static K_THREAD_STACK_DEFINE(user_rgb_leds_stack_area,
                             THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS);
static struct k_thread thread_data;

static volatile UserLEDsPattern_UserRgbLedPattern global_pattern =
    UserLEDsPattern_UserRgbLedPattern_RANDOM_RAINBOW;
static volatile uint8_t global_intensity = 20;

static K_SEM_DEFINE(sem, 0, 1);

_Noreturn static void
user_rgb_leds_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct device *led_strip = a;

    for (;;) {
        switch (global_pattern) {
        case UserLEDsPattern_UserRgbLedPattern_OFF:
            front_unit_rgb_led_off(led_strip, &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE:
            front_unit_rgb_led_white(led_strip, global_intensity, &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_NO_CENTER:
            front_unit_rgb_led_white_no_center(led_strip, global_intensity,
                                               &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_RANDOM_RAINBOW:
            front_unit_rgb_led_random_loop(led_strip, global_intensity, &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_ONLY_CENTER:
            front_unit_rgb_led_white_only_center(led_strip, global_intensity,
                                                 &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_RED:
            front_unit_rgb_led_red(led_strip, global_intensity, &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_GREEN:
            front_unit_rgb_led_green(led_strip, global_intensity, &sem);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_BLUE:
            front_unit_rgb_led_blue(led_strip, global_intensity, &sem);
            break;
        }
    }
}

void
front_unit_rgb_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern)
{
    global_pattern = pattern;
    k_sem_give(&sem);
}

void
front_unit_rgb_leds_set_brightness(uint32_t brightness)
{
    global_intensity = MIN(255, brightness);
    k_sem_give(&sem);
}

ret_code_t
front_unit_rgb_leds_init(void)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid =
        k_thread_create(&thread_data, user_rgb_leds_stack_area,
                        K_THREAD_STACK_SIZEOF(user_rgb_leds_stack_area),
                        user_rgb_leds_thread, (void *)led_strip, NULL, NULL,
                        THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS, 0, K_NO_WAIT);

    k_thread_name_set(tid, "User RGB LED");

    return RET_SUCCESS;
}
