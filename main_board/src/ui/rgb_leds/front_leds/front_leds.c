#include "front_leds.h"
#include "app_config.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "system/version/version.h"
#include "orb_logs.h"
#include "ui/rgb_leds/rgb_leds.h"
#include <app_assert.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(front_unit_rgb_leds, CONFIG_FRONT_UNIT_RGB_LEDS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(front_leds_stack_area,
                             THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS);
static struct k_thread front_led_thread_data;
static K_SEM_DEFINE(sem, 1, 1); // init to 1 to use default values below

static const struct device *const led_strip_w =
    DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds_w));

static const struct device *led_strip = led_strip_w;

#define NUM_LEDS DT_PROP(DT_NODELABEL(front_unit_rgb_leds_w), num_leds)
#if defined(CONFIG_BOARD_PEARL_MAIN)
#define NUM_CENTER_LEDS 9
#define INDEX_RING_ZERO                                                        \
    ((NUM_RING_LEDS * 3 / 4)) // 0º is at the 3 o'clock position
// Maximum amount of time for LED strip update
// It's also the minimum amount of time we need to trigger
// an LED strip update until the next IR LED pulse
#define LED_STRIP_MAXIMUM_UPDATE_TIME_US 10000
#else

static const struct device *const led_strip_apa =
    DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds_apa));

#define NUM_CENTER_LEDS 64
// 0º (3 o'clock position) is at 53rd LED on Front Unit 6.2
#define INDEX_RING_ZERO 53
#endif
#define NUM_RING_LEDS (NUM_LEDS - NUM_CENTER_LEDS)

// for rainbow pattern
#define SHADES_PER_COLOR 4 // 4^3 = 64 different colors

struct center_ring_leds {
#if defined(CONFIG_BOARD_PEARL_MAIN)
    struct led_rgb center_leds[NUM_CENTER_LEDS];
    struct led_rgb ring_leds[NUM_RING_LEDS];
#else
    // on Diamond, the ring LEDs are the first LEDs
    // connected to the LED strip
    struct led_rgb ring_leds[NUM_RING_LEDS];
    struct led_rgb center_leds[NUM_CENTER_LEDS];
#endif
};

typedef union {
    struct center_ring_leds part;
    struct led_rgb all[NUM_LEDS];
} user_leds_t;
static user_leds_t leds;

K_MUTEX_DEFINE(leds_update_mutex);
K_SEM_DEFINE(leds_wait_for_trigger, 0, 1);
static volatile bool final_done = false;

// default values

static volatile UserLEDsPattern_UserRgbLedPattern global_pattern =
    UserLEDsPattern_UserRgbLedPattern_OFF;

static volatile bool use_sequence;
static volatile uint32_t global_start_angle_degrees = 0;
static volatile int32_t global_angle_length_degrees = FULL_RING_DEGREES;
static volatile uint8_t global_intensity = 25;
static volatile struct led_rgb global_color = RGB_WHITE;
static volatile float global_pulsing_scale = PULSING_SCALE_DEFAULT;
static volatile uint32_t global_pulsing_period_ms = INITIAL_PULSING_PERIOD_MS;
static volatile uint32_t global_pulsing_delay_time_ms =
    INITIAL_PULSING_PERIOD_MS / SINE_TABLE_LENGTH;

/// half period <=> from 0 to pi rad <=> 0 to 1 to 0
extern const float SINE_LUT[SINE_TABLE_LENGTH];

#if defined(CONFIG_BOARD_PEARL_MAIN)
static volatile bool wait_for_interrupt = false;

void
front_leds_notify_ir_leds_off(void)
{
    if (wait_for_interrupt) {
        k_sem_give(&leds_wait_for_trigger);
        wait_for_interrupt = false;
    }
}
#endif

// NOTE:
// All delays here are a bit skewed since it takes ~7ms to transmit the LED
// settings. So it takes 7ms + delay_time to between animations.

static void
set_center(struct led_rgb color)
{
    CRITICAL_SECTION_ENTER(k);
    for (size_t i = 0; i < ARRAY_SIZE(leds.part.center_leds); ++i) {
        leds.part.center_leds[i] = color;
    }
    CRITICAL_SECTION_EXIT(k);
}

K_MUTEX_DEFINE(ring_write);

static void
set_ring(struct led_rgb color, uint32_t start_angle, int32_t angle_length)
{
    ASSERT_HARD_BOOL(start_angle <= FULL_RING_DEGREES);
    ASSERT_HARD_BOOL(angle_length <= FULL_RING_DEGREES &&
                     angle_length >= -FULL_RING_DEGREES);

    // get first LED index, based on LED at 0º on trigonometric circle
    size_t led_index =
        INDEX_RING_ZERO - NUM_RING_LEDS * start_angle / FULL_RING_DEGREES;

    k_mutex_lock(&ring_write, K_FOREVER);
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
    k_mutex_unlock(&ring_write);
}

_Noreturn static void
front_leds_thread()
{

    uint8_t intensity;
    struct led_rgb color;
    UserLEDsPattern_UserRgbLedPattern pattern;
    uint32_t start_angle_degrees;
    int32_t angle_length_degrees;
    float pulsing_scale;
    uint32_t pulsing_period_ms;
    float scaler;
    k_timeout_t wait_until = K_FOREVER;
    uint32_t pulsing_index = ARRAY_SIZE(SINE_LUT);

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

        if (!use_sequence) {
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
                    uint32_t shades = intensity > SHADES_PER_COLOR
                                          ? SHADES_PER_COLOR
                                          : intensity;
                    for (size_t i = 0; i < ARRAY_SIZE(leds.all); ++i) {
                        leds.all[i].r = rand() % shades * (intensity / shades);
                        leds.all[i].g = rand() % shades * (intensity / shades);
                        leds.all[i].b = rand() % shades * (intensity / shades);
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
                set_center(color);
                break;
            case UserLEDsPattern_UserRgbLedPattern_ALL_GREEN:
                color.r = 0;
                color.g = intensity;
                color.b = 0;
                set_ring(color, start_angle_degrees, angle_length_degrees);
                set_center(color);
                break;
            case UserLEDsPattern_UserRgbLedPattern_ALL_BLUE:
                color.r = 0;
                color.g = 0;
                color.b = intensity;
                set_ring(color, start_angle_degrees, angle_length_degrees);
                set_center(color);
                break;
            case UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE:;
                color.r = MINIMUM_WHITE_BRIGHTNESS;
                color.g = MINIMUM_WHITE_BRIGHTNESS;
                color.b = MINIMUM_WHITE_BRIGHTNESS;
                pulsing_scale = PULSING_SCALE_DEFAULT;
                // fallthrough
            case UserLEDsPattern_UserRgbLedPattern_PULSING_RGB:;
                if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                    // from 0 to 1.0
                    scaler = SINE_LUT[pulsing_index] * pulsing_scale;
                } else {
                    // from 1.0 to 0
                    size_t index = (ARRAY_SIZE(SINE_LUT) - 1 -
                                    (pulsing_index - ARRAY_SIZE(SINE_LUT)));
                    scaler = SINE_LUT[index] * pulsing_scale;
                }
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
                color.scratch = roundf(scaler * color.scratch);
#else
                color.r = roundf(scaler * color.r);
                color.g = roundf(scaler * color.g);
                color.b = roundf(scaler * color.b);
#endif

                wait_until = K_MSEC(global_pulsing_delay_time_ms);
                set_ring(color, start_angle_degrees, angle_length_degrees);
                set_center((struct led_rgb)RGB_OFF);
                break;
            case UserLEDsPattern_UserRgbLedPattern_PULSING_RGB_ONLY_CENTER:
                if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                    // from 0 to 1.0
                    scaler = SINE_LUT[pulsing_index] * PULSING_SCALE_DEFAULT;
                } else {
                    // from 1.0 to 0
                    size_t index = (ARRAY_SIZE(SINE_LUT) - 1 -
                                    (pulsing_index - ARRAY_SIZE(SINE_LUT)));
                    scaler = SINE_LUT[index] * PULSING_SCALE_DEFAULT;
                }
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
                color.scratch = roundf(scaler * color.scratch);
#else
                color.r = roundf(scaler * color.r);
                color.g = roundf(scaler * color.g);
                color.b = roundf(scaler * color.b);
#endif

                wait_until = K_MSEC(global_pulsing_delay_time_ms);
                set_center(color);
                set_ring((struct led_rgb)RGB_OFF, 0, FULL_RING_DEGREES);
                break;
            case UserLEDsPattern_UserRgbLedPattern_RGB:
                set_ring(color, start_angle_degrees, angle_length_degrees);
                set_center((struct led_rgb)RGB_OFF);
                break;
            case UserLEDsPattern_UserRgbLedPattern_RGB_ONLY_CENTER:
                set_center((struct led_rgb)color);
                set_ring((struct led_rgb)RGB_OFF, 0, FULL_RING_DEGREES);
                break;
            case UserLEDsPattern_UserRgbLedPattern_BOOT_ANIMATION:
                if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                    // from 0 to 1.0
                    scaler = SINE_LUT[pulsing_index] * PULSING_SCALE_DEFAULT;
                } else {
                    // from 1.0 to 0
                    size_t index = (ARRAY_SIZE(SINE_LUT) - 1 -
                                    (pulsing_index - ARRAY_SIZE(SINE_LUT)));
                    scaler = SINE_LUT[index] * PULSING_SCALE_DEFAULT;
                }
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
                color.scratch = roundf(scaler * color.scratch);
#else
                color.r = roundf(scaler * color.r);
                color.g = roundf(scaler * color.g);
                color.b = roundf(scaler * color.b);
#endif

                wait_until = K_MSEC(global_pulsing_delay_time_ms);
                set_center(color);
                set_ring((struct led_rgb)RGB_OFF, 0, FULL_RING_DEGREES);
                break;
            default:
                LOG_ERR("Unhandled front LED pattern: %u", global_pattern);
                continue;
            }
        }

        pulsing_index = (pulsing_index + 1) % (ARRAY_SIZE(SINE_LUT) * 2);

        // update LEDs
        k_mutex_lock(&leds_update_mutex, K_FOREVER);
        if (!final_done) {
#if defined(CONFIG_BOARD_PEARL_MAIN)
            // 850nm and 940nm IR leds mustn't be on when the RGB strip
            // is updated to prevent the LED from flickering.
            // If they are active and the next pulse is too close in time (we
            // don't have enough time to update all the RGB leds), wait for
            // finished IR pulse.
            wait_for_interrupt = (ir_camera_system_get_enabled_leds() >
                                  InfraredLEDs_Wavelength_WAVELENGTH_740NM) &&
                                 (ir_camera_system_get_time_until_update_us() <
                                  LED_STRIP_MAXIMUM_UPDATE_TIME_US) &&
                                 (ir_camera_system_get_fps() > 0);
            if (wait_for_interrupt) {
                k_sem_take(&leds_wait_for_trigger, K_FOREVER);
            }
#endif
            led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
        }
        k_mutex_unlock(&leds_update_mutex);
    }
}

static void
print_new_debug(UserLEDsPattern_UserRgbLedPattern pattern, uint32_t start_angle,
                int32_t angle_length, RgbColor *color,
                uint32_t pulsing_period_ms, float pulsing_scale)
{
    LOG_DBG("pattern = %d", pattern);
    LOG_DBG("start angle = %" PRIu32, start_angle);
    LOG_DBG("angle length = %" PRId32, angle_length);
    if (color) {
        LOG_DBG("color = #%02X%02X%02X", color->red, color->green, color->blue);
    } else {
        LOG_DBG("color = NULL");
    }
    LOG_DBG("pulsing period = %" PRIu32 "ms", pulsing_period_ms);
    LOG_DBG("pulsing scale = %g", pulsing_scale);
}

static ret_code_t
pulsing_rgb_check_range(RgbColor *color, float pulsing_scale)
{
    if ((roundf(color->red * (pulsing_scale + 1)) > 255) ||
        (roundf(color->green * (pulsing_scale + 1)) > 255) ||
        (roundf(color->blue * (pulsing_scale + 1)) > 255)) {
        LOG_ERR("Pulsing scale too large");
        return RET_ERROR_INVALID_PARAM;
    } else {
        return RET_SUCCESS;
    }
}

static bool
previous_settings_are_identical(UserLEDsPattern_UserRgbLedPattern pattern,
                                uint32_t start_angle, int32_t angle_length,
                                RgbColor *color, uint32_t pulsing_period_ms,
                                float pulsing_scale)
{
    bool ret = (global_pulsing_scale == pulsing_scale) &&
               (global_pulsing_period_ms == pulsing_period_ms) &&
               (global_pulsing_delay_time_ms ==
                global_pulsing_period_ms / ARRAY_SIZE(SINE_LUT)) &&
               (global_pattern == pattern) &&
               (global_start_angle_degrees == start_angle) &&
               (global_angle_length_degrees == angle_length);

    if (color != NULL) {
        return ret && (global_color.r == color->red) &&
               (global_color.g == color->green) &&
               (global_color.b == color->blue);
    } else {
        return ret;
    }
}

static void
update_parameters(UserLEDsPattern_UserRgbLedPattern pattern,
                  uint32_t start_angle, int32_t angle_length, RgbColor *color,
                  uint32_t pulsing_period_ms, float pulsing_scale)
{
    if (pulsing_period_ms == 0) {
        LOG_WRN("Pulsing period 0, setting to default");
        pulsing_period_ms = INITIAL_PULSING_PERIOD_MS;
    }

    if (pulsing_scale == 0) {
        LOG_WRN("Pulsing scale is 0, setting to default");
        pulsing_scale = PULSING_SCALE_DEFAULT;
    }

    CRITICAL_SECTION_ENTER(k);

    global_pulsing_scale = pulsing_scale;
    global_pulsing_period_ms = pulsing_period_ms;
    global_pulsing_delay_time_ms =
        global_pulsing_period_ms / ARRAY_SIZE(SINE_LUT);
    global_pattern = pattern;
    global_start_angle_degrees = start_angle;
    global_angle_length_degrees = angle_length;
    if (color != NULL) {
        global_color.r = color->red;
        global_color.g = color->green;
        global_color.b = color->blue;
    }
    use_sequence = false;

    CRITICAL_SECTION_EXIT(k);
}

ret_code_t
front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern pattern,
                       uint32_t start_angle, int32_t angle_length,
                       RgbColor *color, uint32_t pulsing_period_ms,
                       float pulsing_scale)
{
    if (pattern == UserLEDsPattern_UserRgbLedPattern_PULSING_RGB) {
        ret_code_t ret = pulsing_rgb_check_range(color, pulsing_scale);
        if (ret != RET_SUCCESS) {
            return RET_ERROR_INVALID_PARAM;
        }
    }

    if (!previous_settings_are_identical(pattern, start_angle, angle_length,
                                         color, pulsing_period_ms,
                                         pulsing_scale)) {
        print_new_debug(pattern, start_angle, angle_length, color,
                        pulsing_period_ms, pulsing_scale);
        update_parameters(pattern, start_angle, angle_length, color,
                          pulsing_period_ms, pulsing_scale);
        k_sem_give(&sem);
    }

    return RET_SUCCESS;
}

ret_code_t
front_leds_set_center_leds_sequence_argb32(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_ARGB, leds.part.center_leds,
        ARRAY_SIZE(leds.part.center_leds), (bool *)&use_sequence, &sem, NULL);
    ASSERT_SOFT(ret);
    return ret;
}

ret_code_t
front_leds_set_center_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_RGB, leds.part.center_leds,
        ARRAY_SIZE(leds.part.center_leds), (bool *)&use_sequence, &sem, NULL);
    ASSERT_SOFT(ret);
    return ret;
}

ret_code_t
front_leds_set_ring_leds_sequence_argb32(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_ARGB, leds.part.ring_leds,
        ARRAY_SIZE(leds.part.ring_leds), (bool *)&use_sequence, &sem,
        &ring_write);
    ASSERT_SOFT(ret);
    return ret;
}

ret_code_t
front_leds_set_ring_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_RGB, leds.part.ring_leds,
        ARRAY_SIZE(leds.part.ring_leds), (bool *)&use_sequence, &sem,
        &ring_write);
    ASSERT_SOFT(ret);
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

void
front_leds_turn_off_blocking(void)
{
    k_mutex_lock(&leds_update_mutex, K_FOREVER);
    final_done = true;
    memset(leds.all, 0, sizeof leds.all);
    led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
    led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
    k_mutex_unlock(&leds_update_mutex);
}

ret_code_t
front_leds_init(void)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    Hardware_FrontUnitVersion fu_version = version_get_front_unit_rev();
    if (fu_version == Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2B) {
        led_strip = led_strip_apa;
    }
#endif

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid = k_thread_create(
        &front_led_thread_data, front_leds_stack_area,
        K_THREAD_STACK_SIZEOF(front_leds_stack_area), front_leds_thread, NULL,
        NULL, NULL, THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "front_leds");

    return RET_SUCCESS;
}

/**
 * Turn off front LEDs during boot
 * @param dev
 * @return 0 on success, error code otherwise
 */
int
front_leds_initial_state(void)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    ASSERT_CONST_POINTER_NOT_NULL(led_strip_apa);
    ASSERT_CONST_POINTER_NOT_NULL(led_strip_w);

    Hardware_FrontUnitVersion fu_version = version_get_front_unit_rev();
    if (fu_version == Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2B) {
        led_strip = led_strip_apa;
    }
#endif

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
