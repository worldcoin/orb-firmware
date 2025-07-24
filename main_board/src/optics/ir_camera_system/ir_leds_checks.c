
#include "errors.h"
#include "orb_state.h"
#include <common.pb.h>

#include <app_assert.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#if defined(CONFIG_BOARD_PEARL_MAIN)
#include "power/boot/boot.h"
#endif

#if defined(CONFIG_ZTEST)
#include <zephyr/logging/log.h>
#else
#include "orb_logs.h"
#endif

LOG_MODULE_DECLARE(ir_camera_system, CONFIG_IR_CAMERA_SYSTEM_LOG_LEVEL);

ORB_STATE_REGISTER_MULTIPLE(pvcc);
#ifdef CONFIG_BOARD_PEARL_MAIN
ORB_STATE_REGISTER_MULTIPLE(ir_self);
#endif

// pin allows us to check whether PVCC is enabled on the front unit
// PVCC might be disabled by hardware due to an intense usage of the IR LEDs
// that doesn't respect the eye safety constraints
static struct gpio_dt_spec front_unit_pvcc_enabled = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), front_unit_pvcc_enabled_gpios, 0);

int
optics_self_test(void)
{
#if defined(CONFIG_BOARD_PEARL_MAIN)
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
    ret = gpio_pin_configure_dt(
        &front_unit_pvcc_enabled,
        GPIO_INPUT); // todo: on Diamond we need to enable +3V3 before reading
                     // the Front Unit IO expander
    if (ret) {
        ORB_STATE_SET(ir_self, RET_ERROR_NOT_INITIALIZED);
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
            ORB_STATE_SET(ir_self, RET_ERROR_UNSAFE, "%s didn't disable pvcc",
                          ir_leds_names[i]);
        } else {
            LOG_INF("%s tripped safety circuitry", ir_leds_names[i]);
        }

        // eye safety circuitry to reset
        power_vbat_5v_3v3_supplies_off();
        k_msleep(50);
    }

    if (self_test_status == orb_mcu_HardwareDiagnostic_Status_STATUS_OK) {
        ORB_STATE_SET(ir_self, RET_SUCCESS);
    }

    return RET_SUCCESS;
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    // todo: implement for Diamond hardware
    // static const char *ir_leds_names[] = {
    //     "ir_850nm_center",
    //     "ir_850nm_side",
    //     "ir_940nm_left",
    //     "ir_940nm_right",
    // };
    return RET_SUCCESS;
#endif
}

int
ir_leds_safety_circuit_triggerd_internal(struct k_mutex *i2c1_mutex,
                                         const uint32_t timeout_ms,
                                         bool *triggered)
{
    int ret;

    /* protect i2c1 usage to check on pvcc from gpio expander with mutex */
    {
        if (i2c1_mutex == NULL) {
            return RET_ERROR_INVALID_STATE;
        }

        if (k_mutex_lock(i2c1_mutex, K_MSEC(timeout_ms)) != 0) {
            return RET_ERROR_BUSY;
        }

        ret = gpio_pin_get_dt(&front_unit_pvcc_enabled);

        // unlock asap
        k_mutex_unlock(i2c1_mutex);
    }

    if (ret < 0) {
        ASSERT_SOFT(ret);
        return ret;
    }

    const bool pvcc_enabled = (ret != 0);
    // update status on state change
    if (pvcc_enabled) {
        ret = ORB_STATE_SET(pvcc, RET_SUCCESS, "ir leds usable");
        ASSERT_SOFT(ret);
    } else {
        ret = ORB_STATE_SET(pvcc, RET_ERROR_OFFLINE, "ir leds unusable");
        ASSERT_SOFT(ret);
    }
    *triggered = !pvcc_enabled;

    return RET_SUCCESS;
}
