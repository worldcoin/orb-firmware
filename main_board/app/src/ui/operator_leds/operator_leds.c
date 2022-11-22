#include <app_assert.h>
#include <app_config.h>
#include <errors.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(operator_leds);

#include "operator_leds.h"
#include "ui/rgb_leds.h"

static K_THREAD_STACK_DEFINE(operator_leds_stack_area,
                             THREAD_STACK_SIZE_OPERATOR_RGB_LEDS);
static struct k_thread operator_leds_thread_data;
static K_SEM_DEFINE(sem_new_setting, 0, 1);

static struct led_rgb leds[OPERATOR_LEDS_COUNT];

// default values
static volatile DistributorLEDsPattern_DistributorRgbLedPattern global_pattern =
    DistributorLEDsPattern_DistributorRgbLedPattern_ALL_WHITE;
static volatile uint8_t global_intensity = 20;
static volatile uint32_t global_mask = 0b11111;
static volatile struct led_rgb global_color = RGB_WHITE_OPERATOR_LEDS;
static volatile bool use_sequence;

static void
apply_pattern(uint32_t mask, struct led_rgb *color)
{
    // go through mask starting with most significant bit
    // so that mask is applied from left LED to right for the operator
    for (size_t i = 0; i < ARRAY_SIZE_ASSERT(leds); ++i) {
        if (mask & BIT((OPERATOR_LEDS_COUNT - 1) - i)) {
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
    DistributorLEDsPattern_DistributorRgbLedPattern pattern;

    for (;;) {
        k_sem_take(&sem_new_setting, K_FOREVER);

        // ⚠️ Critical section
        // create local copies in a critical section to make sure we don't
        // interfere with the values while applying LED config
        CRITICAL_SECTION_ENTER(k);
        pattern = global_pattern;
        intensity = global_intensity;
        mask = global_mask;
        color = global_color;
        CRITICAL_SECTION_EXIT(k);

        switch (pattern) {
        case DistributorLEDsPattern_DistributorRgbLedPattern_OFF:
            color = (struct led_rgb)RGB_OFF;
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_WHITE:
            color.r = intensity;
            color.g = intensity;
            color.b = intensity;
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_RED:
            color.r = intensity;
            color.g = 0;
            color.b = 0;
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_GREEN:
            color.r = 0;
            color.g = intensity;
            color.b = 0;
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_BLUE:
            color.r = 0;
            color.g = 0;
            color.b = intensity;
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_RGB:
            /* Do nothing, color already set from global_color */
            break;
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
    k_sem_give(&sem_new_setting);

    return RET_SUCCESS;
}

int
operator_leds_set_pattern(
    DistributorLEDsPattern_DistributorRgbLedPattern pattern, uint32_t mask,
    const RgbColor *color)
{
    CRITICAL_SECTION_ENTER(k);

    global_pattern = pattern;
    global_mask = mask;

    if (color != NULL) {
        // RgbColor -> struct led_rgb
        global_color.r = color->red;
        global_color.g = color->green;
        global_color.b = color->blue;
    }
    use_sequence = false;
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_new_setting);

    return RET_SUCCESS;
}

ret_code_t
operator_leds_set_leds_sequence(uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = RET_SUCCESS;

    if (size % 3 != 0) {
        LOG_ERR("Bytes must be a multiple of 3");
        ret = RET_ERROR_INVALID_PARAM;
        ASSERT_SOFT(ret);
        return ret;
    }

    size = MIN(size, ARRAY_SIZE(leds) * 3);

    CRITICAL_SECTION_ENTER(k);
    for (size_t i = 0; i < (size / 3); ++i) {
        leds[i].r = bytes[i * 3];
        leds[i].g = bytes[i * 3 + 1];
        leds[i].b = bytes[i * 3 + 2];
    }
    for (size_t i = size / 3; i < ARRAY_SIZE(leds); ++i) {
        leds[i] = (struct led_rgb)RGB_OFF;
    }
    use_sequence = true;
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_new_setting);
    return ret;
}

int
operator_leds_init(void)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Operator LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid =
        k_thread_create(&operator_leds_thread_data, operator_leds_stack_area,
                        K_THREAD_STACK_SIZEOF(operator_leds_stack_area),
                        operator_leds_thread, (void *)led_strip, NULL, NULL,
                        THREAD_PRIORITY_OPERATOR_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "operator_leds");

    return RET_SUCCESS;
}

void
operator_leds_blocking_set(const RgbColor *color, uint32_t mask)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Operator LED strip not ready!");
        return;
    }

    struct led_rgb c = {.r = color->red, .g = color->green, .b = color->blue};
    apply_pattern(mask, &c);
    int ret = led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    ASSERT_SOFT(ret);
}

/// Turn one operator LED during boot to indicate battery switch is turned on
/// \param dev
/// \return 0 on success, error code otherwise
int
operator_leds_initial_state(const struct device *dev)
{
    ARG_UNUSED(dev);

    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(operator_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Operator LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    struct led_rgb color = global_color;
    apply_pattern(global_mask, &color);
    led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));

    return 0;
}

SYS_INIT(operator_leds_initial_state, POST_KERNEL, SYS_INIT_UI_LEDS_PRIORITY);
