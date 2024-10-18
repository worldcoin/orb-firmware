#define IR_CAMERA_SYSTEM_INTERNAL_USE

#include <ir_camera_system.h>
#include <ir_camera_system_internal.h>
#include <ir_camera_timer_settings.h>
#include <utils.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_hw_init);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_enable_ir_eye_camera_hw);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_disable_ir_eye_camera_hw);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_enable_ir_face_camera_hw);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_disable_ir_face_camera_hw);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_enable_2d_tof_camera_hw);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_disable_2d_tof_camera_hw);
FAKE_VOID_FUNC(ir_camera_system_enable_leds_hw);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_set_fps_hw, uint16_t);
FAKE_VALUE_FUNC(ret_code_t, ir_camera_system_set_on_time_us_hw, uint16_t);

FAKE_VALUE_FUNC(uint32_t, ir_camera_system_get_time_until_update_us_internal);

FAKE_VOID_FUNC(ir_camera_system_set_polynomial_coefficients_for_focus_sweep_hw,
               orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial);
FAKE_VOID_FUNC(ir_camera_system_set_focus_values_for_focus_sweep_hw, int16_t *,
               size_t);
FAKE_VOID_FUNC(ir_camera_system_perform_focus_sweep_hw);
FAKE_VOID_FUNC(ir_camera_system_set_polynomial_coefficients_for_mirror_sweep_hw,
               orb_mcu_main_IREyeCameraMirrorSweepValuesPolynomial);
FAKE_VOID_FUNC(ir_camera_system_perform_mirror_sweep_hw);
FAKE_VALUE_FUNC(uint16_t, ir_camera_system_get_fps_hw);

extern bool ir_camera_system_initialized;
extern atomic_t focus_sweep_in_progress;
extern bool enabled_ir_eye_camera;
extern bool enabled_ir_face_camera;
extern bool enabled_2d_tof_camera;
extern orb_mcu_main_InfraredLEDs_Wavelength enabled_led_wavelength;

static void
before_each_test(void *fixture)
{
    ARG_UNUSED(fixture);

    // Reset module state
    ir_camera_system_initialized = false;
    atomic_set(&focus_sweep_in_progress, 0);
    enabled_ir_eye_camera = false;
    enabled_ir_face_camera = false;
    enabled_2d_tof_camera = false;
    enabled_led_wavelength =
        orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE;

    // Reset mock state
    RESET_FAKE(ir_camera_system_hw_init);

    RESET_FAKE(ir_camera_system_enable_ir_eye_camera_hw);
    RESET_FAKE(ir_camera_system_disable_ir_eye_camera_hw);

    RESET_FAKE(ir_camera_system_enable_ir_face_camera_hw);
    RESET_FAKE(ir_camera_system_disable_ir_face_camera_hw);

    RESET_FAKE(ir_camera_system_enable_2d_tof_camera_hw);
    RESET_FAKE(ir_camera_system_disable_2d_tof_camera_hw);

    RESET_FAKE(ir_camera_system_enable_leds_hw);
    RESET_FAKE(ir_camera_system_set_fps_hw);
    RESET_FAKE(ir_camera_system_set_on_time_us_hw);
    RESET_FAKE(ir_camera_system_get_time_until_update_us_internal);
    RESET_FAKE(ir_camera_system_set_polynomial_coefficients_for_focus_sweep_hw);
    RESET_FAKE(ir_camera_system_set_focus_values_for_focus_sweep_hw);
    RESET_FAKE(ir_camera_system_perform_focus_sweep_hw);
    RESET_FAKE(ir_camera_system_get_fps_hw);
}

ZTEST_SUITE(ir_camera_system_api, NULL, NULL, before_each_test, NULL, NULL);

ZTEST(ir_camera_system_api, test_empty) { zassert_true(1 == 1); }

ZTEST(ir_camera_system_api, test_init_fail)
{
    ret_code_t ret;
    extern bool ir_camera_system_initialized;

    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ret = ir_camera_system_init();
    zassert_equal(ret, RET_ERROR_INTERNAL);
    zassert_false(ir_camera_system_initialized);
}

ZTEST(ir_camera_system_api, test_init_fail_already_initialized)
{
    ret_code_t ret;
    extern bool ir_camera_system_initialized;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ret = ir_camera_system_init();
    zassert_equal(ret, RET_SUCCESS);
    zassert_true(ir_camera_system_initialized);
    // second call should fail with RET_ERROR_ALREADY_INITIALIZED
    ret = ir_camera_system_init();
    zassert_equal(ret, RET_ERROR_ALREADY_INITIALIZED);
    zassert_true(ir_camera_system_initialized);
}

ZTEST(ir_camera_system_api, test_init_success)
{
    ret_code_t ret;
    extern bool ir_camera_system_initialized;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ret = ir_camera_system_init();
    zassert_equal(ret, RET_SUCCESS);
    zassert_true(ir_camera_system_initialized);
}

// clang-format off
{% for camera_name in cameras %}
ZTEST(ir_camera_system_api, test_{{camera_name}}_enable_success)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_true(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_enable_fail_because_no_init)
// clang-format on
{
    ret_code_t ret;

    // Oops, we forgot to call init
    // ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    // ir_camera_system_init();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_enable_fail_because_init_failed)
// clang-format on
{
    ret_code_t ret;

    // Oops, init failed
    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ir_camera_system_init();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_enable_fail_because_focus_sweep_in_progress)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    set_focus_sweep_in_progress();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_BUSY);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_enable_success_because_focus_sweep_finished)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    set_focus_sweep_in_progress();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_BUSY);
    zassert_false(enabled_{{camera_name}});

    clear_focus_sweep_in_progress();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_true(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_disable_success)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_disable_fail_because_no_init)
// clang-format on
{
    ret_code_t ret;

    // Oops, we forgot to call init
    // ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    // ir_camera_system_init();

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_disable_fail_because_init_failed)
// clang-format on
{
    ret_code_t ret;

    // Oops, init failed
    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ir_camera_system_init();

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_disable_fail_because_focus_sweep_in_progress)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    set_focus_sweep_in_progress();

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_BUSY);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_disable_success_because_focus_sweep_finished)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    set_focus_sweep_in_progress();

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_ERROR_BUSY);
    zassert_false(enabled_{{camera_name}});

    clear_focus_sweep_in_progress();

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_disable_success_after_enable)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ir_camera_system_enable_{{camera_name}}();
    zassert_true(enabled_{{camera_name}});

    ret = ir_camera_system_disable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_false(enabled_{{camera_name}});
}

// clang-format off
ZTEST(ir_camera_system_api, test_{{camera_name}}_enable_success_after_disable)
// clang-format on
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_true(enabled_{{camera_name}});

    ir_camera_system_disable_{{camera_name}}();
    zassert_false(enabled_{{camera_name}});

    ret = ir_camera_system_enable_{{camera_name}}();
    zassert_equal(ret, RET_SUCCESS);
    zassert_true(enabled_{{camera_name}});
}

// clang-format off
{% endfor %}
// clang-format off

ZTEST(ir_camera_system_api, test_enable_wavelength_success)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    ret = ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    zassert_equal(ret, RET_SUCCESS);
    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
}

ZTEST(ir_camera_system_api, test_enable_wavelength_fail_because_no_init)
{
    ret_code_t ret;

    // Oops, we forgot to call init
    // ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    // ir_camera_system_init();

    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    ret = ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

ZTEST(ir_camera_system_api, test_enable_wavelength_fail_because_init_failed)
{
    ret_code_t ret;

    // Oops, init failed
    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ir_camera_system_init();

    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    ret = ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

ZTEST(ir_camera_system_api, test_enable_wavelength_fail_because_focus_sweep_in_progress)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    set_focus_sweep_in_progress();

    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    ret = ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    zassert_equal(ret, RET_ERROR_BUSY);
    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

ZTEST(ir_camera_system_api, test_get_enabled_wavelength)
{
    orb_mcu_main_InfraredLEDs_Wavelength w;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    w = ir_camera_system_get_enabled_leds();
    zassert_equal(w, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);

    ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    w = ir_camera_system_get_enabled_leds();
    zassert_equal(w, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);

    set_focus_sweep_in_progress();

    ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    // should be unchanged
    zassert_equal(enabled_led_wavelength, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
    w = ir_camera_system_get_enabled_leds();
    zassert_equal(w, orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE);
}

ZTEST(ir_camera_system_api, test_set_fps_success)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ir_camera_system_set_fps_hw_fake.return_val = RET_SUCCESS;
    for (int fps = 0; fps < IR_CAMERA_SYSTEM_MAX_FPS; ++fps) {
        ret = ir_camera_system_set_fps(fps);
        zassert_equal(ret, RET_SUCCESS);
    }
}

ZTEST(ir_camera_system_api, test_set_fps_fail_because_fps_out_of_range)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_fps(IR_CAMERA_SYSTEM_MAX_FPS + 1);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM);
}

ZTEST(ir_camera_system_api, test_set_fps_fail_because_no_init)
{
    ret_code_t ret;

    // Oops, we forgot to call init
    // ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    // ir_camera_system_init();

    ret = ir_camera_system_set_fps(1);
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
}

ZTEST(ir_camera_system_api, test_set_fps_fail_because_init_failed)
{
    ret_code_t ret;

    // Oops, init failed
    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ir_camera_system_init();

    ret = ir_camera_system_set_fps(1);
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
}

ZTEST(ir_camera_system_api, test_set_fps_fail_because_focus_sweep_in_progress)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_fps(1);
    zassert_equal(ret, RET_SUCCESS);

    set_focus_sweep_in_progress();

    ret = ir_camera_system_set_fps(2);
    zassert_equal(ret, RET_ERROR_BUSY);
}

ZTEST(ir_camera_system_api, test_set_fps_fail_because_hw_call_failed)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_fps(1);
    zassert_equal(ret, RET_SUCCESS);

    ir_camera_system_set_fps_hw_fake.return_val = RET_ERROR_INTERNAL;
    ret = ir_camera_system_set_fps(2);
    zassert_equal(ret, RET_ERROR_INTERNAL);
}

// Note: on-time can be manipulated while IR camera system is busy

ZTEST(ir_camera_system_api, test_set_on_time_success)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_on_time_us(100);
    zassert_equal(ret, RET_SUCCESS);
}

ZTEST(ir_camera_system_api, test_set_on_time_success_while_focus_sweep_in_progress)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    set_focus_sweep_in_progress();

    ret = ir_camera_system_set_on_time_us(100);
    zassert_equal(ret, RET_SUCCESS);
}

ZTEST(ir_camera_system_api, test_set_on_time_fail_because_no_init)
{
    ret_code_t ret;

    // Oops, we forgot to call init
    // ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    // ir_camera_system_init();

    ret = ir_camera_system_set_on_time_us(100);
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
}

ZTEST(ir_camera_system_api, test_set_on_time_fail_because_init_failed)
{
    ret_code_t ret;

    // Oops, init failed
    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ir_camera_system_init();

    ret = ir_camera_system_set_on_time_us(100);
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
}

ZTEST(ir_camera_system_api, test_set_on_time_fail_because_on_time_greater_than_max)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_on_time_us(IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US + 1);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM);
}

ZTEST(ir_camera_system_api, test_set_on_time_fail_because_hw_call_failed)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_on_time_us(1);
    zassert_equal(ret, RET_SUCCESS);

    ir_camera_system_set_on_time_us_hw_fake.return_val = RET_ERROR_INTERNAL;
    ret = ir_camera_system_set_on_time_us(2);
    zassert_equal(ret, RET_ERROR_INTERNAL);
}

ZTEST(ir_camera_system_api, test_set_focus_sweep_polynomial_coefficients_success)
{
    ret_code_t ret;
    orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial poly;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_polynomial_coefficients_for_focus_sweep(poly);
    zassert_equal(ret, RET_SUCCESS);
}

ZTEST(ir_camera_system_api, test_set_focus_sweep_polynomial_coefficients_fail_because_focus_sweep_in_progress)
{
    ret_code_t ret;
    orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial poly;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_polynomial_coefficients_for_focus_sweep(poly);
    zassert_equal(ret, RET_SUCCESS);

    set_focus_sweep_in_progress();

    ret = ir_camera_system_set_polynomial_coefficients_for_focus_sweep(poly);
    zassert_equal(ret, RET_ERROR_BUSY);
}

ZTEST(ir_camera_system_api, test_set_focus_sweep_focus_values_success)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_focus_values_for_focus_sweep(NULL, 0);
    zassert_equal(ret, RET_SUCCESS);
}

ZTEST(ir_camera_system_api, test_set_focus_sweep_focus_values_fail_because_focus_sweep_in_progress)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_focus_values_for_focus_sweep(NULL, 0);
    zassert_equal(ret, RET_SUCCESS);

    set_focus_sweep_in_progress();

    ret = ir_camera_system_set_focus_values_for_focus_sweep(NULL, 0);
    zassert_equal(ret, RET_ERROR_BUSY);
}

ZTEST(ir_camera_system_api, test_set_focus_sweep_focus_values_fail_because_too_many_values)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ret = ir_camera_system_set_focus_values_for_focus_sweep(NULL, MAX_NUMBER_OF_FOCUS_VALUES + 1);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM);
}

ZTEST(ir_camera_system_api, test_perform_focus_sweep_success)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ir_camera_system_get_fps_hw_fake.return_val = 1;

    ret = ir_camera_system_perform_focus_sweep();
    zassert_equal(ret, RET_SUCCESS);
}

ZTEST(ir_camera_system_api, test_perform_focus_sweep_fail_because_no_init)
{
    ret_code_t ret;

    // Oops, we forgot to call init
    // ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    // ir_camera_system_init();

    ir_camera_system_get_fps_hw_fake.return_val = 1;

    ret = ir_camera_system_perform_focus_sweep();
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
}

ZTEST(ir_camera_system_api, test_perform_focus_sweep_fail_because_init_failed)
{
    ret_code_t ret;

    // Oops, init failed
    ir_camera_system_hw_init_fake.return_val = RET_ERROR_INTERNAL;
    ir_camera_system_init();

    ir_camera_system_get_fps_hw_fake.return_val = 1;

    ret = ir_camera_system_perform_focus_sweep();
    zassert_equal(ret, RET_ERROR_NOT_INITIALIZED);
}

ZTEST(ir_camera_system_api, test_perform_focus_sweep_fail_because_fps_is_zero)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ir_camera_system_get_fps_hw_fake.return_val = 0;

    ret = ir_camera_system_perform_focus_sweep();
    zassert_equal(ret, RET_ERROR_INVALID_STATE);
}

ZTEST(ir_camera_system_api, test_perform_focus_sweep_fail_because_ir_eye_camera_is_enabled)
{
    ret_code_t ret;

    ir_camera_system_hw_init_fake.return_val = RET_SUCCESS;
    ir_camera_system_init();

    ir_camera_system_get_fps_hw_fake.return_val = 1;

    ret = ir_camera_system_perform_focus_sweep();
    zassert_equal(ret, RET_SUCCESS);

    ret = ir_camera_system_enable_ir_eye_camera();
    zassert_equal(ret, RET_SUCCESS);

    ret = ir_camera_system_perform_focus_sweep();
    zassert_equal(ret, RET_ERROR_INVALID_STATE);
}
