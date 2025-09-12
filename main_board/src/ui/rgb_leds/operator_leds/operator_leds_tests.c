#include "app_config.h"
#include "operator_leds.h"

#include "orb_logs.h"
LOG_MODULE_REGISTER(operator_leds_test);

#if CONFIG_BOARD_DIAMOND_MAIN
#include "app_assert.h"
#include "orb_state.h"
#include "power/boot/boot.h"
#include "zephyr/drivers/gpio.h"

ORB_STATE_REGISTER(button_led);

static int
operator_leds_self_test(void)
{
    const struct gpio_dt_spec supply_5v_rgb_enable_gpio_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_5v_rgb_enable_gpios);
    int ret;

    power_vbat_5v_3v3_supplies_on();
    ret = gpio_pin_configure_dt(&supply_5v_rgb_enable_gpio_spec,
                                GPIO_OUTPUT_HIGH);
    ASSERT_SOFT(ret);
    k_msleep(100);

    /* bitbang to LED */
    static const struct gpio_dt_spec op_led_spi_mux_set_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_op_led_mux_set_gpios);
    static const struct gpio_dt_spec op_led_spi_mux_en_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_op_led_mux_en_gpios);
    static const struct gpio_dt_spec op_led_spi_clk_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_op_led_spi_clk_gpios);
    static const struct gpio_dt_spec op_led_spi_mosi_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_op_led_spi_mosi_gpios);

    /* io to read signal back */
    static const struct gpio_dt_spec test_op_led_data_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_op_led_data_gpios);
    static const struct gpio_dt_spec test_op_led_clk_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), test_op_led_data_gpios);

    ret = gpio_pin_configure_dt(&op_led_spi_mux_set_spec, GPIO_OUTPUT_HIGH);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&op_led_spi_mux_en_spec, GPIO_OUTPUT_HIGH);
    ASSERT_SOFT(ret);

    ret = gpio_pin_configure_dt(&op_led_spi_clk_spec, GPIO_OUTPUT_LOW);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&op_led_spi_mosi_spec, GPIO_OUTPUT_LOW);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&test_op_led_data_spec, GPIO_INPUT);
    ASSERT_SOFT(ret);
    ret = gpio_pin_configure_dt(&test_op_led_clk_spec, GPIO_INPUT);
    ASSERT_SOFT(ret);

    k_msleep(10);

    // bitbang test pattern
    // 32 zeros
    for (int i = 0; i < 32; ++i) {
        ret = gpio_pin_set_dt(&op_led_spi_clk_spec, 1);
        ASSERT_SOFT(ret);
        k_msleep(1);

        ret = gpio_pin_set_dt(&op_led_spi_clk_spec, 0);
        ASSERT_SOFT(ret);
        k_msleep(1);
    }

    // send 4 leds values so that signal is forwarded to test pins
    int test_data = gpio_pin_get_dt(&test_op_led_data_spec);
    size_t test_toggled_count = 0;
    uint8_t led_array[4] = {0xE1, 0xE1, 0xE1, 0xE1};
    for (int led_count = 0; led_count < 4; ++led_count) {
        for (size_t i = 0; i < ARRAY_SIZE(led_array); ++i) {
            for (int j = 0; j < 8; ++j) {
                ret = gpio_pin_set_dt(&op_led_spi_mosi_spec,
                                      (led_array[i] & (1 << (7 - j))) ? 1 : 0);
                k_usleep(5);

                ret = gpio_pin_set_dt(&op_led_spi_clk_spec, 1);
                ASSERT_SOFT(ret);
                k_usleep(2);

                int current_data = gpio_pin_get_dt(&test_op_led_data_spec);
                if (current_data != test_data) {
                    ++test_toggled_count;
                    test_data = current_data;
                }

                ret = gpio_pin_set_dt(&op_led_spi_clk_spec, 0);
                ASSERT_SOFT(ret);
                k_usleep(2);

                current_data = gpio_pin_get_dt(&test_op_led_data_spec);
                if (current_data != test_data) {
                    ++test_toggled_count;
                    test_data = current_data;
                }
            }
        }
    }

    // send 32 ones
    ret = gpio_pin_set_dt(&op_led_spi_mosi_spec, 1);
    ASSERT_SOFT(ret);
    k_usleep(1);
    for (int i = 0; i < 32; ++i) {
        ret = gpio_pin_set_dt(&op_led_spi_clk_spec, 1);
        ASSERT_SOFT(ret);
        k_msleep(1);
        ret = gpio_pin_set_dt(&op_led_spi_clk_spec, 0);
        ASSERT_SOFT(ret);
        k_msleep(1);
    }

    ret = gpio_pin_set_dt(&op_led_spi_mosi_spec, 0);
    ASSERT_SOFT(ret);

    ret =
        gpio_pin_configure_dt(&supply_5v_rgb_enable_gpio_spec, GPIO_OUTPUT_LOW);
    ASSERT_SOFT(ret);

    power_vbat_5v_3v3_supplies_off();

    if (test_toggled_count) {
        LOG_INF("op leds ok (%u)", test_toggled_count);
        ORB_STATE_SET_CURRENT(RET_SUCCESS, "op leds ok (%lu)",
                              test_toggled_count);
    } else {
        LOG_INF("op leds disconnected?");
        ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE, "disconnected?");
    }

    return test_toggled_count ? RET_SUCCESS : RET_ERROR_INVALID_STATE;
}

SYS_INIT(operator_leds_self_test, POST_KERNEL,
         SYS_INIT_OP_LED_SELF_TEST_PRIORITY);
BUILD_ASSERT(CONFIG_GPIO_PCA95XX_INIT_PRIORITY <=
             SYS_INIT_OP_LED_SELF_TEST_PRIORITY);

#endif /* CONFIG_BOARD_DIAMOND_MAIN */

#if CONFIG_ZTEST

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define RGB_ORANGE_TEST                                                        \
    {                                                                          \
        255, 255 / 2, 0, 5                                                     \
    }

/// Test all patterns with 2 brightness levels
ZTEST(hil, test_operator_leds_patterns)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_OPERATOR_LEDS);

    uint8_t brightness[2] = {0x08, 0x10};
    orb_mcu_main_RgbColor color = RGB_ORANGE_TEST;
    int ret_code;

    for (size_t b = 0; b < ARRAY_SIZE(brightness); ++b) {
        ret_code = operator_leds_set_brightness(brightness[b]);
        zassert_equal(ret_code, 0);

        for (
            int i =
                orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_OFF;
            i <=
            orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_PULSING_RGB;
            ++i) {
            for (uint32_t j = 1; j <= OPERATOR_LEDS_ALL_MASK; j = j * 2) {
                ret_code = operator_leds_set_pattern(i, j, &color);
                zassert_equal(ret_code, 0);

                // give time for visual inspection
                k_msleep(100);
            }
        }
    }

    ret_code = operator_leds_set_pattern(
        orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_OFF, 0,
        NULL);
    zassert_equal(ret_code, 0);
}

#endif /* CONFIG_ZTEST */
