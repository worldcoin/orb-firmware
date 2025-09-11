#include "optics.h"
#include "optics/1d_tof/tof_1d.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/liquid_lens/liquid_lens.h"
#include "optics/mirror/mirror.h"
#include "optics/polarizer_wheel/polarizer_wheel.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <orb_state.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(optics, CONFIG_OPTICS_LOG_LEVEL);
ORB_STATE_REGISTER(pvcc);

static struct k_mutex *i2c1_mutex = NULL;

static int
ir_leds_safety_circuit_triggerd_internal(struct k_mutex *i2c1_mutex,
                                         const uint32_t timeout_ms,
                                         bool *triggered)
{
    // pin allows us to check whether PVCC is enabled on the front unit
    // PVCC might be disabled by hardware due to an intense usage of the IR LEDs
    // that doesn't respect the eye safety constraints
    static struct gpio_dt_spec front_unit_pvcc_enabled =
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                front_unit_pvcc_enabled_gpios, 0);

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

int
optics_safety_circuit_triggered(const uint32_t timeout_ms, bool *triggered)
{
    return ir_leds_safety_circuit_triggerd_internal(i2c1_mutex, timeout_ms,
                                                    triggered);
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
    LOG_INF("⚠️ Distance (1D ToF) is unsafe");
    ir_camera_system_set_on_time_us(0);
}

int
optics_init(const orb_mcu_Hardware *hw_version, struct k_mutex *mutex)
{
    int err_code;

    i2c1_mutex = mutex;

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

    err_code = tof_1d_init(distance_is_unsafe_cb, mutex, hw_version);
    ASSERT_SOFT(err_code);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    err_code = polarizer_wheel_init(hw_version);
    ASSERT_SOFT(err_code);
#endif

    const struct gpio_dt_spec front_unit_pvcc_enabled = GPIO_DT_SPEC_GET_BY_IDX(
        DT_PATH(zephyr_user), front_unit_pvcc_enabled_gpios, 0);
    err_code = gpio_pin_configure_dt(&front_unit_pvcc_enabled, GPIO_INPUT);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    // check pvcc (& update diag)
    bool safety_triggered;
    err_code = optics_safety_circuit_triggered(10, &safety_triggered);
    ASSERT_SOFT(err_code);
    LOG_INF("Safety circuitry triggered: %u", safety_triggered);
    UNUSED_VARIABLE(safety_triggered);

    return err_code;
}
