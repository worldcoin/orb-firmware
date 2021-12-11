#include "ir_camera_system.h"
#include <app_config.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(ir_camera_system_test);

// These tests are intended to be observed with a logic analyzer

static K_THREAD_STACK_DEFINE(ir_camera_system_test_stack_area,
                             THREAD_STACK_SIZE_IR_CAMERA_SYSTEM_TEST);
struct k_thread ir_camera_system_thread_data;

static void
test_camera_triggers(void)
{
    ir_camera_system_set_fps(30);

    k_msleep(1000);

    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_ir_face_camera();
    ir_camera_system_enable_2d_tof_camera();

    k_msleep(1000);

    ir_camera_system_disable_ir_eye_camera();
    ir_camera_system_disable_ir_face_camera();
    ir_camera_system_disable_2d_tof_camera();

    k_msleep(1000);

    ir_camera_system_enable_ir_eye_camera();

    k_msleep(1000);

    ir_camera_system_enable_ir_face_camera();

    k_msleep(1000);

    ir_camera_system_disable_ir_eye_camera();

    k_msleep(1000);

    ir_camera_system_disable_ir_face_camera();
}

static void
test_camera_triggers_with_fps_changing(void)
{
    ir_camera_system_set_fps(30);

    k_msleep(1000);

    ir_camera_system_enable_ir_eye_camera();
    ir_camera_system_enable_ir_face_camera();
    ir_camera_system_enable_2d_tof_camera();

    k_msleep(1000);

    ir_camera_system_set_fps(0);

    k_msleep(1000);

    ir_camera_system_set_fps(5);

    k_msleep(1000);

    ir_camera_system_set_fps(60);

    k_msleep(1000);

    ir_camera_system_set_fps(10);

    k_msleep(1000);

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
    LOG_INF("Executing test %s", __func__);

    ir_camera_system_set_fps(30);

    ir_camera_system_enable_ir_face_camera();
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM);

    k_msleep(1000);

    ir_camera_system_set_fps(15);

    k_msleep(1000);

    ir_camera_system_set_fps(0);

    k_msleep(1000);

    ir_camera_system_set_fps(30);

    k_msleep(1000);

    ir_camera_system_disable_ir_face_camera();
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

static void
test_leds(void)
{
    LOG_INF("Executing test %s", __func__);
    ir_camera_system_set_fps(30);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_850NM);

    k_msleep(1000);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT);

    k_msleep(1000);

    ir_camera_system_enable_leds(
        InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT);

    k_msleep(1000);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM);

    k_msleep(1000);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_940NM_LEFT);

    k_msleep(1000);

    ir_camera_system_enable_leds(
        InfraredLEDs_Wavelength_WAVELENGTH_940NM_RIGHT);

    k_msleep(1000);

    ir_camera_system_set_fps(15);

    k_msleep(1000);

    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

static void (*tests[])(void) = {
    test_camera_triggers, test_camera_triggers_with_fps_changing,
    test_camera_triggers_and_leds_changing_fps, test_leds};

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
ir_camera_system_test(void)
{
    k_thread_create(&ir_camera_system_thread_data,
                    ir_camera_system_test_stack_area,
                    K_THREAD_STACK_SIZEOF(ir_camera_system_test_stack_area),
                    thread_entry_point, NULL, NULL, NULL,
                    THREAD_PRIORITY_IR_CAMERA_SYSTEM_TEST, 0, K_NO_WAIT);
}
