#include "incoming_message_handling.h"
#include "messaging.h"
#include <assert.h>
#include <fan/fan.h>
#include <front_unit_rgb_leds/front_unit_rgb_leds.h>
#include <ir_camera_system/ir_camera_system.h>
#include <logging/log.h>
#include <stepper_motors/stepper_motors.h>
#include <temperature/temperature.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(incoming_message_handling);

#define MAKE_ASSERTS(tag)                                                      \
    __ASSERT_NO_MSG(msg->which_message == McuMessage_j_message_tag);           \
    __ASSERT_NO_MSG(msg->message.j_message.which_payload == tag);

static inline uint32_t
get_ack_num(McuMessage *msg)
{
    return msg->message.j_message.ack_number;
}

static void
send_ack(Ack_ErrorCode error, uint32_t ack_number)
{
    McuMessage ack = {.which_message = McuMessage_m_message_tag,
                      .message.m_message.which_payload = McuToJetson_ack_tag,
                      .message.m_message.payload.ack.ack_number = ack_number,
                      .message.m_message.payload.ack.error = error};
    messaging_push_tx(&ack);
}

static void
handle_infrared_leds_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_infrared_leds_tag);

    InfraredLEDs_Wavelength wavelength =
        msg->message.j_message.payload.infrared_leds.wavelength;

    LOG_DBG("Got LED wavelength message = %d", wavelength);
    ir_camera_system_enable_leds(wavelength);
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_led_on_time_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_led_on_time_tag);

    uint32_t on_time_us =
        msg->message.j_message.payload.led_on_time.on_duration_us;

    LOG_DBG("Got LED on time message = %uus", on_time_us);
    send_ack(ir_camera_system_set_on_time_us(on_time_us), get_ack_num(msg));
}

static void
handle_start_triggering_ir_eye_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_eye_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_eye_camera();
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_stop_triggering_ir_eye_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_eye_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_eye_camera();
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_start_triggering_ir_face_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_face_camera();
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_stop_triggering_ir_face_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_face_camera();
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_start_triggering_2dtof_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_2d_tof_camera();
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_stop_triggering_2dtof_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_2d_tof_camera();
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_740nm_brightness_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_brightness_740nm_leds_tag);

    uint32_t brightness =
        msg->message.j_message.payload.brightness_740nm_leds.brightness;

    LOG_DBG("");
    send_ack(ir_camera_system_set_740nm_led_brightness(brightness),
             get_ack_num(msg));
}

static void
handle_mirror_angle_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_mirror_angle_tag);

    uint32_t horizontal_angle =
        msg->message.j_message.payload.mirror_angle.horizontal_angle;
    int32_t vertical_angle =
        msg->message.j_message.payload.mirror_angle.vertical_angle;

    if (horizontal_angle > MOTORS_ANGLE_HORIZONTAL_MAX ||
        horizontal_angle < MOTORS_ANGLE_HORIZONTAL_MIN) {
        LOG_ERR("Horizontal angle of %u out of range [%u;%u]", horizontal_angle,
                MOTORS_ANGLE_HORIZONTAL_MIN, MOTORS_ANGLE_HORIZONTAL_MAX);
        send_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }

    if (vertical_angle > MOTORS_ANGLE_VERTICAL_MAX ||
        vertical_angle < MOTORS_ANGLE_VERTICAL_MIN) {
        LOG_ERR("Vertical angle of %d out of range [%d;%d]", vertical_angle,
                MOTORS_ANGLE_VERTICAL_MIN, MOTORS_ANGLE_VERTICAL_MAX);
        send_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }

    LOG_DBG("Got mirror angle message, vert: %d, horiz: %u", vertical_angle,
            horizontal_angle);

    if (motors_angle_horizontal(horizontal_angle) != RET_SUCCESS) {
        send_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else if (motors_angle_vertical(vertical_angle) != RET_SUCCESS) {
        send_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else {
        send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_temperature_sample_period_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_temperature_sample_period_tag);

    uint32_t sample_period_ms = msg->message.j_message.payload
                                    .temperature_sample_period.sample_period_ms;

    LOG_DBG("Got new temperature sampling period: %ums", sample_period_ms);

    temperature_set_sampling_period_ms(sample_period_ms);
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_fan_speed(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_fan_speed_tag);

    uint32_t fan_speed_percentage =
        msg->message.j_message.payload.fan_speed.percentage;

    if (fan_speed_percentage > 100) {
        LOG_ERR("Got fan speed of %u out of range [0;100]",
                fan_speed_percentage);
        send_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        LOG_DBG("Got fan speed message: %u%%", fan_speed_percentage);

        fan_set_speed(fan_speed_percentage);
        send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_user_leds_pattern(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_user_leds_pattern_tag);

    UserLEDsPattern_UserRgbLedPattern pattern =
        msg->message.j_message.payload.user_leds_pattern.pattern;

    LOG_DBG("Got new user RBG pattern message: %d", pattern);

    front_unit_rgb_leds_set_pattern(pattern);
    send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_user_leds_brightness(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_user_leds_brightness_tag);

    uint32_t brightness =
        msg->message.j_message.payload.user_leds_brightness.brightness;

    if (brightness > 255) {
        LOG_ERR("Got user LED brightness value of %u out of range [0,255]",
                brightness);
        send_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        LOG_DBG("Got user LED brightness value of %u", brightness);
        front_unit_rgb_leds_set_brightness(brightness);
        send_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

typedef void (*hm_callback)(McuMessage *msg);

// These functions ARE NOT allowed to block!
static const hm_callback handle_message_callbacks[] = {
    [JetsonToMcu_infrared_leds_tag] = handle_infrared_leds_message,
    [JetsonToMcu_led_on_time_tag] = handle_led_on_time_message,
    [JetsonToMcu_start_triggering_ir_eye_camera_tag] =
        handle_start_triggering_ir_eye_camera_message,
    [JetsonToMcu_stop_triggering_ir_eye_camera_tag] =
        handle_stop_triggering_ir_eye_camera_message,
    [JetsonToMcu_start_triggering_ir_face_camera_tag] =
        handle_start_triggering_ir_face_camera_message,
    [JetsonToMcu_stop_triggering_ir_face_camera_tag] =
        handle_stop_triggering_ir_face_camera_message,
    [JetsonToMcu_start_triggering_2dtof_camera_tag] =
        handle_start_triggering_2dtof_camera_message,
    [JetsonToMcu_stop_triggering_2dtof_camera_tag] =
        handle_stop_triggering_2dtof_camera_message,
    [JetsonToMcu_brightness_740nm_leds_tag] = handle_740nm_brightness_message,
    [JetsonToMcu_mirror_angle_tag] = handle_mirror_angle_message,
    [JetsonToMcu_temperature_sample_period_tag] =
        handle_temperature_sample_period_message,
    [JetsonToMcu_fan_speed_tag] = handle_fan_speed,
    [JetsonToMcu_user_leds_pattern_tag] = handle_user_leds_pattern,
    [JetsonToMcu_user_leds_brightness_tag] = handle_user_leds_brightness};

static_assert(
    ARRAY_SIZE(handle_message_callbacks) <= 30,
    "It seems like the `handle_message_callbacks` array is too large");

void
handle_incoming_message(McuMessage *msg)
{
    if (msg->which_message != McuMessage_j_message_tag) {
        LOG_INF("Got message not intended for main MCU. Dropping.");
        return;
    }

    LOG_DBG("Got a message with payload ID %d",
            msg->message.j_message.which_payload);

    if (msg->message.j_message.which_payload <
            ARRAY_SIZE(handle_message_callbacks) &&
        handle_message_callbacks[msg->message.j_message.which_payload] !=
            NULL) {
        handle_message_callbacks[msg->message.j_message.which_payload](msg);
    } else {
        LOG_ERR(
            "A handler for message with a payload ID of %d is not implemented",
            msg->message.j_message.which_payload);
        send_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED, get_ack_num(msg));
    }
}
