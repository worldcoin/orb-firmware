#include "ir_camera_system.h"
#include <app_config.h>
#include <can_messaging.h>
#include <pb_encode.h>
#include <runner/runner.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#if defined(CONFIG_ZTEST)
#include <zephyr/logging/log.h>
#else
#include "logs_can.h"
#endif
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

static void
send_msg(McuMessage *msg)
{
    can_message_t to_send;
    pb_ostream_t stream;
    ret_code_t ret;
    bool encoded_successfully;
    uint8_t buffer[CAN_FRAME_MAX_SIZE] = {0};

    stream = pb_ostream_from_buffer(buffer, sizeof buffer);
    encoded_successfully =
        pb_encode_ex(&stream, McuMessage_fields, msg, PB_ENCODE_DELIMITED);
    zassert_true(encoded_successfully);

    to_send.size = stream.bytes_written;
    to_send.bytes = buffer;
    to_send.destination = 0;

    ret = runner_handle_new_can(&to_send);
    zassert_equal(ret, RET_SUCCESS);
}

#define FOCUS_SWEEP_NUM_FRAMES 50
#define FOCUS_SWEEP_FPS        30
#define FOCUS_SWEEP_WAIT_TIME_MS                                               \
    ((uint32_t)((((float)(FOCUS_SWEEP_NUM_FRAMES) / FOCUS_SWEEP_FPS) * 1000) + \
                ((1.0f / (FOCUS_SWEEP_FPS) * 2))))

ZTEST(hil, test_ir_eye_camera_focus_sweep)
{
    McuMessage msg;

    // Stop triggering IR eye camera message
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload =
        JetsonToMcu_stop_triggering_ir_eye_camera_tag;

    send_msg(&msg);

    // Set FPS
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload = JetsonToMcu_fps_tag;
    msg.message.j_message.payload.fps.fps = FOCUS_SWEEP_FPS;

    send_msg(&msg);

    // Set on-time
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload = JetsonToMcu_led_on_time_tag;
    msg.message.j_message.payload.led_on_time.on_duration_us = 2500;

    // Set focus sweep polynomial
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload =
        JetsonToMcu_ir_eye_camera_focus_sweep_values_polynomial_tag;
    msg.message.j_message.payload.ir_eye_camera_focus_sweep_values_polynomial =
        (IREyeCameraFocusSweepValuesPolynomial){
            .coef_a = -120.0f,
            .coef_b = 4.5f,
            .coef_c = 0.045f,
            .coef_d = 0.00015f,
            .coef_e = 0.0f,
            .coef_f = 0.0f,
            .number_of_frames = FOCUS_SWEEP_NUM_FRAMES,
        };

    send_msg(&msg);

    // Perform focus sweep
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload =
        JetsonToMcu_perform_ir_eye_camera_focus_sweep_tag;

    extern struct k_sem camera_sweep_sem;

    k_sem_reset(&camera_sweep_sem);

    send_msg(&msg);

    int ret = k_sem_take(&camera_sweep_sem, K_MSEC(FOCUS_SWEEP_WAIT_TIME_MS));
    zassert_ok(ret, "Timed out! Waited for %ums", FOCUS_SWEEP_WAIT_TIME_MS);
    zassert_equal(ir_camera_system_get_status(), RET_SUCCESS);
}

#define MIRROR_SWEEP_NUM_FRAMES 100
#define MIRROR_SWEEP_FPS        30
#define MIRROR_SWEEP_WAIT_TIME_MS                                              \
    ((uint32_t)((((float)(MIRROR_SWEEP_NUM_FRAMES) / MIRROR_SWEEP_FPS) *       \
                 1000) +                                                       \
                1000))

ZTEST(hil, test_ir_eye_camera_mirror_sweep)
{
    McuMessage msg;

    // Stop triggering IR eye camera message
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload =
        JetsonToMcu_stop_triggering_ir_eye_camera_tag;
    send_msg(&msg);

    // Set FPS
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload = JetsonToMcu_fps_tag;
    msg.message.j_message.payload.fps.fps = MIRROR_SWEEP_FPS;
    send_msg(&msg);

    // Set on-time
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload = JetsonToMcu_led_on_time_tag;
    msg.message.j_message.payload.led_on_time.on_duration_us = 2500;
    send_msg(&msg);

    // Perform auto-homing
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload = JetsonToMcu_do_homing_tag;
    msg.message.j_message.payload.do_homing.homing_mode =
        PerformMirrorHoming_Mode_ONE_BLOCKING_END;
    msg.message.j_message.payload.do_homing.angle =
        PerformMirrorHoming_Angle_BOTH;
    send_msg(&msg);

    k_msleep(5000);

    // Set initial mirror position
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload = JetsonToMcu_mirror_angle_tag;
    msg.message.j_message.payload.mirror_angle.horizontal_angle = 52000;
    msg.message.j_message.payload.mirror_angle.vertical_angle = -9000;
    send_msg(&msg);

    k_msleep(1000);

    // Set mirror sweep polynomial
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload =
        JetsonToMcu_ir_eye_camera_mirror_sweep_values_polynomial_tag;
    msg.message.j_message.payload.ir_eye_camera_mirror_sweep_values_polynomial =
        (IREyeCameraMirrorSweepValuesPolynomial){
            .radius_coef_a = 1.0f,
            .radius_coef_b = 0.09f,
            .radius_coef_c = 0.0003f,
            .angle_coef_a = 10.0f,
            .angle_coef_b = 0.18849556f,
            .angle_coef_c = 0,
            .number_of_frames = MIRROR_SWEEP_NUM_FRAMES,
        };
    send_msg(&msg);

    // Perform mirror sweep
    msg = (McuMessage)McuMessage_init_zero;
    msg.version = Version_VERSION_0;
    msg.which_message = McuMessage_j_message_tag;
    msg.message.j_message.ack_number = 0;
    msg.message.j_message.which_payload =
        JetsonToMcu_perform_ir_eye_camera_mirror_sweep_tag;

    extern struct k_sem camera_sweep_sem;

    k_sem_reset(&camera_sweep_sem);

    send_msg(&msg);

    int ret = k_sem_take(&camera_sweep_sem, K_MSEC(MIRROR_SWEEP_WAIT_TIME_MS));
    zassert_ok(ret, "Timed out! Waited for %ums", MIRROR_SWEEP_WAIT_TIME_MS);
    zassert_equal(ir_camera_system_get_status(), RET_SUCCESS);
}

ZTEST(hil, test_ir_camera_sys_logic_analyzer)
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
