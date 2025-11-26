#include "front_leds.h"
#include "app_config.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "orb_logs.h"
#include "orb_state.h"
#include "system/version/version.h"
#include "ui/rgb_leds/rgb_leds.h"
#include "zephyr/drivers/gpio.h"
#include <app_assert.h>
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
static K_SEM_DEFINE(sem_leds_refresh, 1,
                    1); // init to 1 to use default values below

// NOLINTNEXTLINE(cppcoreguidelines-interfaces-global-init)
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
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)

ORB_STATE_REGISTER(front_leds);

// NOLINTNEXTLINE(cppcoreguidelines-interfaces-global-init)
static const struct device *const led_strip_apa =
    DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds_apa));

#define NUM_CENTER_LEDS 64
// 0º (3 o'clock position) is at 41st LED on Front Unit 6.3+
#define INDEX_RING_ZERO 41
#endif
#define NUM_RING_LEDS (NUM_LEDS - NUM_CENTER_LEDS)

// for rainbow pattern
#define SHADES_PER_COLOR 4 // 4^3 = 64 different colors

struct center_ring_leds {
#if defined(CONFIG_BOARD_PEARL_MAIN)
    struct led_rgb center_leds[NUM_CENTER_LEDS];
    struct led_rgb ring_leds[NUM_RING_LEDS];
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    // on Diamond, the ring LEDs are the first LEDs
    // connected to the LED strip
    struct led_rgb ring_leds[NUM_RING_LEDS];
    struct led_rgb center_leds[NUM_CENTER_LEDS];
#endif
};

/**
 * The LED buffer is modified by the led strip driver. If the modified data is
 * reused by the led driver, the color is going to be incorrect. Both the ring
 * and center led buffers must be overwritten/updated with a new sequence before
 * reusing the led buffers with the led strip driver.
 * A flag is used to keep track of the dirty state of the led buffers.
 */
enum dirty_flags {
    RING_LEDS_DIRTY = 0x1,
    CENTER_LEDS_DIRTY = 0x2,
    ALL_LEDS_DIRTY = 0x3,
};
static atomic_t leds_dirty;

typedef union {
    struct center_ring_leds part;
    struct led_rgb all[NUM_LEDS];
} user_leds_t;
static user_leds_t leds;

/// Mutex used to protect the LEDs array update from multiple threads
static K_SEM_DEFINE(leds_update_sem, 1, 1);
#if defined(CONFIG_BOARD_PEARL_MAIN)
static K_SEM_DEFINE(leds_wait_for_trigger, 0, 1);
#endif
static volatile bool final_done = false;

// default values
static volatile orb_mcu_main_UserLEDsPattern_UserRgbLedPattern global_pattern =
#ifdef CONFIG_BOARD_DIAMOND_MAIN
    orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_BOOT_ANIMATION;
static volatile enum boot_progress_step_e boot_progress_current =
    BOOT_PROGRESS_STEP_UNKNOWN;
static volatile enum boot_progress_step_e boot_progress_target =
    BOOT_PROGRESS_STEP_UNKNOWN;
#else
    orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_OFF;
#endif

static uint32_t pulsing_index = 0;

// percentage doesn't go to 100% because of light leakage
// so that the last pulsating segment is still clearly visible
#define BOOT_PROGRESS_PERCENT_FULL 95
#define BOOT_PROGRESS_PERCENTAGE_STEP                                          \
    (BOOT_PROGRESS_PERCENT_FULL / (BOOT_PROGRESS_SENTINEL))
#define BOOT_ANIMATION_STEP_ANGLE                                              \
    (360.0f * ((float)BOOT_PROGRESS_PERCENTAGE_STEP / 100.0f))

#define BOOT_ANIMATION_BRIGHTNESS_CUTOFF    0.2f
#define BOOT_ANIMATION_TRANSITION_THRESHOLD 0.06f // diff from max scaler value

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
    if (k_sem_take(&leds_update_sem, K_NO_WAIT) == 0) {
        for (size_t i = 0; i < ARRAY_SIZE(leds.part.center_leds); ++i) {
            leds.part.center_leds[i] = color;
        }
        k_sem_give(&leds_update_sem);
    }
}

/**
 * Optional background color `bg_color` can be provided
 */
static void
set_ring(const struct led_rgb color, const struct led_rgb *bg_color,
         const uint32_t start_angle, const int32_t angle_length)
{
    if (start_angle >= FULL_RING_DEGREES || angle_length > FULL_RING_DEGREES ||
        angle_length < -FULL_RING_DEGREES) {
        LOG_ERR("invalid: start angle: %u, angle length: %u", start_angle,
                angle_length);
        return;
    }

    // get first LED index, based on LED at 0º on trigonometric circle

    int32_t led_index = INDEX_RING_ZERO - NUM_RING_LEDS * (int32_t)start_angle /
                                              FULL_RING_DEGREES;
    if (led_index < 0) {
        led_index += NUM_RING_LEDS;
    }

    if (k_sem_take(&leds_update_sem, K_NO_WAIT) == 0) {
        for (size_t i = 0; i < NUM_RING_LEDS; ++i) {
            if (i < (size_t)(NUM_RING_LEDS * abs(angle_length) /
                             FULL_RING_DEGREES)) {
                leds.part.ring_leds[led_index] = color;
            } else {
                if (bg_color) {
                    leds.part.ring_leds[led_index] = *bg_color;
                }
            }

            // depending on angle_length sign, we go through the ring LED in a
            // different direction
            if (angle_length >= 0) {
#ifdef CONFIG_BOARD_PEARL_MAIN
                led_index = (led_index + 1) % ARRAY_SIZE(leds.part.ring_leds);
#else
                led_index = led_index == 0
                                ? ((int32_t)ARRAY_SIZE(leds.part.ring_leds) - 1)
                                : (led_index - 1);
#endif
            } else {
#ifdef CONFIG_BOARD_PEARL_MAIN
                led_index = led_index == 0
                                ? ((int32_t)ARRAY_SIZE(leds.part.ring_leds) - 1)
                                : (led_index - 1);
#else
                led_index = (led_index + 1) % ARRAY_SIZE(leds.part.ring_leds);
#endif
            }
        }

        k_sem_give(&leds_update_sem);
    }
}

int
front_leds_boot_progress_set(enum boot_progress_step_e step)
{
    UNUSED_PARAMETER(step);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (step > BOOT_PROGRESS_STEP_DONE) {
        // log issue
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
        return RET_ERROR_INVALID_PARAM;
    }

    if (boot_progress_target >= step) {
        // it's a no-op, progress can only increase and be set once
        return RET_SUCCESS;
    }

    // first progress, let's animation start from start
    if (boot_progress_current == BOOT_PROGRESS_STEP_UNKNOWN &&
        step > BOOT_PROGRESS_STEP_UNKNOWN) {
        pulsing_index = 0;
    }

    // smooth transition only if increasing progress
    // but give immediate access to jetson/orb-ui when boot is complete
    if (step == BOOT_PROGRESS_STEP_DONE) {
        boot_progress_current = step;
    } else {
        boot_progress_target = step;
    }
#endif
    return RET_SUCCESS;
}

_Noreturn static void
front_leds_thread()
{
    const struct led_rgb rgb_off = RGB_OFF;
    uint8_t intensity;
    struct led_rgb color;
    orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern;
    uint32_t start_angle_degrees;
    int32_t angle_length_degrees;
    float pulsing_scale;
    float scaler;
    k_timeout_t wait_until = K_FOREVER;

    for (;;) {
        // wait for next command
        k_sem_take(&sem_leds_refresh, wait_until);

        wait_until = K_FOREVER;

        CRITICAL_SECTION_ENTER(k);
        pattern = global_pattern;
        intensity = global_intensity;
        color = global_color;
        start_angle_degrees = global_start_angle_degrees;
        angle_length_degrees = global_angle_length_degrees;
        pulsing_scale = global_pulsing_scale;
        CRITICAL_SECTION_EXIT(k);

        if (!use_sequence) {
            switch (pattern) {
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_OFF:
                set_center((struct led_rgb)RGB_OFF);
                set_ring((struct led_rgb)RGB_OFF, NULL, 0, FULL_RING_DEGREES);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_ALL_WHITE:
                color.r = intensity;
                color.g = intensity;
                color.b = intensity;
                set_center(color);
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_NO_CENTER:
                color.r = intensity;
                color.g = intensity;
                color.b = intensity;
                set_center((struct led_rgb)RGB_OFF);
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_RANDOM_RAINBOW:
                if (intensity > 0) {
                    uint32_t shades = intensity > SHADES_PER_COLOR
                                          ? SHADES_PER_COLOR
                                          : intensity;
                    for (size_t i = 0; i < ARRAY_SIZE(leds.all); ++i) {
                        // NOLINTNEXTLINE(cert-msc30-c, cert-msc50-cpp)
                        leds.all[i].r = rand() % shades * (intensity / shades);
                        // NOLINTNEXTLINE(cert-msc30-c, cert-msc50-cpp)
                        leds.all[i].g = rand() % shades * (intensity / shades);
                        // NOLINTNEXTLINE(cert-msc30-c, cert-msc50-cpp)
                        leds.all[i].b = rand() % shades * (intensity / shades);
                    }
                    wait_until = K_MSEC(50);
                } else {
                    memset(leds.all, 0, sizeof leds.all);
                }
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_ONLY_CENTER:
                color.r = intensity;
                color.g = intensity;
                color.b = intensity;
                set_center(color);
                set_ring((struct led_rgb)RGB_OFF, NULL, 0, FULL_RING_DEGREES);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_ALL_RED:
                color.r = intensity;
                color.g = 0;
                color.b = 0;
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                set_center(color);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_ALL_GREEN:
                color.r = 0;
                color.g = intensity;
                color.b = 0;
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                set_center(color);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_ALL_BLUE:
                color.r = 0;
                color.g = 0;
                color.b = intensity;
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                set_center(color);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_PULSING_WHITE:
                color.r = MINIMUM_WHITE_BRIGHTNESS;
                color.g = MINIMUM_WHITE_BRIGHTNESS;
                color.b = MINIMUM_WHITE_BRIGHTNESS;
                pulsing_scale = PULSING_SCALE_DEFAULT;
                // fallthrough
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_PULSING_RGB:
                if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                    // from 0 to 1.0
                    scaler = SINE_LUT[pulsing_index] * pulsing_scale;
                } else {
                    // from 1.0 to 0
                    size_t index = (ARRAY_SIZE(SINE_LUT) - 1 -
                                    (pulsing_index - ARRAY_SIZE(SINE_LUT)));
                    scaler = SINE_LUT[index] * pulsing_scale;
                }
                color.r = roundf(scaler * color.r);
                color.g = roundf(scaler * color.g);
                color.b = roundf(scaler * color.b);

                wait_until = K_MSEC(global_pulsing_delay_time_ms);
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                set_center((struct led_rgb)RGB_OFF);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_PULSING_RGB_ONLY_CENTER:
                if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                    // from 0 to 1.0
                    scaler = SINE_LUT[pulsing_index] * PULSING_SCALE_DEFAULT;
                } else {
                    // from 1.0 to 0
                    size_t index = (ARRAY_SIZE(SINE_LUT) - 1 -
                                    (pulsing_index - ARRAY_SIZE(SINE_LUT)));
                    scaler = SINE_LUT[index] * PULSING_SCALE_DEFAULT;
                }
                color.r = roundf(scaler * color.r);
                color.g = roundf(scaler * color.g);
                color.b = roundf(scaler * color.b);

                wait_until = K_MSEC(global_pulsing_delay_time_ms);
                set_center(color);
                set_ring((struct led_rgb)RGB_OFF, NULL, 0, FULL_RING_DEGREES);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_RGB:
                set_ring(color, &rgb_off, start_angle_degrees,
                         angle_length_degrees);
                set_center((struct led_rgb)RGB_OFF);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_RGB_ONLY_CENTER:
                set_center((struct led_rgb)color);
                set_ring((struct led_rgb)RGB_OFF, NULL, 0, FULL_RING_DEGREES);
                break;
            case orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_BOOT_ANIMATION:
                if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                    // from 0 to 1.0
                    scaler = SINE_LUT[pulsing_index] * PULSING_SCALE_DEFAULT;
                } else {
                    // from 1.0 to 0
                    size_t index = (ARRAY_SIZE(SINE_LUT) - 1 -
                                    (pulsing_index - ARRAY_SIZE(SINE_LUT)));
                    scaler = SINE_LUT[index] * PULSING_SCALE_DEFAULT;
                }
                scaler = MAX(
                    0.0f,
                    scaler - BOOT_ANIMATION_BRIGHTNESS_CUTOFF); // cut off lower
                                                                // brightness

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
                // smooth transition, push segment only when previous is close
                // to solid color, and restart pulsating from 0 for next segment
                if (scaler > (1.0f - BOOT_ANIMATION_BRIGHTNESS_CUTOFF -
                              BOOT_ANIMATION_TRANSITION_THRESHOLD) &&
                    boot_progress_current != boot_progress_target) {
                    boot_progress_current = boot_progress_target;
                    pulsing_index = 0;
                    scaler = 0.0;
                }
#endif

                color.r = roundf(scaler * color.r);
                color.g = roundf(scaler * color.g);
                color.b = roundf(scaler * color.b);

                wait_until = K_MSEC(global_pulsing_delay_time_ms);
                set_center((struct led_rgb)RGB_OFF);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
                uint32_t angle_progress = boot_progress_current *
                                          BOOT_PROGRESS_PERCENTAGE_STEP *
                                          FULL_RING_DEGREES / 100;
                // fill with solid color up to progress angle
                set_ring((struct led_rgb)global_color, &rgb_off, 90,
                         -angle_progress);
                if (boot_progress_current < BOOT_PROGRESS_STEP_DONE) {
                    // moving head
                    uint32_t boot_anim_start_angle =
                        (90 - angle_progress + 360) % 360;
                    set_ring((struct led_rgb)color, NULL, boot_anim_start_angle,
                             -BOOT_ANIMATION_STEP_ANGLE);
                }
#else
                set_ring((struct led_rgb)color, &rgb_off, 0, 360);
#endif
                break;
            default:
                LOG_ERR("Unhandled front LED pattern: %u", global_pattern);
                continue;
            }
        }

        // make INITIAL_PULSING_PERIOD_MS twice as fast by incrementing by 2
        pulsing_index = (pulsing_index + 2) % (ARRAY_SIZE(SINE_LUT) * 2);

        // update LEDs
        if (k_sem_take(&leds_update_sem, K_NO_WAIT) == 0 && !final_done) {
#if defined(CONFIG_BOARD_PEARL_MAIN)
            // 850nm and 940nm IR leds mustn't be on when the RGB strip
            // is updated to prevent the LED from flickering.
            // If they are active and the next pulse is too close in time (we
            // don't have enough time to update all the RGB leds), wait for
            // finished IR pulse.
            wait_for_interrupt =
                (ir_camera_system_get_enabled_leds() >
                 orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_740NM) &&
                (ir_camera_system_get_time_until_update_us() <
                 LED_STRIP_MAXIMUM_UPDATE_TIME_US) &&
                (ir_camera_system_get_fps() > 0);
            if (wait_for_interrupt) {
                k_sem_take(&leds_wait_for_trigger, K_FOREVER);
            }
#endif
            led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
            atomic_set(&leds_dirty, ALL_LEDS_DIRTY);
            k_sem_give(&leds_update_sem);
        }
    }
}

int
front_leds_self_test(void)
{
#ifndef CONFIG_BOARD_DIAMOND_MAIN
    return 0;
#else
    static const struct gpio_dt_spec test_user_leds_dout_gpios[2] = {
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_user_leds_dout_low_gpios),
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_user_leds_dout_high_gpios),
    };

    static const struct gpio_dt_spec test_user_leds_cout_gpios[2] = {
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_user_leds_cout_low_gpios),
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_user_leds_cout_high_gpios),
    };

    size_t test_user_leds_dout_test_passed_count = 0;
    size_t test_user_leds_cout_test_passed_count = 0;
    const size_t test_count_pass_threshold = 2;

    /* setup signals for internal testing */
    int ret = gpio_pin_configure_dt(&test_user_leds_dout_gpios[0], GPIO_INPUT);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&test_user_leds_dout_gpios[1], GPIO_INPUT);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&test_user_leds_cout_gpios[0], GPIO_INPUT);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&test_user_leds_cout_gpios[1], GPIO_INPUT);
    ASSERT_SOFT(ret);

    for (int i = 0; i < 100; ++i) {
        if (k_sem_take(&leds_update_sem, K_MSEC(1)) == 0) {
            memset(leds.all, 0, sizeof leds.all);
            ret =
                led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
            if (ret != 0) {
                ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE,
                                      "led strip update err %d", ret);
                k_sem_give(&leds_update_sem);
                return RET_ERROR_INVALID_STATE;
            }

            if (test_user_leds_dout_test_passed_count <
                test_count_pass_threshold) {
                const int test_data_low =
                    gpio_pin_get_dt(&test_user_leds_dout_gpios[0]);
                const int test_data_high =
                    gpio_pin_get_dt(&test_user_leds_dout_gpios[1]);
                if (test_data_low == 0 && test_data_high == 1) {
                    ++test_user_leds_dout_test_passed_count;
                }
            }

            if (test_user_leds_cout_test_passed_count <
                test_count_pass_threshold) {
                const int test_clock_low =
                    gpio_pin_get_dt(&test_user_leds_cout_gpios[0]);
                const int test_clock_high =
                    gpio_pin_get_dt(&test_user_leds_cout_gpios[1]);
                if (test_clock_low == 0 && test_clock_high == 1) {
                    ++test_user_leds_cout_test_passed_count;
                }
            }

            if (test_user_leds_cout_test_passed_count >=
                    test_count_pass_threshold &&
                test_user_leds_dout_test_passed_count >=
                    test_count_pass_threshold) {
                // the test lines are seeing an active signal
                // now let's test that it goes back to normal when
                // rgb leds are left unanimated
                // /!\ don't release the semaphore just yet, we need to make
                // sure the led strip isn't used.
                k_msleep(100);

                const int test_clock_low =
                    gpio_pin_get_dt(&test_user_leds_cout_gpios[0]);
                const int test_clock_high =
                    gpio_pin_get_dt(&test_user_leds_cout_gpios[1]);
                const int test_data_low =
                    gpio_pin_get_dt(&test_user_leds_dout_gpios[0]);
                const int test_data_high =
                    gpio_pin_get_dt(&test_user_leds_dout_gpios[1]);

                if (test_clock_low == test_clock_high &&
                    test_data_low == test_data_high) {
                    ORB_STATE_SET_CURRENT(RET_SUCCESS, "front leds ok");
                    LOG_INF("rgb leds test passed");
                } else {
                    ORB_STATE_SET_CURRENT(
                        RET_SUCCESS, "ok, but dout/cout test signal stuck?");
                    LOG_INF("rgb leds test passed but stuck; dout low %d, dout "
                            "high %d, cout low %d, cout high %d",
                            test_data_low, test_data_high, test_clock_low,
                            test_clock_high);
                }
                k_sem_give(&leds_update_sem);
                return RET_SUCCESS;
            }
            k_sem_give(&leds_update_sem);
        }
        k_msleep(10);
    }

    ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE, "led strip cut?",
                          test_user_leds_dout_test_passed_count,
                          test_user_leds_cout_test_passed_count);
    return RET_ERROR_INVALID_STATE;
#endif
}

#ifdef CONFIG_FRONT_UNIT_RGB_LEDS_LOG_LEVEL_DBG
static void
print_new_debug(orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern,
                uint32_t start_angle, int32_t angle_length, RgbColor *color,
                uint32_t pulsing_period_ms, float pulsing_scale)
{
    LOG_DBG("pattern = %d", pattern);
    LOG_DBG("start angle = %" PRIu32, start_angle);
    LOG_DBG("angle length = %" PRId32, angle_length);
    if (color) {
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
        LOG_DBG("color = #%02X%02X%02X%02X", color->dimming, color->red,
                color->green, color->blue);
#else
        LOG_DBG("color = #%02X%02X%02X", color->red, color->green, color->blue);
#endif
    } else {
        LOG_DBG("color = NULL");
    }
    LOG_DBG("pulsing period = %" PRIu32 "ms", pulsing_period_ms);
    LOG_DBG("pulsing scale = %g", pulsing_scale);
}
#endif

static ret_code_t
pulsing_rgb_check_range(orb_mcu_main_RgbColor *color, float pulsing_scale)
{
    if (color == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if ((lroundf((float)color->red * (pulsing_scale + 1)) > 255) ||
        (lroundf((float)color->green * (pulsing_scale + 1)) > 255) ||
        (lroundf((float)color->blue * (pulsing_scale + 1)) > 255)) {
        LOG_ERR("Pulsing scale too large");
        return RET_ERROR_INVALID_PARAM;
    } else {
        return RET_SUCCESS;
    }
}

static bool
previous_settings_are_identical(
    orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern,
    uint32_t start_angle, int32_t angle_length, orb_mcu_main_RgbColor *color,
    uint32_t pulsing_period_ms, float pulsing_scale)
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
               (global_color.b == color->blue
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
                && (global_color.scratch == color->dimming)
#endif
               );
    } else {
        return ret;
    }
}

static void
update_parameters(orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern,
                  uint32_t start_angle, int32_t angle_length,
                  orb_mcu_main_RgbColor *color, uint32_t pulsing_period_ms,
                  float pulsing_scale)
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
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
        if (color->dimming != 0) {
            global_color.scratch = color->dimming;
        } else {
            global_color.scratch = RGB_BRIGHTNESS_MAX;
        }
#endif
    }
    use_sequence = false;

    CRITICAL_SECTION_EXIT(k);
}

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
bool
front_leds_is_shroud_on(void)
{
    if (k_sem_take(&leds_update_sem, K_MSEC(1)) == 0) {
        for (size_t i = 0; i < ARRAY_SIZE(leds.part.center_leds); ++i) {
            if (leds.part.center_leds[i].scratch != 0 &&
                (leds.part.center_leds[i].r != 0 ||
                 leds.part.center_leds[i].g != 0 ||
                 leds.part.center_leds[i].b != 0)) {
                k_sem_give(&leds_update_sem);
                return true;
            }
        }
    } else {
        /* default to pessimistic value */
        return true;
    }

    k_sem_give(&leds_update_sem);

    return false;
}
#endif

ret_code_t
front_leds_set_pattern(orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern,
                       uint32_t start_angle, int32_t angle_length,
                       orb_mcu_main_RgbColor *color, uint32_t pulsing_period_ms,
                       float pulsing_scale)
{
    if (pattern == orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_PULSING_RGB) {
        const ret_code_t ret = pulsing_rgb_check_range(color, pulsing_scale);
        if (ret != RET_SUCCESS) {
            return RET_ERROR_INVALID_PARAM;
        }
    }

#if defined(CONFIG_SPI_RGB_LED_DIMMING)
    // if dimming is not set or out of bounds
    // set it to the maximum brightness
    if (color != NULL &&
        (color->dimming == 0 || color->dimming > RGB_BRIGHTNESS_MAX)) {
        color->dimming = RGB_BRIGHTNESS_MAX;
    }
#endif

    if (!previous_settings_are_identical(pattern, start_angle, angle_length,
                                         color, pulsing_period_ms,
                                         pulsing_scale)) {
#ifdef CONFIG_FRONT_UNIT_RGB_LEDS_LOG_LEVEL_DBG
        print_new_debug(pattern, start_angle, angle_length, color,
                        pulsing_period_ms, pulsing_scale);
#endif
        update_parameters(pattern, start_angle, angle_length, color,
                          pulsing_period_ms, pulsing_scale);
        k_sem_give(&sem_leds_refresh);
    }

    return RET_SUCCESS;
}

ret_code_t
front_leds_set_center_leds_sequence_argb32(const uint8_t *bytes, uint32_t size)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (boot_progress_current != BOOT_PROGRESS_STEP_DONE) {
        return RET_SUCCESS;
    }
#endif

    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_ARGB, leds.part.center_leds,
        ARRAY_SIZE(leds.part.center_leds), &leds_update_sem);

    if (ret == RET_SUCCESS || ret == RET_ERROR_ALREADY_INITIALIZED) {
        // RET_ERROR_ALREADY_INITIALIZED or RET_SUCCESS
        // update strip if both center and ring have been
        // set into the LED buffer
        use_sequence = true;
        atomic_and(&leds_dirty, ~CENTER_LEDS_DIRTY);
        if (atomic_get(&leds_dirty) == 0) {
            k_sem_give(&sem_leds_refresh);
        }
        ret = RET_SUCCESS; // overwrite the error if LEDs are already set to
                           // expected values
    } else if (ret != RET_ERROR_INTERNAL) {
        ASSERT_SOFT(ret);
    }

    return ret;
}

ret_code_t
front_leds_set_center_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (boot_progress_current != BOOT_PROGRESS_STEP_DONE) {
        return RET_SUCCESS;
    }
#endif

    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_RGB, leds.part.center_leds,
        ARRAY_SIZE(leds.part.center_leds), &leds_update_sem);

    if (ret == RET_SUCCESS || ret == RET_ERROR_ALREADY_INITIALIZED) {
        // RET_ERROR_ALREADY_INITIALIZED or RET_SUCCESS
        // update strip if both center and ring have been
        // set into the LED buffer
        use_sequence = true;
        atomic_and(&leds_dirty, ~CENTER_LEDS_DIRTY);
        if (atomic_get(&leds_dirty) == 0) {
            k_sem_give(&sem_leds_refresh);
        }
        ret = RET_SUCCESS; // overwrite the error if LEDs are already set to
                           // expected values
    } else if (ret != RET_ERROR_INTERNAL) {
        ASSERT_SOFT(ret);
    }

    return ret;
}

ret_code_t
front_leds_set_ring_leds_sequence_argb32(const uint8_t *bytes, uint32_t size)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (boot_progress_current != BOOT_PROGRESS_STEP_DONE) {
        return RET_SUCCESS;
    }
#endif

    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_ARGB, leds.part.ring_leds,
        ARRAY_SIZE(leds.part.ring_leds), &leds_update_sem);

    if (ret == RET_SUCCESS || ret == RET_ERROR_ALREADY_INITIALIZED) {
        // RET_ERROR_ALREADY_INITIALIZED or RET_SUCCESS
        // update strip if both center and ring have been
        // set into the LED buffer
        use_sequence = true;
        atomic_and(&leds_dirty, ~RING_LEDS_DIRTY);
        if (atomic_get(&leds_dirty) == 0) {
            k_sem_give(&sem_leds_refresh);
        }
        ret = RET_SUCCESS; // overwrite the error if LEDs are already set to
                           // expected values
    } else if (ret != RET_ERROR_INTERNAL) {
        ASSERT_SOFT(ret);
    }

    return ret;
}

ret_code_t
front_leds_set_ring_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (boot_progress_current != BOOT_PROGRESS_STEP_DONE) {
        return RET_SUCCESS;
    }
#endif

    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_RGB, leds.part.ring_leds,
        ARRAY_SIZE(leds.part.ring_leds), &leds_update_sem);

    if (ret == RET_SUCCESS || ret == RET_ERROR_ALREADY_INITIALIZED) {
        // RET_ERROR_ALREADY_INITIALIZED or RET_SUCCESS
        // update strip if both center and ring have been
        // set into the LED buffer
        use_sequence = true;
        atomic_and(&leds_dirty, ~RING_LEDS_DIRTY);
        if (atomic_get(&leds_dirty) == 0) {
            k_sem_give(&sem_leds_refresh);
        }
        ret = RET_SUCCESS; // overwrite the error if LEDs are already set to
                           // expected values
    } else if (ret != RET_ERROR_INTERNAL) {
        ASSERT_SOFT(ret);
    }

    return ret;
}

void
front_leds_set_brightness(uint32_t brightness)
{
    CRITICAL_SECTION_ENTER(k);
    global_intensity = MIN(255, brightness);
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_leds_refresh);
}

void
front_leds_turn_off_blocking(void)
{
    if (k_sem_take(&leds_update_sem, K_MSEC(50)) == 0) {
        final_done = true;
        memset(leds.all, 0, sizeof leds.all);
        led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
        led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
        atomic_set(&leds_dirty, ALL_LEDS_DIRTY);
        k_sem_give(&leds_update_sem);
    }
}

ret_code_t
front_leds_init(void)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    orb_mcu_Hardware_FrontUnitVersion fu_version = version_get_front_unit_rev();
    if (fu_version ==
        orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2B) {
        led_strip = led_strip_apa;
    }
#endif

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid =
        k_thread_create(&front_led_thread_data, front_leds_stack_area,
                        K_THREAD_STACK_SIZEOF(front_leds_stack_area),
                        (k_thread_entry_t)front_leds_thread, NULL, NULL, NULL,
                        THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "front_leds");

    front_leds_self_test();

#ifdef CONFIG_BOARD_DIAMOND_MAIN
    const struct gpio_dt_spec en_5v_switched =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), front_unit_en_5v_switched_gpios);
    int ret = gpio_pin_configure_dt(&en_5v_switched, GPIO_OUTPUT_ACTIVE);
    ASSERT_SOFT(ret);
#endif

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
    ASSERT_CONST_POINTER_NOT_NULL(led_strip_apa)
    ASSERT_CONST_POINTER_NOT_NULL(led_strip_w)

    orb_mcu_Hardware_FrontUnitVersion fu_version = version_get_front_unit_rev();
    if (fu_version ==
        orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2B) {
        led_strip = led_strip_apa;
    }
#endif

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    set_center((struct led_rgb)RGB_OFF);
    set_ring((struct led_rgb)RGB_OFF, NULL, 0, FULL_RING_DEGREES);

    if (k_sem_take(&leds_update_sem, K_MSEC(50)) == 0) {
        led_strip_update_rgb(led_strip, leds.all, ARRAY_SIZE(leds.all));
        atomic_set(&leds_dirty, ALL_LEDS_DIRTY);
        k_sem_give(&leds_update_sem);
    }

    return 0;
}

SYS_INIT(front_leds_initial_state, POST_KERNEL, SYS_INIT_UI_LEDS_PRIORITY);
