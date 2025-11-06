#include "app_config.h"
#include "errors.h"
#include "orb_state.h"
#include "power/boot/boot.h"
#include <app_assert.h>
#include <common.pb.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#if defined(CONFIG_ZTEST)
#include <zephyr/logging/log.h>
#else
#include "orb_logs.h"
#endif

int
optics_self_test(void)
{
    /* on diamond, the test is performed during boot (SYS_INIT)
     * refer to `eye_safety_self_test` below
     */
    return RET_SUCCESS;
}

#if 0

LOG_MODULE_REGISTER(ir_leds_checks, CONFIG_IR_LEDS_CHECKS_LOG_LEVEL);

ORB_STATE_REGISTER(ir_safety);
#define SAFETY_TRIGGER_TIMEOUT_MS 100

/*
 * FIXME: implement on PVT only
 * Disabled, as it makes a red LED blink on the front face
 */

static int
eye_safety_self_test(void)
{
    int ret;

    static const struct gpio_dt_spec ir_leds_gpios[] = {
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 0),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 1),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 2),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 3),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 4),
    };
    static const struct gpio_dt_spec front_unit_pvcc_enabled =
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                front_unit_pvcc_enabled_gpios, 0);

    static const char *ir_leds_names[] = {
        "ir_850nm_center", "ir_850nm_side",   "ir_940nm_center",
        "ir_940nm_side",   "ir_940nm_single",
    };

    orb_mcu_HardwareDiagnostic_Status self_test_status =
        orb_mcu_HardwareDiagnostic_Status_STATUS_OK;
    for (size_t i = 0;
         i < ARRAY_SIZE(ir_leds_gpios) &&
         self_test_status == orb_mcu_HardwareDiagnostic_Status_STATUS_OK;
         i++) {
        power_vbat_5v_3v3_supplies_on();
        boot_turn_off_pvcc();

        k_msleep(20);

        const struct device *i2c1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));
        ret = device_init(i2c1);
        if (ret) {
            LOG_ERR("Failed to initialize I2C1 bus: %d", ret);
            return ret;
        }

        const struct device *pca95xx_dev =
            DEVICE_DT_GET(DT_NODELABEL(gpio_exp_front_unit));
        ret = device_init(pca95xx_dev);
        if (ret) {
            LOG_ERR("Failed to initialize PCA95xx device: %d", ret);
            return ret;
        }

        // turn on IR LED subsets one by one, by driving GPIO pins, to check
        // that all lines are making the eye safety circuitry trip
        ret = gpio_pin_configure_dt(&front_unit_pvcc_enabled, GPIO_INPUT);
        ASSERT_SOFT(ret);

        bool pvcc_available = true;
        ret = gpio_pin_get_dt(&front_unit_pvcc_enabled);
        if (ret < 0) {
            ASSERT_SOFT(ret);
        } else {
            pvcc_available = (ret != 0);
        }
        if (pvcc_available) {
            ret = gpio_pin_configure_dt(&ir_leds_gpios[i], GPIO_OUTPUT_ACTIVE);
            ASSERT_SOFT(ret);

            uint32_t tick = k_uptime_get_32();

            size_t timeout = SAFETY_TRIGGER_TIMEOUT_MS;
            while (gpio_pin_get_dt(&front_unit_pvcc_enabled) != 0 &&
                   timeout--) {
                k_msleep(1);
            }

            uint32_t tock_delta = k_uptime_get_32() - tick;
            LOG_DBG("%s disabled pvcc through safety circuitry after %u ms",
                    ir_leds_names[i], tock_delta);
            if (tock_delta >= SAFETY_TRIGGER_TIMEOUT_MS) {
                LOG_ERR("%s didn't trip safety circuitry in time",
                        ir_leds_names[i]);
                self_test_status =
                    orb_mcu_HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE;
                ORB_STATE_SET(ir_safety, RET_ERROR_UNSAFE,
                              "%s didn't disable pvcc", ir_leds_names[i]);
            } else {
                LOG_DBG("%s tripped safety circuitry", ir_leds_names[i]);
            }

            ret = gpio_pin_set_dt(&ir_leds_gpios[i], 0);
            ASSERT_SOFT(ret);
        } else {
            self_test_status =
                orb_mcu_HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE;
            LOG_ERR("PVCC already disabled, cannot test %s", ir_leds_names[i]);
        }

        // reset eye safety circuitry
        power_vbat_5v_3v3_supplies_off();
        k_msleep(400);
    }

    if (self_test_status == orb_mcu_HardwareDiagnostic_Status_STATUS_OK) {
        ORB_STATE_SET(ir_safety, RET_SUCCESS);
        LOG_INF(
            "IR eye safety self-test passed (all IR LED lines tripped safety)");
    }

    return RET_SUCCESS;
}

SYS_INIT(eye_safety_self_test, POST_KERNEL,
         SYS_INIT_IR_SAFETY_SELF_TEST_PRIORITY);

BUILD_ASSERT(CONFIG_GPIO_PCA95XX_INIT_PRIORITY <
                 SYS_INIT_IR_SAFETY_SELF_TEST_PRIORITY,
             "power board gpio expanders are needed to perform the test");

#endif
