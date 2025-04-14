#include "optics.h"
#include "optics/1d_tof/tof_1d.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/liquid_lens/liquid_lens.h"
#include "optics/mirror/mirror.h"
#include "orb_logs.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "system/diag.h"
#include <app_assert.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(optics);

static orb_mcu_HardwareDiagnostic_Status self_test_status =
    orb_mcu_HardwareDiagnostic_Status_STATUS_UNKNOWN;

// pin allows us to check whether PVCC is enabled on the front unit
// PVCC might be disabled by hardware due to an intense usage of the IR LEDs
// that doesn't respect the eye safety constraints
static struct gpio_dt_spec front_unit_pvcc_enabled = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), front_unit_pvcc_enabled_gpios, 0);

bool
optics_safety_circuit_triggered(void)
{
    int ret = gpio_pin_get_dt(&front_unit_pvcc_enabled);
    ASSERT_SOFT_BOOL(ret >= 0);

    bool pvcc_enabled = (ret >= 0 && ret != 0);

    // update status
    if (pvcc_enabled) {
        diag_set_status(orb_mcu_HardwareDiagnostic_Source_OPTICS_IR_LEDS,
                        orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    } else {
        diag_set_status(orb_mcu_HardwareDiagnostic_Source_OPTICS_IR_LEDS,
                        orb_mcu_HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE);
    }

    return !pvcc_enabled;
}

/**
 * This callback is called by the 1D ToF sensor if
 * the distance is *not* safe for IR-LEDs to be turned on
 * The on-time is set to 0 because it can be changed in any ir-camera
 * state.
 * Any new value being set in the IR camera system will
 * check the distance safety so this function only serves
 * in case the ir-leds are not actuated anymore.
 */
__maybe_unused static void
distance_is_unsafe_cb(void)
{
    ir_camera_system_set_on_time_us(0);
}

int
optics_init(const orb_mcu_Hardware *hw_version, struct k_mutex *mutex)
{
    int err_code;

    diag_set_status(
        orb_mcu_HardwareDiagnostic_Source_OPTICS_EYE_SAFETY_CIRCUIT_SELF_TEST,
        self_test_status);

    err_code = ir_camera_system_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = mirror_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = liquid_lens_init(hw_version);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

#if PROXIMITY_DETECTION_FOR_IR_SAFETY
    err_code = tof_1d_init(distance_is_unsafe_cb, mutex);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }
#else
    UNUSED_PARAMETER(mutex);
#endif

    err_code = gpio_pin_configure_dt(&front_unit_pvcc_enabled, GPIO_INPUT);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    // check pvcc (& update diag)
    bool safety_triggered = optics_safety_circuit_triggered();
    LOG_INF("Safety circuitry triggered: %u", safety_triggered);
    UNUSED_VARIABLE(safety_triggered);

    return err_code;
}

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

    static const char *ir_leds_names[] = {
        "ir_850nm_left",
        "ir_850nm_right",
        "ir_940nm_left",
        "ir_940nm_right",
    };

    int ret;

    if (self_test_status == orb_mcu_HardwareDiagnostic_Status_STATUS_OK ||
        self_test_status ==
            orb_mcu_HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE) {
        return RET_ERROR_ALREADY_INITIALIZED;
    }

    // turn on IR LED subsets one by one, by driving GPIO pins, to check
    // that all lines are making the eye safety circuitry trip
    ret = gpio_pin_configure_dt(
        &front_unit_pvcc_enabled,
        GPIO_INPUT); // todo: on Diamond we need to enable +3V3 before reading
                     // the Front Unit IO expander
    if (ret) {
        self_test_status =
            orb_mcu_HardwareDiagnostic_Status_STATUS_INITIALIZATION_ERROR;
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    self_test_status = orb_mcu_HardwareDiagnostic_Status_STATUS_OK;
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
            self_test_status =
                orb_mcu_HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE;
        } else {
            LOG_INF("%s tripped safety circuitry", ir_leds_names[i]);
        }

        // eye safety circuitry to reset
        power_vbat_5v_3v3_supplies_off();
        k_msleep(50);
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
