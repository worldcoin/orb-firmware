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

LOG_MODULE_REGISTER(ir_leds_checks, CONFIG_IR_LEDS_CHECKS_LOG_LEVEL);
ORB_STATE_REGISTER(ir_safety);

int
optics_self_test(void)
{
    static const struct gpio_dt_spec ir_leds_gpios[] = {
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 0),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 1),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 2),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 3),
    };
    static const struct gpio_dt_spec front_unit_pvcc_enabled =
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                front_unit_pvcc_enabled_gpios, 0);

    static const char *ir_leds_names[] = {
        "ir_850nm_left",
        "ir_850nm_right",
        "ir_940nm_left",
        "ir_940nm_right",
    };

    int ret;

    // turn on IR LED subsets one by one, by driving GPIO pins, to check
    // that all lines are making the eye safety circuitry trip
    ret = gpio_pin_configure_dt(&front_unit_pvcc_enabled, GPIO_INPUT);
    if (ret) {
        ORB_STATE_SET(ir_safety, RET_ERROR_NOT_INITIALIZED);
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    orb_mcu_HardwareDiagnostic_Status self_test_status =
        orb_mcu_HardwareDiagnostic_Status_STATUS_OK;
    for (size_t i = 0;
         i < ARRAY_SIZE(ir_leds_gpios) &&
         self_test_status == orb_mcu_HardwareDiagnostic_Status_STATUS_OK;
         i++) {
        power_vbat_5v_3v3_supplies_on();
        boot_turn_off_pvcc();

        gpio_pin_configure_dt(&ir_leds_gpios[i], GPIO_OUTPUT);
        gpio_pin_set_dt(&ir_leds_gpios[i], 1);
        k_msleep(250);
        gpio_pin_set_dt(&ir_leds_gpios[i], 0);
        k_msleep(250);

        bool pvcc_available = true;
        int ret = gpio_pin_get_dt(&front_unit_pvcc_enabled);
        if (ret < 0) {
            ASSERT_SOFT(ret);
        } else {
            pvcc_available = (ret != 0);
        }
        if (pvcc_available) {
            // eye safety circuitry doesn't respond to self test
            LOG_ERR("%s didn't disable PVCC via eye safety circuitry",
                    ir_leds_names[i]);
            ORB_STATE_SET(ir_safety, RET_ERROR_UNSAFE, "%s didn't disable pvcc",
                          ir_leds_names[i]);
            self_test_status =
                orb_mcu_HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE;
        } else {
            LOG_DBG("%s tripped safety circuitry", ir_leds_names[i]);
        }

        // eye safety circuitry to reset
        power_vbat_5v_3v3_supplies_off();
        k_msleep(50);
    }

    if (self_test_status == orb_mcu_HardwareDiagnostic_Status_STATUS_OK) {
        ORB_STATE_SET(ir_safety, RET_SUCCESS);
        LOG_INF(
            "IR eye safety self-test passed (all IR LED lines tripped safety)");
    }
    return RET_SUCCESS;
}
