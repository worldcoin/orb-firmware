#include "ir_camera_system.h"
#include <app_config.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(ir_camera_system_test);

// These tests are intended to be observed with a logic analyzer

static K_THREAD_STACK_DEFINE(ir_camera_system_test_stack_area,
                             THREAD_STACK_SIZE_IR_CAMERA_SYSTEM_TEST);
struct k_thread ir_camera_system_thread_data;

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
    // on-time duration = 3333us
    ir_camera_system_set_fps(30);

    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM);

    k_msleep(SEPARATION_TIME_MS);

    // decrease FPS
    // on-time should still be valid
    ir_camera_system_set_fps(15);
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
    ir_camera_system_set_on_time_us(4000);
    k_msleep(SEPARATION_TIME_MS);

    // turn off
    ir_camera_system_set_fps(0);
    k_msleep(SEPARATION_TIME_MS);

    ir_camera_system_disable_ir_eye_camera();
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

static void
test_leds(void)
{
    PRINT_TEST_NAME();

    ir_camera_system_set_fps(30);
    ir_camera_system_set_on_time_us(1000);

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

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

static void (*tests[])(void) = {
    test_camera_triggers,
    test_camera_triggers_with_fps_changing,
    test_camera_triggers_with_fps_changing_and_cameras_enable_and_disable,
    test_camera_triggers_and_leds_changing_fps,
    test_leds,
};

static void
thread_entry_point(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    LOG_INF("Begin tests");

    size_t i;
    for (i = 0; i < ARRAY_SIZE(tests); ++i) {
        LOG_INF("Executing test %d/%lu", i + 1, ARRAY_SIZE(tests));
        tests[i]();
        if (i != ARRAY_SIZE(tests) - 1) {
            k_msleep(5000);
        }
    }

    LOG_INF("Tests complete");
}

void
ir_camera_system_test_async(void)
{
    k_thread_create(&ir_camera_system_thread_data,
                    ir_camera_system_test_stack_area,
                    K_THREAD_STACK_SIZEOF(ir_camera_system_test_stack_area),
                    thread_entry_point, NULL, NULL, NULL,
                    THREAD_PRIORITY_IR_CAMERA_SYSTEM_TEST, 0, K_NO_WAIT);
}

void
ir_camera_system_test(void)
{
    thread_entry_point(NULL, NULL, NULL);
}
