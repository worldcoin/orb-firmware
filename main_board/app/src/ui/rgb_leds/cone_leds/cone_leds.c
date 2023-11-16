#include "cone_leds.h"
#include "app_config.h"
#include "logs_can.h"
#include "ui/rgb_leds/rgb_leds.h"
#include <app_assert.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(cone_rgb_leds, CONFIG_CONE_RGB_LEDS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(cone_leds_stack_area,
                             THREAD_STACK_SIZE_CONE_RGB_LEDS);
static struct k_thread cone_leds_thread_data;
static K_SEM_DEFINE(sem_new_setting, 0, 1);

static const struct device *const led_strip =
    DEVICE_DT_GET(DT_NODELABEL(cone_rgb_leds));

#define NUM_LEDS DT_PROP(DT_NODELABEL(cone_rgb_leds), num_leds)

static struct led_rgb leds[NUM_LEDS] = {0};

static const struct gpio_dt_spec cone_5v_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), cone_5v_enable_gpios);

static volatile bool use_sequence;

static const volatile uint32_t global_pulsing_delay_time_ms =
    (INITIAL_PULSING_PERIOD_MS / 2) / SINE_TABLE_LENGTH;

static volatile bool use_sequence;
static volatile ConeLEDsPattern_ConeRgbLedPattern global_pattern =
    ConeLEDsPattern_ConeRgbLedPattern_OFF;
static volatile struct led_rgb global_color = RGB_WHITE;

static void
set_ring(struct led_rgb color)
{
    CRITICAL_SECTION_ENTER(k);
    for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
        leds[i] = color;
    }
    CRITICAL_SECTION_EXIT(k);
}

_Noreturn static void
cone_leds_thread()
{
    ConeLEDsPattern_ConeRgbLedPattern pattern;
    struct led_rgb color;
    k_timeout_t wait_until = K_FOREVER;

    while (1) {
        k_sem_take(&sem_new_setting, wait_until);
        wait_until = K_FOREVER;

        CRITICAL_SECTION_ENTER(k);
        pattern = global_pattern;
        color = global_color;
        CRITICAL_SECTION_EXIT(k);

        if (!use_sequence) {
            switch (pattern) {
            case ConeLEDsPattern_ConeRgbLedPattern_OFF:
                set_ring((struct led_rgb)RGB_OFF);
                break;
            case ConeLEDsPattern_ConeRgbLedPattern_RGB:
                set_ring(color);
                break;
            }
        }

        led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    }
}

ret_code_t
cone_leds_set_pattern(ConeLEDsPattern_ConeRgbLedPattern pattern,
                      RgbColor *color)
{
    ret_code_t ret = RET_SUCCESS;

    CRITICAL_SECTION_ENTER(k);
    global_pattern = pattern;
    global_color = (struct led_rgb){
#ifdef CONFIG_LED_STRIP_RGB_SCRATCH
        .scratch = RGB_BRIGHTNESS_MAX,
#endif
        .r = color->red,
        .g = color->green,
        .b = color->blue};
    use_sequence = false;
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_new_setting);
    return ret;
}

ret_code_t
cone_leds_set_leds_sequence_argb32(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_ARGB, leds, ARRAY_SIZE(leds),
        (bool *)&use_sequence, &sem_new_setting, NULL);
    ASSERT_SOFT(ret);
    return ret;
}

ret_code_t
cone_leds_set_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size)
{
    ret_code_t ret = rgb_leds_set_leds_sequence(
        bytes, size, LED_FORMAT_RGB, leds, ARRAY_SIZE(leds),
        (bool *)&use_sequence, &sem_new_setting, NULL);
    ASSERT_SOFT(ret);
    return ret;
}

ret_code_t
cone_leds_init(void)
{
    if (!device_is_ready(led_strip)) {
        // Might be an Orb without a Cone
        LOG_WRN("Cone LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    if (!device_is_ready(cone_5v_enable_gpio_spec.port)) {
        LOG_WRN("cone 5V enable signal device not ready");
    } else {
        int err_code = gpio_pin_configure_dt(&cone_5v_enable_gpio_spec,
                                             GPIO_OUTPUT_ACTIVE);
        if (err_code) {
            LOG_WRN("error enabling 5V on cone");
        }
    }

    k_tid_t tid = k_thread_create(&cone_leds_thread_data, cone_leds_stack_area,
                                  K_THREAD_STACK_SIZEOF(cone_leds_stack_area),
                                  cone_leds_thread, NULL, NULL, NULL,
                                  THREAD_PRIORITY_CONE_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "cone_leds");

    return RET_SUCCESS;
}
