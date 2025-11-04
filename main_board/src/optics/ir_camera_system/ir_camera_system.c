#define IR_CAMERA_SYSTEM_INTERNAL_USE

#include "ir_camera_system.h"
#include "ir_camera_system_hw.h"
#include "ir_camera_system_internal.h"
#include "ir_camera_timer_settings.h"
#include "utils.h"

#if !defined(IR_CAMERA_UNIT_TESTS)
// include reference to function in case outside unit tests, otherwise
// mock them.
#include <optics/1d_tof/tof_1d.h>
#include <optics/optics.h>

#else
/** mocked function below for unit tests */
static bool
optics_safety_circuit_triggered(const uint32_t timeout_ms, bool *triggered)
{
    ARG_UNUSED(timeout_ms);
    *triggered = false;
    return 0;
}
#endif

#if CONFIG_ZTEST && !CONFIG_ZTEST_SHELL
#include <zephyr/logging/log.h>
#else
#include "orb_logs.h"
#endif

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util_macro.h>
LOG_MODULE_REGISTER(ir_camera_system, CONFIG_IR_CAMERA_SYSTEM_LOG_LEVEL);

/* Internal */

STATIC_OR_EXTERN atomic_t focus_sweep_in_progress;
STATIC_OR_EXTERN atomic_t mirror_sweep_in_progress;
STATIC_OR_EXTERN bool ir_camera_system_initialized;
STATIC_OR_EXTERN orb_mcu_main_InfraredLEDs_Wavelength enabled_led_wavelength =
    orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE;

#define MAKE_CAMERA_ENABLE_FUNC(camera_name)                                   \
    ret_code_t ir_camera_system_enable_##camera_name##_camera(void)            \
    {                                                                          \
        ret_code_t ret = RET_ERROR_NOT_INITIALIZED;                            \
                                                                               \
        if (!ir_camera_system_initialized) {                                   \
            ret = RET_ERROR_NOT_INITIALIZED;                                   \
        } else if (get_focus_sweep_in_progress() == true) {                    \
            ret = RET_ERROR_BUSY;                                              \
        } else {                                                               \
            enabled_##camera_name##_camera = true;                             \
            ir_camera_system_enable_##camera_name##_camera_hw();               \
            ret = RET_SUCCESS;                                                 \
        }                                                                      \
                                                                               \
        return ret;                                                            \
    }                                                                          \
    void ir_camera_system_enable_##camera_name##_camera_force(void)            \
    {                                                                          \
        enabled_##camera_name##_camera = true;                                 \
        ir_camera_system_enable_##camera_name##_camera_hw();                   \
    }

#define MAKE_CAMERA_DISABLE_FUNC(camera_name)                                  \
    ret_code_t ir_camera_system_disable_##camera_name##_camera(void)           \
    {                                                                          \
        ret_code_t ret = RET_ERROR_NOT_INITIALIZED;                            \
                                                                               \
        if (!ir_camera_system_initialized) {                                   \
            ret = RET_ERROR_NOT_INITIALIZED;                                   \
        } else if (get_focus_sweep_in_progress() == true) {                    \
            ret = RET_ERROR_BUSY;                                              \
        } else {                                                               \
            enabled_##camera_name##_camera = false;                            \
            ir_camera_system_disable_##camera_name##_camera_hw();              \
            ret = RET_SUCCESS;                                                 \
        }                                                                      \
                                                                               \
        return ret;                                                            \
    }                                                                          \
    void ir_camera_system_disable_##camera_name##_camera_force(void)           \
    {                                                                          \
        enabled_##camera_name##_camera = false;                                \
        ir_camera_system_disable_##camera_name##_camera_hw();                  \
    }

#define MAKE_CAMERA_IS_ENABLED_FUNC(camera_name)                               \
    bool ir_camera_system_##camera_name##_camera_is_enabled(void)              \
    {                                                                          \
        return enabled_##camera_name##_camera;                                 \
    }

#define MAKE_CAMERA_ENABLE_DISABLE_GET_FUNCTIONS(camera_name)                  \
    STATIC_OR_EXTERN bool enabled_##camera_name##_camera;                      \
    MAKE_CAMERA_ENABLE_FUNC(camera_name)                                       \
    MAKE_CAMERA_DISABLE_FUNC(camera_name)                                      \
    MAKE_CAMERA_IS_ENABLED_FUNC(camera_name)

/* Module internal */

#ifdef CONFIG_ZTEST
/**
 * Cleanup function for the test suite, called before & after each ir_camera
 * test
 * @param fixture
 */
void
ir_camera_test_reset(void *fixture)
{
    ARG_UNUSED(fixture);
    ir_camera_system_enable_leds(
        orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    ir_camera_system_set_fps(0);
    ir_camera_system_set_on_time_us(0);
}
#endif

// Focus sweep
bool
get_focus_sweep_in_progress(void)
{
    return atomic_get(&focus_sweep_in_progress);
}

void
set_focus_sweep_in_progress(void)
{
    atomic_set(&focus_sweep_in_progress, 1);
}

void
clear_focus_sweep_in_progress(void)
{
    atomic_set(&focus_sweep_in_progress, 0);
}

// Mirror sweep
bool
get_mirror_sweep_in_progress(void)
{
    return atomic_get(&mirror_sweep_in_progress);
}

void
set_mirror_sweep_in_progress(void)
{
    atomic_set(&mirror_sweep_in_progress, 1);
}

void
clear_mirror_sweep_in_progress(void)
{
    atomic_set(&mirror_sweep_in_progress, 0);
}

/* Public */

ret_code_t
ir_camera_system_init(void)
{
    ret_code_t ret;

    if (ir_camera_system_initialized) {
        LOG_WRN("IR camera system already initialized");
        ret = RET_ERROR_ALREADY_INITIALIZED;
    } else {
        ret = ir_camera_system_hw_init();

        if (ret == RET_SUCCESS) {
            ir_camera_system_initialized = true;
        }
    }

    return ret;
}

MAKE_CAMERA_ENABLE_DISABLE_GET_FUNCTIONS(ir_eye)
MAKE_CAMERA_ENABLE_DISABLE_GET_FUNCTIONS(ir_face)
MAKE_CAMERA_ENABLE_DISABLE_GET_FUNCTIONS(2d_tof)

ret_code_t
ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength wavelength)
{
    ret_code_t ret = RET_SUCCESS;

    if (wavelength != orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE) {
        ret = ir_camera_system_get_status();
    }

    if (ret == RET_SUCCESS) {
        // check for deprecated wavelength first
        if (
#if defined(CONFIG_BOARD_PEARL_MAIN)
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_740NM ||
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_850NM_CENTER ||
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_850NM_SIDE ||
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_940NM_SINGLE
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_740NM ||
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT ||
            wavelength ==
                orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT
#else
            false
#endif
        ) {
            ret = RET_ERROR_INVALID_PARAM;
        } else {
            if (enabled_led_wavelength != wavelength) {
                enabled_led_wavelength = wavelength;
                ir_camera_system_enable_leds_hw();
            }
            ret = RET_SUCCESS;
        }
    }

    return ret;
}

orb_mcu_main_InfraredLEDs_Wavelength
ir_camera_system_get_enabled_leds(void)
{
    return enabled_led_wavelength;
}

uint32_t
ir_camera_system_get_time_until_update_us(void)
{
    return ir_camera_system_get_time_until_update_us_internal();
}

uint16_t
ir_camera_system_get_fps(void)
{
    return ir_camera_system_get_fps_hw();
}

ret_code_t
ir_camera_system_set_fps(uint16_t fps)
{
    ret_code_t ret = RET_SUCCESS;

    if (fps > IR_CAMERA_SYSTEM_MAX_FPS) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if (fps != 0) {
        ret = ir_camera_system_get_status();
    }

    if (ret == RET_SUCCESS) {
        ret = ir_camera_system_set_fps_hw(fps);
    }

    return ret;
}

ret_code_t
ir_camera_system_set_on_time_us(uint16_t on_time_us)
{
    ret_code_t ret = RET_SUCCESS;

    if (on_time_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if (on_time_us != 0) {
        ret = ir_camera_system_get_status();

        // When a focus sweep is in progress, we _do_ want to allow the on time
        // to be changed, since the Jetson's auto exposure algorithm will still
        // be running.
        if (ret == RET_ERROR_BUSY) {
            ret = RET_SUCCESS;
        }
    }

    if (ret == RET_SUCCESS) {
        ret = ir_camera_system_set_on_time_us_hw(on_time_us);
    }

    return ret;
}

ret_code_t
ir_camera_system_set_polynomial_coefficients_for_focus_sweep(
    orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial poly)
{
    ret_code_t ret;

    if (get_focus_sweep_in_progress() == true) {
        ret = RET_ERROR_BUSY;
    } else {
        ir_camera_system_set_polynomial_coefficients_for_focus_sweep_hw(poly);
        ret = RET_SUCCESS;
    }

    return ret;
}

ret_code_t
ir_camera_system_set_focus_values_for_focus_sweep(int16_t *focus_values,
                                                  size_t num_focus_values)
{
    ret_code_t ret;

    if (num_focus_values > MAX_NUMBER_OF_FOCUS_VALUES) {
        LOG_ERR("Too many focus values!");
        ret = RET_ERROR_INVALID_PARAM;
    } else if (get_focus_sweep_in_progress() == true) {
        ret = RET_ERROR_BUSY;
    } else {
        ir_camera_system_set_focus_values_for_focus_sweep_hw(focus_values,
                                                             num_focus_values);
        ret = RET_SUCCESS;
    }

    return ret;
}

ret_code_t
ir_camera_system_perform_focus_sweep(void)
{
    ret_code_t ret;

    ret = ir_camera_system_get_status();

    if (ret == RET_SUCCESS) {
        if (ir_camera_system_get_fps_hw() == 0) {
            LOG_ERR("FPS must be greater than 0!");
            ret = RET_ERROR_INVALID_STATE;
        } else if (ir_camera_system_ir_eye_camera_is_enabled()) {
            LOG_ERR("IR eye camera must be disabled!");
            ret = RET_ERROR_INVALID_STATE;
        } else {
            ir_camera_system_perform_focus_sweep_hw();
            ret = RET_SUCCESS;
        }
    } else {
        LOG_ERR("Focus sweep not performed, status: %d", ret);
    }

    return ret;
}

ret_code_t
ir_camera_system_set_polynomial_coefficients_for_mirror_sweep(
    orb_mcu_main_IREyeCameraMirrorSweepValuesPolynomial poly)
{
    ret_code_t ret;

    if (get_mirror_sweep_in_progress() == true) {
        ret = RET_ERROR_BUSY;
    } else {
        ir_camera_system_set_polynomial_coefficients_for_mirror_sweep_hw(poly);
        ret = RET_SUCCESS;
    }

    return ret;
}

ret_code_t
ir_camera_system_perform_mirror_sweep(void)
{
    ret_code_t ret;

    ret = ir_camera_system_get_status();

    if (ret == RET_SUCCESS) {
        if (ir_camera_system_get_fps_hw() == 0) {
            LOG_ERR("FPS must be greater than 0!");
            ret = RET_ERROR_INVALID_STATE;
        } else if (ir_camera_system_ir_eye_camera_is_enabled()) {
            LOG_ERR("IR eye camera must be disabled!");
            ret = RET_ERROR_INVALID_STATE;
        } else {
            ir_camera_system_perform_mirror_sweep_hw();
            ret = RET_SUCCESS;
        }
    }

    return ret;
}

ret_code_t
ir_camera_system_get_status(void)
{
    ret_code_t ret;

    /* it's fine to discard returned value when checking pvcc state
     * because even if IR LEDs are mistakenly enabled, the circuitry won't
     * power them.
     * The user won't be informed though with a `forbidden` error code
     */
    bool safety_triggered = false;
    optics_safety_circuit_triggered(0, &safety_triggered);

    /* ⚠️ ordered by level of importance as it's fine to set a new ir-led
     * on duration during a sweep (busy) but not if unsafe conditions are met */
    if (!ir_camera_system_initialized) {
        ret = RET_ERROR_NOT_INITIALIZED;
    } else if (safety_triggered) {
        ret = RET_ERROR_FORBIDDEN;
    } else if (get_focus_sweep_in_progress() ||
               get_mirror_sweep_in_progress()) {
        ret = RET_ERROR_BUSY;
    } else {
        ret = RET_SUCCESS;
    }

    return ret;
}
