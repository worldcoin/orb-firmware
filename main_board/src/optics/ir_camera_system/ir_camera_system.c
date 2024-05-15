#define IR_CAMERA_SYSTEM_INTERNAL_USE

#include "ir_camera_system.h"
#include "ir_camera_system_hw.h"
#include "ir_camera_system_internal.h"
#include "ir_camera_timer_settings.h"
#include "utils.h"

#if defined(CONFIG_ZTEST)
#include <zephyr/logging/log.h>
#else
#include "logs_can.h"
#endif

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util_macro.h>
LOG_MODULE_REGISTER(ir_camera_system, CONFIG_IR_CAMERA_SYSTEM_LOG_LEVEL);

/* Internal */

STATIC_OR_EXTERN atomic_t focus_sweep_in_progress;
STATIC_OR_EXTERN atomic_t mirror_sweep_in_progress;
STATIC_OR_EXTERN bool ir_camera_system_initialized;
STATIC_OR_EXTERN InfraredLEDs_Wavelength enabled_led_wavelength =
    InfraredLEDs_Wavelength_WAVELENGTH_NONE;

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
ir_camera_system_enable_leds(InfraredLEDs_Wavelength wavelength)
{
    ret_code_t ret;

    if (enabled_led_wavelength == wavelength) {
        LOG_DBG("wavelength not changed");
        return RET_SUCCESS;
    }

    ret = ir_camera_system_get_status();

    if (ret == RET_SUCCESS) {
        if (
#if defined(CONFIG_BOARD_PEARL_MAIN)
            wavelength ==
                InfraredLEDs_Wavelength_WAVELENGTH_740NM || // 740nm is
                                                            // deprecated
            wavelength == InfraredLEDs_Wavelength_WAVELENGTH_850NM_CENTER ||
            wavelength == InfraredLEDs_Wavelength_WAVELENGTH_850NM_SIDE ||
            wavelength == InfraredLEDs_Wavelength_WAVELENGTH_940NM_SINGLE
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
            wavelength ==
                InfraredLEDs_Wavelength_WAVELENGTH_740NM || // 740nm is
                                                            // deprecated
            wavelength == InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT ||
            wavelength == InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT
#else
            false
#endif
        ) {
            ret = RET_ERROR_INVALID_PARAM;
        } else {
            enabled_led_wavelength = wavelength;
            ir_camera_system_enable_leds_hw();
            ret = RET_SUCCESS;
        }
    }

    return ret;
}

InfraredLEDs_Wavelength
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

// Do not allow changing the FPS when a focus sweep is in progress.
ret_code_t
ir_camera_system_set_fps(uint16_t fps)
{
    ret_code_t ret;

    if (!ir_camera_system_initialized) {
        ret = RET_ERROR_NOT_INITIALIZED;
    } else if (fps > IR_CAMERA_SYSTEM_MAX_FPS) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if (get_focus_sweep_in_progress() == true) {
        ret = RET_ERROR_BUSY;
    } else {
        ret = ir_camera_system_set_fps_hw(fps);
    }

    return ret;
}

// When a focus sweep is in progress, we _do_ want to allow the on time to be
// changed, since the Jetson's auto exposure algorithm will still be running.
ret_code_t
ir_camera_system_set_on_time_us(uint16_t on_time_us)
{
    ret_code_t ret;

    if (!ir_camera_system_initialized) {
        ret = RET_ERROR_NOT_INITIALIZED;
    } else if (on_time_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        ret = RET_ERROR_INVALID_PARAM;
    } else {
        ret = ir_camera_system_set_on_time_us_hw(on_time_us);
    }

    return ret;
}

ret_code_t
ir_camera_system_set_polynomial_coefficients_for_focus_sweep(
    IREyeCameraFocusSweepValuesPolynomial poly)
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
    IREyeCameraMirrorSweepValuesPolynomial poly)
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

    if (!ir_camera_system_initialized) {
        ret = RET_ERROR_NOT_INITIALIZED;
    } else if (get_focus_sweep_in_progress() ||
               get_mirror_sweep_in_progress()) {
        ret = RET_ERROR_BUSY;
    } else {
        ret = RET_SUCCESS;
    }

    return ret;
}
