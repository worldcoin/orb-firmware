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

static struct k_mutex *i2c1_mutex = NULL;

int
ir_leds_safety_circuit_triggerd_internal(struct k_mutex *i2c1_mutex,
                                         const uint32_t timeout_ms,
                                         bool *triggered);

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
