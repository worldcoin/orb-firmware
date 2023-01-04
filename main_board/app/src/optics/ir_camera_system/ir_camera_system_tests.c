#include "ir_camera_system.h"
#include <app_config.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ir_camera_system_tests_init);

// These tests are intended to be observed with a logic analyzer

#define PRINT_TEST_NAME() LOG_INF("Executing test '%s'", __func__)

#define SEPARATION_TIME_MS 1000

static void
test_camera_triggers(void)
{
    PRINT_TEST_NAME();

    ir_camera_system_set_fps(30);
    ir_camera_system_set_on_time_us(1000);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_ir_face_camera();
    ir_camera_system_enable_2d_tof_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_disable_ir_eye_camera();
    ir_camera_system_disable_ir_face_camera();
    ir_camera_system_disable_2d_tof_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_ir_eye_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_ir_face_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_2d_tof_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_disable_ir_eye_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_disable_ir_face_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_disable_2d_tof_camera();
}

static void
test_camera_triggers_with_fps_changing(void)
{
    PRINT_TEST_NAME();

    ir_camera_system_set_fps(30);
    ir_camera_system_set_on_time_us(10);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_ir_face_camera();
    ir_camera_system_enable_2d_tof_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(0);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(5);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(60);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(10);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(1);
    ir_camera_system_set_fps(5);
    ir_camera_system_set_fps(10);
    ir_camera_system_set_fps(20);

    ir_camera_system_disable_ir_eye_camera();
    ir_camera_system_disable_ir_face_camera();
    ir_camera_system_disable_2d_tof_camera();
}

static void
test_camera_triggers_with_fps_changing_and_cameras_enable_and_disable(void)
{
    PRINT_TEST_NAME();

    ir_camera_system_set_fps(30);
    ir_camera_system_set_on_time_us(1000);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_ir_eye_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(0);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(5);
    ir_camera_system_enable_ir_face_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(60);
    ir_camera_system_enable_2d_tof_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(10);
    ir_camera_system_disable_ir_eye_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(1);
    ir_camera_system_set_fps(5);
    ir_camera_system_set_fps(10);
    ir_camera_system_set_fps(20);

    ir_camera_system_disable_ir_eye_camera();
    ir_camera_system_disable_ir_face_camera();
    ir_camera_system_disable_2d_tof_camera();
}

static void
test_camera_triggers_and_leds_changing_fps(void)
{
    PRINT_TEST_NAME();

    // reset values
    ir_camera_system_set_fps(0);
    ir_camera_system_set_on_time_us(1000);

    // set FPS = 30
    ir_camera_system_set_fps(30);

    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM);

    k_msleep(SEPARATION_TIME_MS);

    // decrease FPS
    // on-time should still be valid
    ir_camera_system_set_fps(15);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_740NM);
    ir_camera_system_set_on_time_740nm_us(1000);

    k_msleep(SEPARATION_TIME_MS);

    // increase FPS to 50
    // on-time should still be valid
    ir_camera_system_set_fps(50);

    k_msleep(SEPARATION_TIME_MS);

    // decrease on-time duration
    // on-time should still be valid
    ir_camera_system_set_on_time_us(500);

    k_msleep(SEPARATION_TIME_MS);

    // increase on-time duration to 4000us
    // this should fail and no change should be observed in the output
    LOG_WRN("Setting next on-time value will fail");
    ir_camera_system_set_on_time_us(4000);

    k_msleep(SEPARATION_TIME_MS);

    // turn off
    ir_camera_system_set_fps(0);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(50);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_on_time_740nm_us(5000);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_disable_ir_eye_camera();

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_2d_tof_camera();
    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_ir_face_camera();

    k_msleep(SEPARATION_TIME_MS);

    // final, turn off everything at end of test
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
    ir_camera_system_disable_2d_tof_camera();
    ir_camera_system_disable_ir_eye_camera();
    ir_camera_system_disable_ir_face_camera();
}

static void
test_leds(void)
{
    PRINT_TEST_NAME();

    ir_camera_system_set_fps(30);
    ir_camera_system_set_on_time_us(1000);
    ir_camera_system_set_on_time_740nm_us(1000);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_850NM);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(
        InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM_LEFT);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(
        InfraredLEDs_Wavelength_WAVELENGTH_940NM_RIGHT);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(15);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(0);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_set_fps(30);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(
        InfraredLEDs_Wavelength_WAVELENGTH_940NM_RIGHT);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_740NM);

    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

static void (*tests[])(void) = {
    test_camera_triggers,
    test_camera_triggers_with_fps_changing,
    test_camera_triggers_with_fps_changing_and_cameras_enable_and_disable,
    test_camera_triggers_and_leds_changing_fps,
    test_leds,
};

ZTEST(runtime_tests_1, ir_camera_system)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_IR_CAMERA_SYSTEM);

    size_t i;
    for (i = 0; i < ARRAY_SIZE(tests); ++i) {
        LOG_INF("Executing test %d/%zu", i + 1, ARRAY_SIZE(tests));
        tests[i]();
        if (i != ARRAY_SIZE(tests) - 1) {
            k_msleep(5000);
        }
    }
}
