#include "runner.h"
#include "app_config.h"
#include "can_messaging.h"
#include "dfu.h"
#include "mcu_messaging.pb.h"
#include "power_sequence/power_sequence.h"
#include "pubsub/pubsub.h"
#include "ui/operator_leds/operator_leds.h"
#include "ui/rgb_leds.h"
#include "version/version.h"
#include <app_assert.h>
#include <assert.h>
#include <fan/fan.h>
#include <heartbeat.h>
#include <ir_camera_system/ir_camera_system.h>
#include <liquid_lens/liquid_lens.h>
#include <pb_decode.h>
#include <stdlib.h>
#include <stepper_motors/stepper_motors.h>
#include <temperature/temperature.h>
#include <ui/front_leds/front_leds.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(runner);

static K_THREAD_STACK_DEFINE(auto_homing_stack, 600);
static struct k_thread auto_homing_thread;
static k_tid_t auto_homing_tid = NULL;

#define MAKE_ASSERTS(tag)                                                      \
    ASSERT_SOFT_BOOL(msg->which_message == McuMessage_j_message_tag);          \
    ASSERT_SOFT_BOOL(msg->message.j_message.which_payload == tag)

static uint32_t job_counter = 0;

/// Keep context information in this module
/// for Device Firmware Upgrade (DFU) library,
/// so that we can ack firmware blocks
struct handle_error_context_s {
    uint32_t remote_addr;
    uint32_t ack_number;
};

/// Job to run with the identifier of the remote job initiator
typedef struct {
    uint32_t
        remote_addr; // destination ID to use to respond to the job initiator
    McuMessage mcu_message;
} job_t;

// Message queue
#define QUEUE_ALIGN 8
K_MSGQ_DEFINE(process_queue, sizeof(job_t), 8, QUEUE_ALIGN);

#ifndef CONFIG_CAN_ADDRESS_DEFAULT_REMOTE
#error "CONFIG_CAN_ADDRESS_DEFAULT_REMOTE not set"
#endif

uint32_t
runner_successful_jobs_count(void)
{
    return job_counter;
}

static void
job_ack(Ack_ErrorCode error, job_t *job)
{
    // get ack number from job's message
    uint32_t ack_number = job->mcu_message.message.j_message.ack_number;

    Ack ack = {.ack_number = ack_number, .error = error};

    int err_code =
        publish_new(&ack, sizeof(ack), McuToJetson_ack_tag, job->remote_addr);
    ASSERT_SOFT(err_code);

    if (error == Ack_ErrorCode_SUCCESS) {
        ++job_counter;
    }
}

/// Convert error codes to ack codes
static void
handle_err_code(void *ctx, int err)
{
    struct handle_error_context_s *context =
        (struct handle_error_context_s *)ctx;
    Ack ack = {.ack_number = context->ack_number, .error = Ack_ErrorCode_FAIL};

    switch (err) {
    case RET_SUCCESS:
        ack.ack_number = Ack_ErrorCode_SUCCESS;
        break;

    case RET_ERROR_INVALID_PARAM:
    case RET_ERROR_NOT_FOUND:
        ack.ack_number = Ack_ErrorCode_RANGE;
        break;

    case RET_ERROR_BUSY:
    case RET_ERROR_INVALID_STATE:
        ack.ack_number = Ack_ErrorCode_IN_PROGRESS;
        break;

    case RET_ERROR_FORBIDDEN:
        ack.ack_number = Ack_ErrorCode_OPERATION_NOT_SUPPORTED;
        break;

    default:
        /* nothing */
        break;
    }

    int err_code = publish_new(&ack, sizeof(ack), McuToJetson_ack_tag,
                               context->remote_addr);
    ASSERT_SOFT(err_code);

    ++job_counter;
}

void
auto_homing_thread_entry_point(void *a, void *b, void *c)
{
    ARG_UNUSED(c);

    PerformMirrorHoming_Mode mode = (PerformMirrorHoming_Mode)a;
    PerformMirrorHoming_Angle angle = (PerformMirrorHoming_Angle)b;

    ret_code_t ret;
    struct k_thread *horiz = NULL, *vert = NULL;

    if (mode == PerformMirrorHoming_Mode_STALL_DETECTION) {
        if (angle == PerformMirrorHoming_Angle_BOTH ||
            angle == PerformMirrorHoming_Angle_HORIZONTAL) {
            ret = motors_auto_homing_stall_detection(MOTOR_HORIZONTAL, &horiz);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
        if (angle == PerformMirrorHoming_Angle_BOTH ||
            angle == PerformMirrorHoming_Angle_VERTICAL) {
            ret = motors_auto_homing_stall_detection(MOTOR_VERTICAL, &vert);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
    } else if (mode == PerformMirrorHoming_Mode_ONE_BLOCKING_END) {
        if (angle == PerformMirrorHoming_Angle_BOTH ||
            angle == PerformMirrorHoming_Angle_HORIZONTAL) {
            ret = motors_auto_homing_one_end(MOTOR_HORIZONTAL, &horiz);
            if (ret == RET_ERROR_BUSY) {
                goto leave;
            }
        }
        if (angle == PerformMirrorHoming_Angle_BOTH ||
            angle == PerformMirrorHoming_Angle_VERTICAL) {
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
handle_infrared_leds_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_infrared_leds_tag);

    InfraredLEDs_Wavelength wavelength =
        msg->message.j_message.payload.infrared_leds.wavelength;

    LOG_DBG("Got LED wavelength message = %d", wavelength);
    ir_camera_system_enable_leds(wavelength);
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_led_on_time_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_led_on_time_tag);

    uint32_t on_time_us =
        msg->message.j_message.payload.led_on_time.on_duration_us;

    LOG_DBG("Got LED on time message = %uus", on_time_us);
    ret_code_t ret = ir_camera_system_set_on_time_us(on_time_us);
    if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else if (ret == RET_ERROR_INVALID_PARAM) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_led_on_time_740nm_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_led_on_time_740nm_tag);

    uint32_t on_time_us =
        msg->message.j_message.payload.led_on_time_740nm.on_duration_us;

    LOG_DBG("Got LED on time for 740nm message = %uus", on_time_us);
    ret_code_t ret = ir_camera_system_set_on_time_740nm_us(on_time_us);
    if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else {
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_start_triggering_ir_eye_camera_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_eye_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_eye_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_ir_eye_camera_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_eye_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_eye_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_start_triggering_ir_face_camera_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_face_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_ir_face_camera_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_face_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_start_triggering_2dtof_camera_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_start_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_2d_tof_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_2dtof_camera_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_2d_tof_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_shutdown(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_shutdown_tag);

    uint32_t delay = msg->message.j_message.payload.shutdown.delay_s;
    LOG_DBG("Got shutdown in %us", delay);

    if (delay > 30) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        int ret = power_reset(delay);

        if (ret == RET_SUCCESS) {
            job_ack(Ack_ErrorCode_SUCCESS, job);
        } else {
            job_ack(Ack_ErrorCode_FAIL, job);
        }
    }
}

static void
handle_reboot_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_reboot_tag);

    uint32_t delay = msg->message.j_message.payload.reboot.delay;

    LOG_DBG("Got reboot in %us", delay);

    if (delay > 60) {
        job_ack(Ack_ErrorCode_RANGE, job);
        LOG_ERR("Reboot with delay > 60 seconds: %u", delay);
    } else {
        int ret = power_reset(delay);

        if (ret == RET_SUCCESS) {
            job_ack(Ack_ErrorCode_SUCCESS, job);
        } else {
            job_ack(Ack_ErrorCode_FAIL, job);
        }
    }
}

static void
handle_mirror_angle_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_mirror_angle_tag);

    uint32_t horizontal_angle =
        msg->message.j_message.payload.mirror_angle.horizontal_angle;
    int32_t vertical_angle =
        msg->message.j_message.payload.mirror_angle.vertical_angle;

    if (auto_homing_tid != NULL || motors_auto_homing_in_progress()) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
        return;
    }

    if (horizontal_angle > MOTORS_ANGLE_HORIZONTAL_MAX ||
        horizontal_angle < MOTORS_ANGLE_HORIZONTAL_MIN) {
        LOG_ERR("Horizontal angle of %u out of range [%u;%u]", horizontal_angle,
                MOTORS_ANGLE_HORIZONTAL_MIN, MOTORS_ANGLE_HORIZONTAL_MAX);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }

    if (vertical_angle > MOTORS_ANGLE_VERTICAL_MAX ||
        vertical_angle < MOTORS_ANGLE_VERTICAL_MIN) {
        LOG_ERR("Vertical angle of %d out of range [%d;%d]", vertical_angle,
                MOTORS_ANGLE_VERTICAL_MIN, MOTORS_ANGLE_VERTICAL_MAX);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }

    LOG_DBG("Got mirror angle message, vert: %d, horiz: %u", vertical_angle,
            horizontal_angle);

    if (motors_angle_horizontal(horizontal_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else if (motors_angle_vertical(vertical_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_temperature_sample_period_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_temperature_sample_period_tag);

    uint32_t sample_period_ms = msg->message.j_message.payload
                                    .temperature_sample_period.sample_period_ms;

    LOG_DBG("Got new temperature sampling period: %ums", sample_period_ms);

    if (sample_period_ms > 15000) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        temperature_set_sampling_period_ms(sample_period_ms);
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_fan_speed(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_fan_speed_tag);

    // value and percentage have the same representation,
    // so there's no point switching on which one
    uint32_t fan_speed = msg->message.j_message.payload.fan_speed.payload.value;

    if (temperature_is_in_overtemp()) {
        LOG_WRN("Fan speed command rejected do to overtemperature condition");
        job_ack(Ack_ErrorCode_OVER_TEMPERATURE, job);
    } else {
        switch (msg->message.j_message.payload.fan_speed.which_payload) {
        case 0: /* no tag provided with legacy API */
        case FanSpeed_percentage_tag:
            if (fan_speed > 100) {
                LOG_ERR("Got fan speed of %" PRIu32 " out of range [0;100]",
                        fan_speed);
                job_ack(Ack_ErrorCode_RANGE, job);
            } else {
                LOG_DBG("Got fan speed percentage message: %" PRIu32 "%%",
                        fan_speed);

                fan_set_speed_by_percentage(fan_speed);
                job_ack(Ack_ErrorCode_SUCCESS, job);
            }
            break;
        case FanSpeed_value_tag:
            if (fan_speed > UINT16_MAX) {
                LOG_ERR("Got fan speed of %" PRIu32 " out of range [0;%u]",
                        fan_speed, UINT16_MAX);
                job_ack(Ack_ErrorCode_RANGE, job);
            } else {
                LOG_DBG("Got fan speed value message: %" PRIu32, fan_speed);

                fan_set_speed_by_value(fan_speed);
                job_ack(Ack_ErrorCode_SUCCESS, job);
            }
            break;
        default:
            job_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
            ASSERT_SOFT(RET_ERROR_INTERNAL);
            break;
        }
    }
}

static void
handle_user_leds_pattern(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_user_leds_pattern_tag);

    UserLEDsPattern_UserRgbLedPattern pattern =
        msg->message.j_message.payload.user_leds_pattern.pattern;
    uint32_t start_angle =
        msg->message.j_message.payload.user_leds_pattern.start_angle;
    int32_t angle_length =
        msg->message.j_message.payload.user_leds_pattern.angle_length;
    uint32_t pulsing_period_ms =
        msg->message.j_message.payload.user_leds_pattern.pulsing_period_ms;
    float pulsing_scale =
        msg->message.j_message.payload.user_leds_pattern.pulsing_scale;

    LOG_DBG("Got new user RBG pattern message: %d, start %uº, angle length %dº",
            pattern, start_angle, angle_length);

    if (start_angle > FULL_RING_DEGREES ||
        abs(angle_length) > FULL_RING_DEGREES) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        RgbColor *color_ptr = NULL;
        if (msg->message.j_message.payload.user_leds_pattern.pattern ==
            UserLEDsPattern_UserRgbLedPattern_RGB) {
            color_ptr =
                &msg->message.j_message.payload.user_leds_pattern.custom_color;
        }
        ret_code_t ret =
            front_leds_set_pattern(pattern, start_angle, angle_length,
                                   color_ptr, pulsing_period_ms, pulsing_scale);

        job_ack(ret == RET_SUCCESS ? Ack_ErrorCode_SUCCESS : Ack_ErrorCode_FAIL,
                job);
    }
}

static void
handle_user_leds_brightness(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_user_leds_brightness_tag);

    uint32_t brightness =
        msg->message.j_message.payload.user_leds_brightness.brightness;

    if (brightness > 255) {
        LOG_ERR("Got user LED brightness value of %u out of range [0,255]",
                brightness);
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got user LED brightness value of %u", brightness);
        front_leds_set_brightness(brightness);
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_distributor_leds_pattern(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_pattern_tag);

    DistributorLEDsPattern_DistributorRgbLedPattern pattern =
        msg->message.j_message.payload.distributor_leds_pattern.pattern;
    uint32_t mask =
        msg->message.j_message.payload.distributor_leds_pattern.leds_mask;

    LOG_DBG("Got distributor LED pattern: %u, mask 0x%x", pattern, mask);

    if (mask > OPERATOR_LEDS_ALL_MASK) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        RgbColor *color_ptr = NULL;
        if (msg->message.j_message.payload.distributor_leds_pattern.pattern ==
            DistributorLEDsPattern_DistributorRgbLedPattern_RGB) {
            color_ptr = &msg->message.j_message.payload.distributor_leds_pattern
                             .custom_color;
        }
        if (operator_leds_set_pattern(pattern, mask, color_ptr) !=
            RET_SUCCESS) {
            job_ack(Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_distributor_leds_brightness(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_brightness_tag);

    uint32_t brightness =
        msg->message.j_message.payload.distributor_leds_brightness.brightness;
    if (brightness > 255) {
        LOG_ERR("Got user LED brightness value of %u out of range [0,255]",
                brightness);
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got distributor LED brightness: %u", brightness);
        if (operator_leds_set_brightness((uint8_t)brightness) != RET_SUCCESS) {
            job_ack(Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_fw_img_crc(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_fw_image_check_tag);

    LOG_DBG("Got CRC comparison");
    int ret = dfu_secondary_check(
        msg->message.j_message.payload.fw_image_check.crc32);
    if (ret) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_fw_img_sec_activate(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
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
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);

        // turn operator LEDs orange
        operator_leds_set_pattern(
            DistributorLEDsPattern_DistributorRgbLedPattern_RGB,
            OPERATOR_LEDS_ALL_MASK, &(RgbColor)RGB_ORANGE);

        // wait for Jetson to shut down before we can reboot
        power_reboot_set_pending();
    }
}

static void
handle_fps(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_fps_tag);

    uint16_t fps = (uint16_t)msg->message.j_message.payload.fps.fps;

    LOG_DBG("Got FPS message = %u", fps);
    ret_code_t ret = ir_camera_system_set_fps(fps);
    if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else if (ret == RET_ERROR_INVALID_PARAM) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_dfu_block_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_dfu_block_tag);

    // must be static to be used by callback
    static struct handle_error_context_s context = {0};
    context.remote_addr = job->remote_addr;
    context.ack_number = job->mcu_message.message.j_message.ack_number;

    LOG_DBG("Got firmware image block");
    int ret =
        dfu_load(msg->message.j_message.payload.dfu_block.block_number,
                 msg->message.j_message.payload.dfu_block.block_count,
                 msg->message.j_message.payload.dfu_block.image_block.bytes,
                 msg->message.j_message.payload.dfu_block.image_block.size,
                 (void *)&context, handle_err_code);

    // if the operation is not over,
    // the DFU module will handle acknowledgement
    if (ret == -EINPROGRESS) {
        return;
    }

    switch (ret) {
    case RET_ERROR_INVALID_PARAM:
        job_ack(Ack_ErrorCode_RANGE, job);
        break;

    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
        break;

    case RET_SUCCESS:
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    default:
        LOG_ERR("Unhandled error code %d", ret);
    };
}

static void
handle_do_homing(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_do_homing_tag);

    PerformMirrorHoming_Mode mode =
        msg->message.j_message.payload.do_homing.homing_mode;
    PerformMirrorHoming_Angle angle =
        msg->message.j_message.payload.do_homing.angle;
    LOG_DBG("Got do autohoming message, mode = %u, angle = %u", mode, angle);

    if (auto_homing_tid != NULL || motors_auto_homing_in_progress()) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
    } else {
        auto_homing_tid =
            k_thread_create(&auto_homing_thread, auto_homing_stack,
                            K_THREAD_STACK_SIZEOF(auto_homing_stack),
                            auto_homing_thread_entry_point, (void *)mode,
                            (void *)angle, NULL, 4, 0, K_NO_WAIT);
        k_thread_name_set(auto_homing_tid, "autohoming");

        // send ack before timeout even though auto-homing not completed
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_liquid_lens(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_liquid_lens_tag);

    int32_t current = msg->message.j_message.payload.liquid_lens.current;
    bool enable = msg->message.j_message.payload.liquid_lens.enable;

    if (current < -400 || current > 400) {
        LOG_ERR("Got liquid lens current value of %d out of range [-400,400]",
                current);
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got liquid lens current value of %d", current);
        liquid_set_target_current_ma(current);

        if (enable) {
            liquid_lens_enable();
        } else {
            liquid_lens_disable();
        }

        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_heartbeat(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_heartbeat_tag);

    LOG_DBG("Got heartbeat");
    int ret = heartbeat_boom(
        msg->message.j_message.payload.heartbeat.timeout_seconds);

    if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else {
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_mirror_angle_relative_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_mirror_angle_relative_tag);

    int32_t horizontal_angle =
        msg->message.j_message.payload.mirror_angle_relative.horizontal_angle;
    int32_t vertical_angle =
        msg->message.j_message.payload.mirror_angle_relative.vertical_angle;

    if (auto_homing_tid != NULL || motors_auto_homing_in_progress()) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
        return;
    }

    if (abs(horizontal_angle) > MOTORS_ANGLE_HORIZONTAL_RANGE) {
        LOG_ERR("Horizontal angle of %u out of range (max %u)",
                horizontal_angle, MOTORS_ANGLE_HORIZONTAL_RANGE);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }
    if (abs(vertical_angle) > MOTORS_ANGLE_VERTICAL_RANGE) {
        LOG_ERR("Vertical angle of %u out of range (max %u)", vertical_angle,
                MOTORS_ANGLE_VERTICAL_RANGE);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }

    LOG_DBG("Got relative mirror angle message, vert: %d, horiz: %u",
            vertical_angle, horizontal_angle);

    if (motors_angle_horizontal_relative(horizontal_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else if (motors_angle_vertical_relative(vertical_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_value_get_message(job_t *job)
{
    McuMessage *msg = &job->mcu_message;
    MAKE_ASSERTS(JetsonToMcu_value_get_tag);

    LOG_DBG("Got Value Get request");

    ValueGet_Value value = msg->message.j_message.payload.value_get.value;
    switch (value) {
    case ValueGet_Value_FIRMWARE_VERSIONS:
        version_send(job->remote_addr);
        break;
    default: {
        // unknown value, respond with error
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }
    }

    job_ack(Ack_ErrorCode_SUCCESS, job);
}

typedef void (*hm_callback)(job_t *job);

// These functions ARE NOT allowed to block!
static const hm_callback handle_message_callbacks[] = {
    [JetsonToMcu_shutdown_tag] = handle_shutdown,
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

_Noreturn static void
runner_process_jobs_thread()
{
    job_t new;
    int ret;

    while (1) {
        ret = k_msgq_get(&process_queue, &new, K_FOREVER);
        if (ret != 0) {
            ASSERT_SOFT(ret);
            continue;
        }

        LOG_INF("⬇️ Received message from remote 0x%03x with payload ID "
                "%02d, ack #%u",
                new.remote_addr,
                new.mcu_message.message.j_message.which_payload,
                new.mcu_message.message.j_message.ack_number);

        // first job, enable publishing new messages
        if (job_counter == 0) {
            publish_start();
        }

        if (new.mcu_message.message.j_message.which_payload <
                ARRAY_SIZE(handle_message_callbacks) &&
            handle_message_callbacks[new.mcu_message.message.j_message
                                         .which_payload] != NULL) {
            handle_message_callbacks[new.mcu_message.message.j_message
                                         .which_payload](&new);
        } else {
            LOG_ERR("A handler for message with a payload ID of %d is not "
                    "implemented",
                    new.mcu_message.message.j_message.which_payload);
            job_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED, &new);
        }
    }
}

K_MUTEX_DEFINE(new_job_mutex);

void
runner_handle_new(can_message_t *msg)
{
    static job_t new = {0}; // declared as static not to use caller stack

    pb_istream_t stream = pb_istream_from_buffer(msg->bytes, msg->size);

    int ret = k_mutex_lock(&new_job_mutex, K_MSEC(5));
    if (ret == 0) {
        bool decoded = pb_decode_ex(&stream, McuMessage_fields,
                                    &new.mcu_message, PB_DECODE_DELIMITED);
        if (decoded) {
            if (new.mcu_message.which_message != McuMessage_j_message_tag) {
                LOG_INF("Got message not intended for us. Dropping.");
                return;
            }

            if (msg->destination & CAN_ADDR_IS_ISOTP) {
                // keep flags of the received message destination
                // & invert source and destination
                new.remote_addr = (msg->destination & ~0xFF);
                new.remote_addr |= (msg->destination & 0xF) << 4;
                new.remote_addr |= (msg->destination & 0xF0) >> 4;
            } else {
                new.remote_addr = CONFIG_CAN_ADDRESS_DEFAULT_REMOTE;
            }

            ret = k_msgq_put(&process_queue, &new, K_NO_WAIT);
            ASSERT_SOFT(ret);
        } else {
            LOG_ERR("Unable to decode");
        }

        k_mutex_unlock(&new_job_mutex);
    } else {
        LOG_ERR("Error locking mutex to create new runner job: %d", ret);
    }
}

K_THREAD_DEFINE(runner, THREAD_STACK_SIZE_RUNNER, runner_process_jobs_thread,
                NULL, NULL, NULL, THREAD_PRIORITY_RUNNER, 0, 0);
