#include "incoming_message_handling.h"
#include "can_messaging.h"
#include "dfu.h"
#include "mcu_messaging.pb.h"
#include "power_sequence/power_sequence.h"
#include "ui/distributor_leds/distributor_leds.h"
#include "version/version.h"
#include <assert.h>
#include <fan/fan.h>
#include <heartbeat.h>
#include <ir_camera_system/ir_camera_system.h>
#include <liquid_lens/liquid_lens.h>
#include <logging/log.h>
#include <stdlib.h>
#include <stepper_motors/stepper_motors.h>
#include <temperature/temperature.h>
#include <ui/front_leds/front_leds.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(incoming_message_handling);

static K_THREAD_STACK_DEFINE(auto_homing_stack, 600);
static struct k_thread auto_homing_thread;
static k_tid_t auto_homing_tid = NULL;

#define MAKE_ASSERTS(tag)                                                      \
    __ASSERT_NO_MSG(msg->which_message == McuMessage_j_message_tag);           \
    __ASSERT_NO_MSG(msg->message.j_message.which_payload == tag);

static uint32_t message_counter = 0;

static inline uint32_t
get_ack_num(McuMessage *msg)
{
    return msg->message.j_message.ack_number;
}

uint32_t
incoming_message_acked_counter(void)
{
    return message_counter;
}

void
incoming_message_ack(Ack_ErrorCode error, uint32_t ack_number)
{
    McuMessage ack = {.which_message = McuMessage_m_message_tag,
                      .message.m_message.which_payload = McuToJetson_ack_tag,
                      .message.m_message.payload.ack.ack_number = ack_number,
                      .message.m_message.payload.ack.error = error};
    can_messaging_push_tx(&ack);
    ++message_counter;
}

/// Convert error codes to ack codes
static void
handle_err_code(uint32_t ack_number, int err)
{
    switch (err) {
    case RET_SUCCESS:
        incoming_message_ack(Ack_ErrorCode_SUCCESS, ack_number);
        break;

    case RET_ERROR_INVALID_PARAM:
    case RET_ERROR_NOT_FOUND:
        incoming_message_ack(Ack_ErrorCode_RANGE, ack_number);
        break;

    case RET_ERROR_BUSY:
    case RET_ERROR_INVALID_STATE:
        incoming_message_ack(Ack_ErrorCode_IN_PROGRESS, ack_number);
        break;

    case RET_ERROR_FORBIDDEN:
        incoming_message_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED, ack_number);
        break;

    default:
        incoming_message_ack(Ack_ErrorCode_FAIL, ack_number);
    }
}

void
auto_homing_thread_entry_point(void *a, void *b, void *c)
{
    ARG_UNUSED(c);

    PerformMirrorHoming_Mode mode = (PerformMirrorHoming_Mode)a;
    PerformMirrorHoming_Mirror mirror = (PerformMirrorHoming_Mirror)b;

    ret_code_t ret;
    struct k_thread *horiz = NULL, *vert = NULL;

    if (mode == PerformMirrorHoming_Mode_STALL_DETECTION) {
        if (mirror == PerformMirrorHoming_Mirror_BOTH ||
            mirror == PerformMirrorHoming_Mirror_HORIZONTAL) {
            ret = motors_auto_homing_stall_detection(MOTOR_HORIZONTAL, &horiz);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
        if (mirror == PerformMirrorHoming_Mirror_BOTH ||
            mirror == PerformMirrorHoming_Mirror_VERTICAL) {
            ret = motors_auto_homing_stall_detection(MOTOR_VERTICAL, &vert);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
    } else if (mode == PerformMirrorHoming_Mode_ONE_BLOCKING_END) {
        if (mirror == PerformMirrorHoming_Mirror_BOTH ||
            mirror == PerformMirrorHoming_Mirror_HORIZONTAL) {
            ret = motors_auto_homing_one_end(MOTOR_HORIZONTAL, &horiz);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
        if (mirror == PerformMirrorHoming_Mirror_BOTH ||
            mirror == PerformMirrorHoming_Mirror_VERTICAL) {
            ret = motors_auto_homing_one_end(MOTOR_VERTICAL, &vert);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
    }

    if (horiz != NULL) {
        k_thread_join(horiz, K_FOREVER);
    }
    if (vert != NULL) {
        k_thread_join(vert, K_FOREVER);
    }

leave:
    auto_homing_tid = NULL;
}

// Handlers

static void
handle_infrared_leds_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_infrared_leds_tag);

    InfraredLEDs_Wavelength wavelength =
        msg->message.j_message.payload.infrared_leds.wavelength;

    LOG_DBG("Got LED wavelength message = %d", wavelength);
    ir_camera_system_enable_leds(wavelength);
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_led_on_time_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_led_on_time_tag);

    uint32_t on_time_us =
        msg->message.j_message.payload.led_on_time.on_duration_us;

    LOG_DBG("Got LED on time message = %uus", on_time_us);
    ret_code_t ret = ir_camera_system_set_on_time_us(on_time_us);
    if (ret == RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    } else if (ret == RET_ERROR_INVALID_PARAM) {
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    }
}

static void
handle_led_on_time_740nm_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_led_on_time_740nm_tag);

    uint32_t on_time_us =
        msg->message.j_message.payload.led_on_time_740nm.on_duration_us;

    LOG_DBG("Got LED on time for 740nm message = %uus", on_time_us);
    ret_code_t ret = ir_camera_system_set_on_time_740nm_us(on_time_us);
    if (ret == RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    }
}

static void
handle_start_triggering_ir_eye_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_eye_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_eye_camera();
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_stop_triggering_ir_eye_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_eye_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_eye_camera();
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_start_triggering_ir_face_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_face_camera();
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_stop_triggering_ir_face_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_face_camera();
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_start_triggering_2dtof_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_start_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_2d_tof_camera();
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_stop_triggering_2dtof_camera_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_2d_tof_camera();
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_reboot_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_reboot_tag);

    uint32_t delay = msg->message.j_message.payload.reboot.delay;

    LOG_DBG("Got reboot in %us", delay);

    if (delay > 60) {
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        LOG_ERR("Reboot with delay > 60 seconds: %u", delay);
    } else {
        int ret = power_reset(delay);

        if (ret == RET_SUCCESS) {
            incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
        } else {
            incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
        }
    }
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
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }

    if (vertical_angle > MOTORS_ANGLE_VERTICAL_MAX ||
        vertical_angle < MOTORS_ANGLE_VERTICAL_MIN) {
        LOG_ERR("Vertical angle of %d out of range [%d;%d]", vertical_angle,
                MOTORS_ANGLE_VERTICAL_MIN, MOTORS_ANGLE_VERTICAL_MAX);
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }

    LOG_DBG("Got mirror angle message, vert: %d, horiz: %u", vertical_angle,
            horizontal_angle);

    if (motors_angle_horizontal(horizontal_angle) != RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else if (motors_angle_vertical(vertical_angle) != RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
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
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
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
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        LOG_DBG("Got fan speed message: %u%%", fan_speed_percentage);

        fan_set_speed(fan_speed_percentage);
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_user_leds_pattern(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_user_leds_pattern_tag);

    UserLEDsPattern_UserRgbLedPattern pattern =
        msg->message.j_message.payload.user_leds_pattern.pattern;

    LOG_DBG("Got new user RBG pattern message: %d", pattern);

    front_leds_set_pattern(pattern);
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
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
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        LOG_DBG("Got user LED brightness value of %u", brightness);
        front_leds_set_brightness(brightness);
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_distributor_leds_pattern(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_pattern_tag);

    LOG_DBG("Got distributor LED pattern");
    distributor_leds_set_pattern(
        msg->message.j_message.payload.distributor_leds_pattern.pattern);
    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

static void
handle_distributor_leds_brightness(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_brightness_tag);

    uint32_t brightness =
        msg->message.j_message.payload.distributor_leds_brightness.brightness;
    if (brightness > 255) {
        LOG_ERR("Got user LED brightness value of %u out of range [0,255]",
                brightness);
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        LOG_DBG("Got distributor LED brightness: %u", brightness);
        distributor_leds_set_brightness((uint8_t)brightness);
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_fw_img_crc(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_fw_image_check_tag);

    LOG_DBG("Got CRC comparison");
    int ret = dfu_secondary_check(
        msg->message.j_message.payload.fw_image_check.crc32);
    if (ret) {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_fw_img_sec_activate(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_fw_image_secondary_activate_tag);

    LOG_DBG("Got secondary slot activation");
    int ret;
    if (msg->message.j_message.payload.fw_image_secondary_activate
            .force_permanent) {
        ret = dfu_secondary_activate_permanently();
    } else {
        ret = dfu_secondary_activate_temporarily();
    }

    if (ret) {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));

        // wait for Jetson to shut down before we can reboot
        power_reboot_set_pending();
    }
}

static void
handle_fps(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_fps_tag);

    uint16_t fps = (uint16_t)msg->message.j_message.payload.fps.fps;

    LOG_DBG("Got FPS message = %u", fps);
    ret_code_t ret = ir_camera_system_set_fps(fps);
    if (ret == RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    } else if (ret == RET_ERROR_INVALID_PARAM) {
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    }
}

static void
handle_dfu_block_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_dfu_block_tag);
    __ASSERT(msg->message.j_message.payload.dfu_block.image_block.size <=
                 DFU_BLOCK_SIZE_MAX,
             "Block size must be <= DFU_BLOCK_SIZE_MAX bytes");

    LOG_DBG("Got firmware image block");
    int ret =
        dfu_load(msg->message.j_message.payload.dfu_block.block_number,
                 msg->message.j_message.payload.dfu_block.block_count,
                 msg->message.j_message.payload.dfu_block.image_block.bytes,
                 msg->message.j_message.payload.dfu_block.image_block.size,
                 msg->message.j_message.ack_number, handle_err_code);

    // if the operation is not over,
    // the DFU module will handle acknowledgement
    if (ret == -EINPROGRESS) {
        return;
    }

    switch (ret) {
    case RET_ERROR_INVALID_PARAM:
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        break;

    case RET_ERROR_BUSY:
        incoming_message_ack(Ack_ErrorCode_IN_PROGRESS, get_ack_num(msg));
        break;

    case RET_SUCCESS:
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
        break;
    default:
        LOG_ERR("Unhandled error code %d", ret);
    };
}

static void
handle_do_homing(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_do_homing_tag);

    PerformMirrorHoming_Mode mode =
        msg->message.j_message.payload.do_homing.homing_mode;
    PerformMirrorHoming_Mirror mirror =
        msg->message.j_message.payload.do_homing.mirror;
    LOG_DBG("Got do autohoming message, mode = %u, mirror = %u", mode, mirror);

    if (auto_homing_tid != NULL || motors_auto_homing_in_progress()) {
        incoming_message_ack(Ack_ErrorCode_IN_PROGRESS, get_ack_num(msg));
    } else {
        auto_homing_tid =
            k_thread_create(&auto_homing_thread, auto_homing_stack,
                            K_THREAD_STACK_SIZEOF(auto_homing_stack),
                            auto_homing_thread_entry_point, (void *)mode,
                            (void *)mirror, NULL, 4, 0, K_NO_WAIT);

        // send ack before timeout even though auto-homing not completed
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_liquid_lens(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_liquid_lens_tag);

    int32_t current = msg->message.j_message.payload.liquid_lens.current;
    bool enable = msg->message.j_message.payload.liquid_lens.enable;

    if (current < -400 || current > 400) {
        LOG_ERR("Got liquid lens current value of %d out of range [-400,400]",
                current);
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
    } else {
        LOG_DBG("Got liquid lens current value of %d", current);
        liquid_set_target_current_ma(current);

        if (enable) {
            liquid_lens_enable();
        } else {
            liquid_lens_disable();
        }

        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_heartbeat(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_heartbeat_tag);

    LOG_DBG("Got heartbeat");
    int ret = heartbeat_boom(
        msg->message.j_message.payload.heartbeat.timeout_seconds);

    if (ret == RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    }
}

static void
handle_mirror_angle_relative_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_mirror_angle_relative_tag);

    int32_t horizontal_angle =
        msg->message.j_message.payload.mirror_angle_relative.horizontal_angle;
    int32_t vertical_angle =
        msg->message.j_message.payload.mirror_angle_relative.vertical_angle;

    if (abs(horizontal_angle) > MOTORS_ANGLE_HORIZONTAL_RANGE) {
        LOG_ERR("Horizontal angle of %u out of range (max %u)",
                horizontal_angle, MOTORS_ANGLE_HORIZONTAL_RANGE);
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }
    if (abs(vertical_angle) > MOTORS_ANGLE_VERTICAL_RANGE) {
        LOG_ERR("Vertical angle of %u out of range (max %u)", vertical_angle,
                MOTORS_ANGLE_VERTICAL_RANGE);
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }

    LOG_DBG("Got relative mirror angle message, vert: %d, horiz: %u",
            vertical_angle, horizontal_angle);

    if (motors_angle_horizontal_relative(horizontal_angle) != RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else if (motors_angle_vertical_relative(vertical_angle) != RET_SUCCESS) {
        incoming_message_ack(Ack_ErrorCode_FAIL, get_ack_num(msg));
    } else {
        incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
    }
}

static void
handle_value_get_message(McuMessage *msg)
{
    MAKE_ASSERTS(JetsonToMcu_value_get_tag);

    LOG_DBG("Got Value Get request");

    ValueGet_Value value = msg->message.j_message.payload.value_get.value;
    switch (value) {
    case ValueGet_Value_FIRMWARE_VERSIONS:
        version_send();
        break;
    default: {
        // unknown value, respond with error
        incoming_message_ack(Ack_ErrorCode_RANGE, get_ack_num(msg));
        return;
    }
    }

    incoming_message_ack(Ack_ErrorCode_SUCCESS, get_ack_num(msg));
}

typedef void (*hm_callback)(McuMessage *msg);

// These functions ARE NOT allowed to block!
static const hm_callback handle_message_callbacks[] = {
    [JetsonToMcu_reboot_tag] = handle_reboot_message,
    [JetsonToMcu_mirror_angle_tag] = handle_mirror_angle_message,
    [JetsonToMcu_do_homing_tag] = handle_do_homing,
    [JetsonToMcu_infrared_leds_tag] = handle_infrared_leds_message,
    [JetsonToMcu_led_on_time_tag] = handle_led_on_time_message,
    [JetsonToMcu_user_leds_pattern_tag] = handle_user_leds_pattern,
    [JetsonToMcu_user_leds_brightness_tag] = handle_user_leds_brightness,
    [JetsonToMcu_distributor_leds_pattern_tag] =
        handle_distributor_leds_pattern,
    [JetsonToMcu_distributor_leds_brightness_tag] =
        handle_distributor_leds_brightness,
    [JetsonToMcu_dfu_block_tag] = handle_dfu_block_message,
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
    [JetsonToMcu_temperature_sample_period_tag] =
        handle_temperature_sample_period_message,
    [JetsonToMcu_fan_speed_tag] = handle_fan_speed,
    [JetsonToMcu_fps_tag] = handle_fps,
    [JetsonToMcu_liquid_lens_tag] = handle_liquid_lens,
    [JetsonToMcu_fw_image_check_tag] = handle_fw_img_crc,
    [JetsonToMcu_fw_image_secondary_activate_tag] = handle_fw_img_sec_activate,
    [JetsonToMcu_heartbeat_tag] = handle_heartbeat,
    [JetsonToMcu_led_on_time_740nm_tag] = handle_led_on_time_740nm_message,
    [JetsonToMcu_mirror_angle_relative_tag] =
        handle_mirror_angle_relative_message,
    [JetsonToMcu_value_get_tag] = handle_value_get_message,
};

static_assert(
    ARRAY_SIZE(handle_message_callbacks) <= 34,
    "It seems like the `handle_message_callbacks` array is too large");

void
incoming_message_handle(McuMessage *msg)
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
        incoming_message_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED,
                             get_ack_num(msg));
    }
}
