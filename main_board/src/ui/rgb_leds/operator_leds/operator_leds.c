#include "operator_leds.h"
#include "app_config.h"
#include "orb_logs.h"
#include "orb_state.h"
#include "ui/rgb_leds/rgb_leds.h"
#include <app_assert.h>
#include <errors.h>
#include <math.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(operator_leds);

static K_THREAD_STACK_DEFINE(operator_leds_stack_area,
                             THREAD_STACK_SIZE_OPERATOR_RGB_LEDS);
static struct k_thread operator_leds_thread_data;
static K_SEM_DEFINE(sem_leds_refresh, 0, 1);

static struct led_rgb leds[OPERATOR_LEDS_COUNT];

// default values
static volatile orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern
    global_pattern =
        orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_BOOT_ANIMATION;
static volatile uint8_t global_intensity = 20;
static volatile uint32_t global_mask = 0b11111;
static volatile struct led_rgb global_color = RGB_WHITE_OPERATOR_LEDS;
static volatile bool use_sequence;

static const volatile uint32_t global_pulsing_delay_time_ms =
    (INITIAL_PULSING_PERIOD_MS / 2) / SINE_TABLE_LENGTH;
/// half period <=> from 0 to pi rad <=> 0 to 1 to 0
extern const float SINE_LUT[SINE_TABLE_LENGTH];

/**
 * Apply color to LEDs based on mask
 * @note  On Pearl, the left LED is the most significant bit
 *        On Diamond, the right LED is the most significant bit
 * @param mask 5-bit mask
 * @param color color to apply
 */
static void
apply_pattern(uint32_t mask, struct led_rgb *color)
{
    // go through mask starting with most significant bit
    // so that mask is applied from left LED to right for the operator
    for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
#if defined(CONFIG_BOARD_PEARL_MAIN)
        uint32_t bit = BIT((OPERATOR_LEDS_COUNT - 1) - i);
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
        uint32_t bit = BIT(i);
#endif
        if (mask & bit) {
            leds[i] = *color;
        } else {
            leds[i] = (struct led_rgb)RGB_OFF;
        }
    }
}

_Noreturn static void
operator_leds_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct device *led_strip = a;
    uint8_t intensity;
    uint32_t mask;
    struct led_rgb color;
    orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern pattern =
        orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_BOOT_ANIMATION;
    float scaler;
    k_timeout_t wait_until = K_MSEC(global_pulsing_delay_time_ms);
    uint32_t pulsing_index = ARRAY_SIZE(SINE_LUT);

    for (;;) {
        k_sem_take(&sem_leds_refresh, wait_until);
        wait_until = K_MSEC(global_pulsing_delay_time_ms);

        // ⚠️ Critical section
        // create local copies in a critical section to make sure we don't
        // interfere with the values while applying LED config
        CRITICAL_SECTION_ENTER(k);
        if (pattern != global_pattern) {
            // start with LEDs on
            pulsing_index = ARRAY_SIZE(SINE_LUT);
        }
        pattern = global_pattern;
        intensity = global_intensity;
        mask = global_mask;
        color = global_color;
        CRITICAL_SECTION_EXIT(k);

        switch (pattern) {
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_OFF:
            color = (struct led_rgb)RGB_OFF;
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_ALL_WHITE:
            color.r = intensity;
            color.g = intensity;
            color.b = intensity;
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_ALL_RED:
            color.r = intensity;
            color.g = 0;
            color.b = 0;
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_ALL_GREEN:
            color.r = 0;
            color.g = intensity;
            color.b = 0;
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_ALL_BLUE:
            color.r = 0;
            color.g = 0;
            color.b = intensity;
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_RGB:
            /* Do nothing, color already set from global_color */
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_PULSING_RGB:
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
            pulsing_index = (pulsing_index + 1) % (ARRAY_SIZE(SINE_LUT) * 2);
            break;
        case orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_BOOT_ANIMATION: {
            size_t solid_iterations = 1000 / global_pulsing_delay_time_ms;
            if (pulsing_index < ARRAY_SIZE(SINE_LUT)) {
                // from 0 to 1.0
                scaler = SINE_LUT[pulsing_index] * PULSING_SCALE_DEFAULT;
            } else if (pulsing_index <
                       (ARRAY_SIZE(SINE_LUT) + solid_iterations)) {
                // solid
                scaler = PULSING_SCALE_DEFAULT;
            } else {
                // [array size, 0] -> [1.0 to 0]
                scaler = SINE_LUT[(ARRAY_SIZE(SINE_LUT) - 1 -
                                   (pulsing_index - solid_iterations -
                                    ARRAY_SIZE(SINE_LUT)))] *
                         PULSING_SCALE_DEFAULT;
            }
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
            color.scratch = roundf(scaler * color.scratch);
#else
            color.r = roundf(scaler * color.r);
            color.g = roundf(scaler * color.g);
            color.b = roundf(scaler * color.b);
#endif
            wait_until = K_MSEC(global_pulsing_delay_time_ms);

            pulsing_index = (pulsing_index + 1) %
                            (ARRAY_SIZE(SINE_LUT) * 2 + solid_iterations);

        } break;
        default:
            LOG_ERR("Unhandled operator LED pattern: %u", pattern);
            break;
        }

        if (!use_sequence) {
            CRITICAL_SECTION_ENTER(k);
            apply_pattern(mask, &color);
            CRITICAL_SECTION_EXIT(k);
        }
        led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    }
}

int
operator_leds_set_brightness(uint8_t brightness)
{
    global_intensity = brightness;
    k_sem_give(&sem_leds_refresh);

    return RET_SUCCESS;
}

int
operator_leds_set_pattern(
    orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern pattern,
    uint32_t mask, const orb_mcu_main_RgbColor *color)
{
    CRITICAL_SECTION_ENTER(k);

    global_pattern = pattern;
    global_mask = mask;

    if (color != NULL) {
        // RgbColor -> struct led_rgb
#ifdef CONFIG_LED_STRIP_RGB_SCRATCH
        if (color->dimming == 0 || color->dimming > RGB_BRIGHTNESS_MAX) {
            global_color.scratch = RGB_BRIGHTNESS_MAX;
        } else {
            global_color.scratch = color->dimming;
        }
#endif
        global_color.r = color->red;
        global_color.g = color->green;
        global_color.b = color->blue;
    }
    use_sequence = false;
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_leds_refresh);

    return RET_SUCCESS;
}

ret_code_t
operator_leds_set_leds_sequence_argb32(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(bytes, size, LED_FORMAT_ARGB,
                                                leds, ARRAY_SIZE(leds), NULL);
    if (ret == RET_SUCCESS) {
        use_sequence = true;
        k_sem_give(&sem_leds_refresh);
    } else if (ret != RET_ERROR_ALREADY_INITIALIZED) {
        ASSERT_SOFT(ret);
    } else {
        ret = RET_SUCCESS; // overwrite the error if LEDs are already set to
                           // expected values
    }
    return ret;
}

ret_code_t
operator_leds_set_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(bytes, size, LED_FORMAT_RGB,
                                                leds, ARRAY_SIZE(leds), NULL);
    if (ret == RET_SUCCESS) {
        use_sequence = true;
        k_sem_give(&sem_leds_refresh);
    } else if (ret != RET_ERROR_ALREADY_INITIALIZED) {
        ASSERT_SOFT(ret);
    } else {
        ret = RET_SUCCESS; // overwrite the error if LEDs are already set to
                           // expected values
    }
    return ret;
}

int
operator_leds_init(orb_mcu_Hardware_OrbVersion board_version)
{
#ifdef CONFIG_BOARD_DIAMOND_MAIN
#define LED_STRIP_MAYBE_CONST
#else
#define LED_STRIP_MAYBE_CONST const
    UNUSED_PARAMETER(board_version);
#endif

    // ReSharper disable once CppPointerConversionDropsQualifiers
    LED_STRIP_MAYBE_CONST struct device *led_strip =
        (struct device *)DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds));

#ifdef CONFIG_BOARD_DIAMOND_MAIN
    // on evt units, apa102 were mounted
    if (board_version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_EVT) {
        led_strip =
            (struct device *)DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds_apa));
    }
#endif

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Operator LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid = k_thread_create(
        &operator_leds_thread_data, operator_leds_stack_area,
        K_THREAD_STACK_SIZEOF(operator_leds_stack_area),
        (k_thread_entry_t)operator_leds_thread, (void *)led_strip, NULL, NULL,
        THREAD_PRIORITY_OPERATOR_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "operator_leds");

    return RET_SUCCESS;
}

void
operator_leds_set_blocking(const orb_mcu_main_RgbColor *color, uint32_t mask)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Operator LED strip not ready!");
        return;
    }

    if (color == NULL) {
        LOG_ERR("Color is NULL");
        return;
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    // previous_mask initialized to invalid value to force first update
    static uint32_t previous_mask = -1;
    const struct gpio_dt_spec supply_5v_rgb_enable_gpio_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_5v_rgb_enable_gpios);

    if (previous_mask == 0 && mask != 0) {
        // enable power supply and mux for communication to LEDs
        gpio_pin_set_dt(&supply_5v_rgb_enable_gpio_spec, 1);
    }
#endif

#ifdef CONFIG_LED_STRIP_RGB_SCRATCH
    uint8_t intensity = RGB_BRIGHTNESS_MAX;
    if (color->dimming != 0 && color->dimming <= RGB_BRIGHTNESS_MAX) {
        intensity = color->dimming;
    }
#endif

    struct led_rgb c = {
#ifdef CONFIG_LED_STRIP_RGB_SCRATCH
        .scratch = intensity,
#endif
        .r = color->red,
        .g = color->green,
        .b = color->blue};
    apply_pattern(mask, &c);
    int ret = led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    ASSERT_SOFT(ret);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (mask == 0 && previous_mask != 0) {
        // disable power supply and mux for communication to LEDs
        gpio_pin_set_dt(&supply_5v_rgb_enable_gpio_spec, 0);
    }

    previous_mask = mask;
#endif
}

void
operator_leds_indicate_low_battery_blocking(void)
{
    orb_mcu_main_RgbColor color = {.red = 5, .green = 0, .blue = 0};
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
    color.dimming = RGB_BRIGHTNESS_MAX;
#endif

    for (int i = 0; i < 3; ++i) {
        operator_leds_set_blocking(&color, 0b11111);
        k_msleep(500);
        operator_leds_set_blocking(&color, 0b00000);
        k_msleep(500);
    }
}

/**
 * Turn one operator LED during boot to indicate battery switch is turned on
 * @param dev
 * @return 0 on success, error code otherwise
 */
int
operator_leds_initial_state(void)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Operator LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    // enable 5V_RGB and mux for communication to LEDs
    const struct gpio_dt_spec supply_5v_rgb_enable_gpio_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_5v_rgb_enable_gpios);
    gpio_pin_set_dt(&supply_5v_rgb_enable_gpio_spec, 1);
#endif

    struct led_rgb color = global_color;
    apply_pattern(global_mask, &color);
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));

    return 0;
}

SYS_INIT(operator_leds_initial_state, POST_KERNEL, SYS_INIT_UI_LEDS_PRIORITY);
