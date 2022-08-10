#include "front_leds.h"
#include "ui/rgb_leds.h"
#include <app_assert.h>
#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/led_strip.h>
#include <logging/log.h>
#include <math.h>
#include <random/rand32.h>
#include <stdlib.h>
#include <utils.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(front_unit_rgb_leds);

static K_THREAD_STACK_DEFINE(front_leds_stack_area,
                             THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS);
static struct k_thread front_led_thread_data;
static K_SEM_DEFINE(sem, 1, 1); // init to 1 to use default values below

#define NUM_LEDS         DT_PROP(DT_NODELABEL(front_unit_rgb_leds), num_leds)
#define NUM_CENTER_LEDS  9
#define NUM_RING_LEDS    (NUM_LEDS - NUM_CENTER_LEDS)
#define SHADES_PER_COLOR 4 // 4^3 = 64 different colors
#define INDEX_RING_ZERO                                                        \
    ((NUM_RING_LEDS * 3 / 4)) // LED index at angle 0º in trigonometric circle

struct center_ring_leds {
    struct led_rgb center_leds[NUM_CENTER_LEDS];
    struct led_rgb ring_leds[NUM_RING_LEDS];
};

typedef union {
    struct center_ring_leds part;
    struct led_rgb all[NUM_LEDS];
} user_leds_t;
static user_leds_t leds;

// default values

static volatile UserLEDsPattern_UserRgbLedPattern global_pattern =
#ifdef CONFIG_NO_BOOT_LEDS
    UserLEDsPattern_UserRgbLedPattern_OFF;
#else
    UserLEDsPattern_UserRgbLedPattern_PULSING_RGB;
#endif // CONFIG_NO_BOOT_LEDS

#define INITIAL_PULSING_PERIOD_MS 2000

static volatile uint32_t global_start_angle_degrees = 0;
static volatile int32_t global_angle_length_degrees = FULL_RING_DEGREES;
static volatile uint8_t global_intensity = 30;
static volatile struct led_rgb global_color = {.r = 0, .g = 0, .b = 4};
static volatile float global_pulsing_scale = 2.25;
static volatile uint32_t global_pulsing_period_ms = INITIAL_PULSING_PERIOD_MS;
static volatile uint32_t global_pulsing_delay_time_ms =
    INITIAL_PULSING_PERIOD_MS / PULSING_NUM_UPDATES_PER_PERIOD;

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

static void
set_center(struct led_rgb color)
{
    for (size_t i = 0; i < ARRAY_SIZE(leds.part.center_leds); ++i) {
        leds.part.center_leds[i] = color;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
static void
set_ring(struct led_rgb color, uint32_t start_angle, int32_t angle_length)
{
    ASSERT_HARD_BOOL(start_angle <= FULL_RING_DEGREES);
    ASSERT_HARD_BOOL(angle_length <= FULL_RING_DEGREES &&
                     angle_length >= -FULL_RING_DEGREES);

    // get first LED index, based on LED at 0º on trigonometric circle
    size_t led_index =
        INDEX_RING_ZERO - NUM_RING_LEDS * start_angle / FULL_RING_DEGREES;

    for (size_t i = 0; i < NUM_RING_LEDS; ++i) {
        if (i <
            (size_t)(NUM_RING_LEDS * abs(angle_length) / FULL_RING_DEGREES)) {
            leds.part.ring_leds[led_index] = color;
        } else {
            leds.part.ring_leds[led_index] = (struct led_rgb)RGB_OFF;
        }

        // depending on angle_length sign, we go through the ring LED in a
        // different direction
        if (angle_length >= 0) {
            led_index = (led_index + 1) % ARRAY_SIZE(leds.part.ring_leds);
        } else {
            led_index = led_index == 0 ? (ARRAY_SIZE(leds.part.ring_leds) - 1)
                                       : (led_index - 1);
        }
    }
}
#pragma GCC diagnostic pop

_Noreturn static void
front_leds_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct device *led_strip = a;
    k_timeout_t wait_until = K_FOREVER;
    uint32_t pulsing_index = 0;

    uint8_t intensity;
    struct led_rgb color;
    UserLEDsPattern_UserRgbLedPattern pattern;
    uint32_t start_angle_degrees;
    int32_t angle_length_degrees;
    float pulsing_scale;
    uint32_t pulsing_period_ms;

    for (;;) {
        // wait for next command
        k_sem_take(&sem, wait_until);

        wait_until = K_FOREVER;

        CRITICAL_SECTION_ENTER(k);
        pattern = global_pattern;
        intensity = global_intensity;
        color = global_color;
        start_angle_degrees = global_start_angle_degrees;
        angle_length_degrees = global_angle_length_degrees;
        pulsing_scale = global_pulsing_scale;
        pulsing_period_ms = global_pulsing_period_ms;
        CRITICAL_SECTION_EXIT(k);

        if (pattern != UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE &&
            pattern != UserLEDsPattern_UserRgbLedPattern_PULSING_RGB) {
            pulsing_index = 0;
        }

        switch (pattern) {
        case UserLEDsPattern_UserRgbLedPattern_OFF:
            set_center((struct led_rgb)RGB_OFF);
            set_ring((struct led_rgb)RGB_OFF, 0, FULL_RING_DEGREES);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE:
            color.r = intensity;
            color.g = intensity;
            color.b = intensity;
            set_center(color);
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_NO_CENTER:
            color.r = intensity;
            color.g = intensity;
            color.b = intensity;
            set_center((struct led_rgb)RGB_OFF);
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        case UserLEDsPattern_UserRgbLedPattern_RANDOM_RAINBOW:
            if (intensity > 0) {
                uint32_t shades =
                    intensity > SHADES_PER_COLOR ? SHADES_PER_COLOR : intensity;
                for (size_t i = 0; i < ARRAY_SIZE(leds.all); ++i) {
                    leds.all[i].r =
                        sys_rand32_get() % shades * (intensity / shades);
                    leds.all[i].g =
                        sys_rand32_get() % shades * (intensity / shades);
                    leds.all[i].b =
                        sys_rand32_get() % shades * (intensity / shades);
                }
                wait_until = K_MSEC(50);
            } else {
                memset(leds.all, 0, sizeof leds.all);
            }
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_ONLY_CENTER:
            color.r = intensity;
            color.g = intensity;
            color.b = intensity;
            set_center(color);
            set_ring((struct led_rgb)RGB_OFF, 0, FULL_RING_DEGREES);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_RED:
            color.r = intensity;
            color.g = 0;
            color.b = 0;
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_GREEN:
            color.r = 0;
            color.g = intensity;
            color.b = 0;
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        case UserLEDsPattern_UserRgbLedPattern_ALL_BLUE:
            color.r = 0;
            color.g = 0;
            color.b = intensity;
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        case UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE:;
            color.r = 10;
            color.g = 10;
            color.b = 10;
            pulsing_scale = 2;
            // fallthrough
        case UserLEDsPattern_UserRgbLedPattern_PULSING_RGB:;
            float scaler = (SINE_LUT[pulsing_index] * pulsing_scale) + 1;
            color.r = roundf(scaler * color.r);
            color.g = roundf(scaler * color.g);
            color.b = roundf(scaler * color.b);

            wait_until = K_MSEC(global_pulsing_delay_time_ms);
            pulsing_index = (pulsing_index + 1) % ARRAY_SIZE(SINE_LUT);
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        case UserLEDsPattern_UserRgbLedPattern_RGB:
            set_ring(color, start_angle_degrees, angle_length_degrees);
            break;
        default:
            LOG_ERR("Unhandled front LED pattern: %u", global_pattern);
            continue;
        }

        // update LEDs
        led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
    }
}

ret_code_t
front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern,
                       uint32_t start_angle, int32_t angle_length,
                       RgbColor *color, uint32_t pulsing_period_ms,
                       float pulsing_scale)
{
    CRITICAL_SECTION_ENTER(k);

    if ((roundf(color->red * (pulsing_scale + 1)) > 255) ||
        (roundf(color->green * (pulsing_scale + 1)) > 255) ||
        (roundf(color->blue * (pulsing_scale + 1)) > 255)) {
        LOG_ERR("pulsing scale too large");
        return RET_ERROR_INVALID_PARAM;
    }

    global_pattern = pattern;
    global_start_angle_degrees = start_angle;
    global_angle_length_degrees = angle_length;
    global_pulsing_scale = pulsing_scale;
    global_pulsing_period_ms = pulsing_period_ms;
    global_pulsing_delay_time_ms =
        global_pulsing_period_ms / PULSING_NUM_UPDATES_PER_PERIOD;
    if (color != NULL) {
        global_color.r = color->red;
        global_color.g = color->green;
        global_color.b = color->blue;
    }

    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem);
    return RET_SUCCESS;
}

ret_code_t
front_leds_set_center_leds_sequence(uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = RET_SUCCESS;

    if (size % 3 != 0) {
        LOG_ERR("Bytes must be a multiple of 3");
        ret = RET_ERROR_INVALID_PARAM;
        ASSERT_SOFT(ret);
        return ret;
    }

    size = MIN(size, ARRAY_SIZE(leds.part.center_leds) * 3);

    CRITICAL_SECTION_ENTER(k);
    for (size_t i = 0; i < (size / 3); ++i) {
        leds.part.center_leds[i].r = bytes[i * 3];
        leds.part.center_leds[i].g = bytes[i * 3 + 1];
        leds.part.center_leds[i].b = bytes[i * 3 + 2];
    }
    for (size_t i = size / 3; i < ARRAY_SIZE(leds.part.center_leds); ++i) {
        leds.part.center_leds[i] = (struct led_rgb)RGB_OFF;
    }
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem);
    return ret;
}

void
front_leds_set_brightness(uint32_t brightness)
{
    CRITICAL_SECTION_ENTER(k);
    global_intensity = MIN(255, brightness);
    CRITICAL_SECTION_EXIT(k);

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
    k_thread_name_set(tid, "front_leds");

    return RET_SUCCESS;
}

/// Turn off front LEDs during boot
/// \param dev
/// \return 0 on success, error code otherwise
int
front_leds_initial_state(const struct device *dev)
{
    ARG_UNUSED(dev);

    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    set_center((struct led_rgb)RGB_OFF);
    set_ring((struct led_rgb)RGB_OFF, 0, FULL_RING_DEGREES);
    led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));

    return 0;
}

SYS_INIT(front_leds_initial_state, POST_KERNEL, SYS_INIT_UI_LEDS_PRIORITY);
