#include "front_leds.h"
#include "ui/rgb_leds.h"
#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/led_strip.h>
#include <logging/log.h>
#include <random/rand32.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(front_unit_rgb_leds);

static K_THREAD_STACK_DEFINE(front_leds_stack_area,
                             THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS);
static struct k_thread front_led_thread_data;
static K_SEM_DEFINE(sem, 1, 1); // init to 1 to use default values below

#define NUM_LEDS         DT_PROP(DT_NODELABEL(front_unit_rgb_leds), num_leds)
#define NUM_CENTER_LEDS  9
#define SHADES_PER_COLOR 4 // 4^3 = 64 different colors
static struct led_rgb leds[NUM_LEDS];

// default values
static volatile UserLEDsPattern_UserRgbLedPattern global_pattern =
    UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE;
static volatile uint8_t global_intensity = 30;

#define PULSING_PERIOD_MS 2000
#define PULSING_DELAY_TIME_US                                                  \
    (PULSING_PERIOD_MS / PULSING_NUM_UPDATES_PER_PERIOD)
// We use a minimum intensity because the apparent brightness of the lower 10
// brightness settings is very noticeable. Above 10 it _seems_ like the pulsing
// is much smoother and organic.
#define PULSING_MIN_INTENSITY 10
extern const float SINE_LUT[PULSING_NUM_UPDATES_PER_PERIOD];

static_assert(PULSING_DELAY_TIME_US > 0, "pulsing delay time is too low");

// NOTE:
// All delays here are a bit skewed since it takes ~7ms to transmit the LED
// settings. So it takes 7ms + delay_time to between animations.

_Noreturn static void
front_leds_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct device *led_strip = a;
    k_timeout_t wait_until = K_FOREVER;
    uint32_t pulsing_index = 0;

    for (;;) {
        // wait for next command
        k_sem_take(&sem, wait_until);

        wait_until = K_FOREVER;

        if (global_pattern != UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE) {
            pulsing_index = 0;
        }

        switch (global_pattern) {
        case UserLEDsPattern_UserRgbLedPattern_OFF:
            RGB_LEDS_OFF(leds);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE:
            RGB_LEDS_WHITE(leds, global_intensity);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_NO_CENTER:
            memset(leds, 0, (sizeof *leds) * NUM_CENTER_LEDS);
            memset(leds + NUM_CENTER_LEDS, global_intensity,
                   sizeof leds - ((sizeof *leds) * NUM_CENTER_LEDS));
            break;
        case UserLEDsPattern_UserRgbLedPattern_RANDOM_RAINBOW:
            if (global_intensity > 0) {
                uint32_t shades = global_intensity > SHADES_PER_COLOR
                                      ? SHADES_PER_COLOR
                                      : global_intensity;
                for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
                    leds[i].r =
                        sys_rand32_get() % shades * (global_intensity / shades);
                    leds[i].g =
                        sys_rand32_get() % shades * (global_intensity / shades);
                    leds[i].b =
                        sys_rand32_get() % shades * (global_intensity / shades);
                }
                wait_until = K_MSEC(50);
            } else {
                memset(leds, 0, sizeof leds);
            }
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_ONLY_CENTER:
            memset(leds, global_intensity, (sizeof *leds) * NUM_CENTER_LEDS);
            memset(leds + NUM_CENTER_LEDS, 0,
                   sizeof leds - ((sizeof *leds) * NUM_CENTER_LEDS));
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_RED:
            RGB_LEDS_RED(leds, global_intensity);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_GREEN:
            RGB_LEDS_GREEN(leds, global_intensity);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_BLUE:
            RGB_LEDS_BLUE(leds, global_intensity);
            break;
        case UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE:
            if (global_intensity > 0) {
                for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
                    uint8_t v = (SINE_LUT[pulsing_index] * global_intensity) +
                                PULSING_MIN_INTENSITY;
                    leds[i].r = v;
                    leds[i].g = v;
                    leds[i].b = v;
                }
                wait_until = K_MSEC(PULSING_DELAY_TIME_US);
                pulsing_index = (pulsing_index + 1) % ARRAY_SIZE(SINE_LUT);
            } else {
                pulsing_index = 0;
                memset(leds, 0, sizeof leds);
            }
            break;
        default:
            LOG_ERR("Unhandled front LED pattern: %u", global_pattern);
            continue;
        }

        // update LEDs
        led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    }
}

void
front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern)
{
    global_pattern = pattern;
    k_sem_give(&sem);
}

void
front_leds_set_brightness(uint32_t brightness)
{
    global_intensity = MIN(255, brightness);
    k_sem_give(&sem);
}

ret_code_t
front_leds_init(void)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid =
        k_thread_create(&front_led_thread_data, front_leds_stack_area,
                        K_THREAD_STACK_SIZEOF(front_leds_stack_area),
                        front_leds_thread, (void *)led_strip, NULL, NULL,
                        THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "User RGB LED");

    return RET_SUCCESS;
}
