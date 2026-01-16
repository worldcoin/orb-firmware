#include "runner.h"
#include "app_assert.h"
#include "app_config.h"
#include "can_messaging.h"
#include "dfu.h"
#include "mcu.pb.h"
#include "mcu_ping.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/mirror/mirror.h"
#include "orb_logs.h"
#include "pb_encode.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "storage.h"
#include "system/backup_regs.h"
#include "system/version/version.h"
#include "temperature/fan/fan.h"
#include "temperature/sensors/temperature.h"
#include "ui/rgb_leds/operator_leds/operator_leds.h"
#include "ui/rgb_leds/rgb_leds.h"
#include "ui/ui.h"
#include "voltage_measurement/voltage_measurement.h"
#include <date.h>
#include <drivers/optics/liquid_lens/liquid_lens.h>
#include <heartbeat.h>
#include <math.h>
#include <optics/polarizer_wheel/polarizer_wheel.h>
#include <pb_decode.h>
#include <stdlib.h>
#include <uart_messaging.h>
#include <ui/rgb_leds/front_leds/front_leds.h>
#include <zephyr/kernel.h>

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#include "ui/rgb_leds/cone_leds/cone_leds.h"
#include "ui/white_leds/white_leds.h"
#endif

#if defined(CONFIG_MEMFAULT_METRICS_CONNECTIVITY_CONNECTED_TIME)
#include <memfault/metrics/connectivity.h>
#endif

#if defined(CONFIG_MEMFAULT)
#include <memfault/core/reboot_tracking.h>
#endif

LOG_MODULE_REGISTER(runner, CONFIG_RUNNER_LOG_LEVEL);

K_THREAD_STACK_DEFINE(runner_process_stack, THREAD_STACK_SIZE_RUNNER);
static struct k_thread runner_process;
static k_tid_t runner_tid = NULL;

#define MAKE_ASSERTS(tag) ASSERT_SOFT_BOOL(msg->which_payload == tag)

static uint32_t job_counter = 0;

enum remote_type_e {
    CAN_JETSON_MESSAGING,
    CAN_SEC_MCU_MESSAGING,
    UART_MESSAGING,
    CLI,
};

/// Keep context information in this module
/// for Device Firmware Upgrade (DFU) library,
/// so that we can ack firmware blocks
struct handle_error_context_s {
    enum remote_type_e remote;
    uint32_t remote_addr;
    uint32_t ack_number;
};

/// Job to run with the identifier of the remote job initiator
typedef struct {
    enum remote_type_e remote;
    /// destination ID to use to respond to the job initiator
    uint32_t remote_addr;
    uint32_t ack_number;
    union {
        orb_mcu_main_JetsonToMcu jetson_cmd;
        orb_mcu_sec_SecToMain sec_cmd;
    } message;
} job_t;

// Message queue
#define QUEUE_ALIGN 8
K_MSGQ_DEFINE(process_queue, sizeof(job_t), 8, QUEUE_ALIGN);

#ifndef CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX
#error "CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX not set"
#endif

uint32_t
runner_successful_jobs_count(void)
{
    return job_counter;
}

static void
job_ack(orb_mcu_Ack_ErrorCode error, job_t *job)
{
    // ack only messages sent using CAN
    if (job->remote == CAN_JETSON_MESSAGING) {
        // get ack number from job
        uint32_t ack_number = job->ack_number;

        orb_mcu_Ack ack = {.ack_number = ack_number, .error = error};

        publish_new(&ack, sizeof(ack), orb_mcu_main_McuToJetson_ack_tag,
                    job->remote_addr);
    }

    if (error == orb_mcu_Ack_ErrorCode_SUCCESS) {
        ++job_counter;
    }
}

/// Convert error codes to ack codes
static void
handle_err_code(void *ctx, int err)
{
    struct handle_error_context_s *context =
        (struct handle_error_context_s *)ctx;

    if (context->remote == CAN_JETSON_MESSAGING) {
        orb_mcu_Ack ack = {.ack_number = context->ack_number,
                           .error = orb_mcu_Ack_ErrorCode_FAIL};

        switch (err) {
        case RET_SUCCESS:
            ack.error = orb_mcu_Ack_ErrorCode_SUCCESS;
            break;

        case RET_ERROR_INVALID_PARAM:
        case RET_ERROR_NOT_FOUND:
            ack.error = orb_mcu_Ack_ErrorCode_RANGE;
            break;

        case RET_ERROR_BUSY:
        case RET_ERROR_INVALID_STATE:
            ack.error = orb_mcu_Ack_ErrorCode_IN_PROGRESS;
            break;

        case RET_ERROR_FORBIDDEN:
            ack.error = orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED;
            break;

        default:
            ack.error = orb_mcu_Ack_ErrorCode_FAIL;
            break;
        }

        publish_new(&ack, sizeof(ack), orb_mcu_main_McuToJetson_ack_tag,
                    context->remote_addr);
    }

    if (err == RET_SUCCESS) {
        ++job_counter;
    }
}

// Handlers

static void
handle_infrared_leds_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_infrared_leds_tag);

    orb_mcu_main_InfraredLEDs_Wavelength wavelength =
        msg->payload.infrared_leds.wavelength;

    LOG_DBG("Got LED wavelength message = %d", wavelength);
    ret_code_t err = ir_camera_system_enable_leds(wavelength);
    switch (err) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    case RET_ERROR_FORBIDDEN:
        job_ack(orb_mcu_Ack_ErrorCode_FORBIDDEN, job);
        break;
    case RET_ERROR_INVALID_PARAM:
        job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_led_on_time_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_led_on_time_tag);

    uint32_t on_time_us = msg->payload.led_on_time.on_duration_us;

    LOG_DBG("Got LED on time message = %uus", on_time_us);
    ret_code_t ret = ir_camera_system_set_on_time_us(on_time_us);
    switch (ret) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_FORBIDDEN:
        job_ack(orb_mcu_Ack_ErrorCode_FORBIDDEN, job);
        break;
    case RET_ERROR_INVALID_PARAM:
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        break;
    }
}

static void
handle_start_triggering_ir_eye_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_start_triggering_ir_eye_camera_tag);

    ret_code_t err = ir_camera_system_enable_ir_eye_camera();
    switch (err) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_stop_triggering_ir_eye_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_stop_triggering_ir_eye_camera_tag);

    ret_code_t err = ir_camera_system_disable_ir_eye_camera();

    switch (err) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_start_triggering_ir_face_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_start_triggering_ir_face_camera_tag);

    ir_camera_system_enable_ir_face_camera();
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_start_triggering_rgb_face_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_start_triggering_rgb_face_camera_tag);

    ir_camera_system_enable_rgb_face_camera();
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_rgb_face_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_stop_triggering_rgb_face_camera_tag);

    ir_camera_system_disable_rgb_face_camera();
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_ir_face_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_stop_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_face_camera();
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_start_triggering_2dtof_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_start_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_2d_tof_camera();
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_2dtof_camera_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_stop_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_2d_tof_camera();
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_shutdown(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_shutdown_tag);

    uint32_t delay = msg->payload.shutdown.delay_s;
    LOG_DBG("Got shutdown in %us", delay);

    if (delay > 30) {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        int ret = reboot(delay);

        if (ret == RET_SUCCESS) {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        }
    }
}

static void
handle_reboot_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_reboot_tag);

    uint32_t delay = msg->payload.reboot.delay;

    LOG_DBG("Got reboot in %us", delay);

    if (delay > 60) {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        LOG_ERR("Reboot with delay > 60 seconds: %u", delay);
    } else {
        int ret = reboot(delay);

        if (ret == RET_SUCCESS) {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
            // Send out shutdown scheduled CAN message
            orb_mcu_main_ShutdownScheduled shutdown;
            shutdown.shutdown_reason =
                orb_mcu_main_ShutdownScheduled_ShutdownReason_JETSON_REQUESTED_REBOOT;
            shutdown.has_ms_until_shutdown = true;
            shutdown.ms_until_shutdown = delay * 1000;
            publish_new(&shutdown, sizeof(shutdown),
                        orb_mcu_main_McuToJetson_shutdown_tag,
                        CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX);
#ifdef CONFIG_MEMFAULT
            MEMFAULT_REBOOT_MARK_RESET_IMMINENT(
                kMfltRebootReason_JetsonRequestedReboot);
#endif
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        }
    }
}

static void
handle_reboot_orb(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_reboot_orb_tag);

    uint32_t delay = msg->payload.reboot_orb.force_reboot_timeout_s;

    if (delay != 0 && (delay > 60 || delay < 10)) {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        LOG_ERR("Reboot with delay > 60s or < 10s: %u", delay);
    } else {
        int ret =
            backup_regs_write_byte(REBOOT_FLAG_OFFSET_BYTE, REBOOT_INSTABOOT);
        if (ret == 0) {
            if (delay != 0) {
                // force reboot after `delay` seconds,
                // but a shutdown request from the Jetson is preferred
                // (SHUTDOWN_REQ gpio)
                ret = reboot(delay);
            } else {
                LOG_INF("waiting for reboot request from Jetson");
            }

            if (ret == 0) {
#ifdef CONFIG_MEMFAULT
                MEMFAULT_REBOOT_MARK_RESET_IMMINENT(
                    kMfltRebootReason_JetsonRequestedRebootOrb);
#endif
                job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
                return;
            }
        }
    }

    // failure setting flag or initiating reboot
    // reset flag
    backup_regs_write_byte(REBOOT_FLAG_OFFSET_BYTE, 0);
    job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
}

static void
handle_boot_complete(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_boot_complete_tag);

    int ret = front_leds_boot_progress_set(BOOT_PROGRESS_STEP_DONE);
    if (ret == RET_SUCCESS) {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    } else {
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_mirror_angle_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_mirror_angle_tag);

    uint32_t mirror_target_angle_phi_millidegrees;
    uint32_t mirror_target_angle_theta_millidegrees;

    switch (msg->payload.mirror_angle.angle_type) {
    case orb_mcu_main_MirrorAngleType_HORIZONTAL_VERTICAL: {
        // this angle type should not be used anymore but is kept for
        // compatibility
        uint32_t horizontal_angle_millidegrees =
            msg->payload.mirror_angle.horizontal_angle;
        int32_t vertical_angle_millidegrees =
            msg->payload.mirror_angle.vertical_angle;
        mirror_target_angle_phi_millidegrees =
            -(((int32_t)horizontal_angle_millidegrees - 45000) / 2) + 45000;
        mirror_target_angle_theta_millidegrees =
            vertical_angle_millidegrees / 2 + 90000;
    } break;
    case orb_mcu_main_MirrorAngleType_PHI_THETA:
        mirror_target_angle_phi_millidegrees =
            msg->payload.mirror_angle.phi_angle_millidegrees;
        mirror_target_angle_theta_millidegrees =
            msg->payload.mirror_angle.theta_angle_millidegrees;
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        return;
    }

    if (mirror_auto_homing_in_progress()) {
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
        return;
    }

    LOG_DBG("Got mirror angle message, theta: %u, phi: %u",
            mirror_target_angle_theta_millidegrees,
            mirror_target_angle_phi_millidegrees);

    ret_code_t ret = mirror_set_angle_phi(mirror_target_angle_phi_millidegrees);
    if (ret == RET_SUCCESS) {
        ret = mirror_set_angle_theta(mirror_target_angle_theta_millidegrees);
    }

    if (ret != RET_SUCCESS) {
        if (ret == RET_ERROR_INVALID_PARAM) {
            job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        }
        return;
    }

    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_temperature_sample_period_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_temperature_sample_period_tag);

    uint32_t sample_period_ms =
        msg->payload.temperature_sample_period.sample_period_ms;

    LOG_DBG("Got new temperature sampling period: %ums", sample_period_ms);

    if (temperature_set_sampling_period_ms(sample_period_ms) == RET_SUCCESS) {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    } else {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    }
}

static void
handle_fan_speed(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_fan_speed_tag);

    // value and percentage have the same representation,
    // so there's no point switching on which one
    uint32_t fan_speed = msg->payload.fan_speed.payload.value;

    if (temperature_is_in_overtemp()) {
        LOG_WRN("Overtemperature: fan speed command rejected");
        job_ack(orb_mcu_Ack_ErrorCode_OVER_TEMPERATURE, job);
    } else {
        switch (msg->payload.fan_speed.which_payload) {
        case 0: /* no tag provided with legacy API */
        case orb_mcu_main_FanSpeed_percentage_tag:
            if (fan_speed > 100) {
                LOG_ERR("Got fan speed of %" PRIu32 " out of range [0;100]",
                        fan_speed);
                job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
            } else {
                LOG_DBG("Got fan speed percentage message: %" PRIu32 "%%",
                        fan_speed);

                fan_set_speed_by_percentage(fan_speed);
                job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
            }
            break;
        case orb_mcu_main_FanSpeed_value_tag:
            if (fan_speed > UINT16_MAX) {
                LOG_ERR("Got fan speed of %" PRIu32 " out of range [0;%u]",
                        fan_speed, UINT16_MAX);
                job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
            } else {
                LOG_DBG("Got fan speed value message: %" PRIu32, fan_speed);

                fan_set_speed_by_value(fan_speed);
                job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
            }
            break;
        default:
            job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
            ASSERT_SOFT(RET_ERROR_INTERNAL);
            break;
        }
    }
}

static void
handle_user_leds_pattern(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_user_leds_pattern_tag);

    orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern =
        msg->payload.user_leds_pattern.pattern;
    uint32_t start_angle = msg->payload.user_leds_pattern.start_angle;
    int32_t angle_length = msg->payload.user_leds_pattern.angle_length;
    uint32_t pulsing_period_ms =
        msg->payload.user_leds_pattern.pulsing_period_ms;
    float pulsing_scale = msg->payload.user_leds_pattern.pulsing_scale;

    LOG_DBG("Got new user RBG pattern message: %d, start %uº, angle length %dº",
            pattern, start_angle, angle_length);

    if (start_angle > FULL_RING_DEGREES ||
        abs(angle_length) > FULL_RING_DEGREES) {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        orb_mcu_main_RgbColor *color_ptr = NULL;
        if (msg->payload.user_leds_pattern.has_custom_color) {
            color_ptr = &msg->payload.user_leds_pattern.custom_color;
        }
        ret_code_t ret =
            front_leds_set_pattern(pattern, start_angle, angle_length,
                                   color_ptr, pulsing_period_ms, pulsing_scale);

        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
    }
}

static void
handle_user_center_leds_sequence(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_center_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format = msg->payload.center_leds_sequence.which_data_format;
    uint8_t *bytes;
    uint32_t size;

    switch (data_format) {
    case orb_mcu_main_UserCenterLEDsSequence_rgb_uncompressed_tag:;
        bytes = msg->payload.center_leds_sequence.data_format.rgb_uncompressed
                    .bytes;
        size =
            msg->payload.center_leds_sequence.data_format.rgb_uncompressed.size;

        ret = front_leds_set_center_leds_sequence_rgb24(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    case orb_mcu_main_UserCenterLEDsSequence_argb32_uncompressed_tag:;
        bytes = msg->payload.center_leds_sequence.data_format
                    .argb32_uncompressed.bytes;
        size = msg->payload.center_leds_sequence.data_format.argb32_uncompressed
                   .size;

        ret = front_leds_set_center_leds_sequence_argb32(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_user_ring_leds_sequence(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_ring_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format = msg->payload.ring_leds_sequence.which_data_format;
    uint8_t *bytes;
    uint32_t size;

    switch (data_format) {
    case orb_mcu_main_UserRingLEDsSequence_rgb_uncompressed_tag:
        bytes =
            msg->payload.ring_leds_sequence.data_format.rgb_uncompressed.bytes;
        size =
            msg->payload.ring_leds_sequence.data_format.rgb_uncompressed.size;

        ret = front_leds_set_ring_leds_sequence_rgb24(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    case orb_mcu_main_UserRingLEDsSequence_argb32_uncompressed_tag:
        bytes = msg->payload.ring_leds_sequence.data_format.argb32_uncompressed
                    .bytes;
        size = msg->payload.ring_leds_sequence.data_format.argb32_uncompressed
                   .size;
        ret = front_leds_set_ring_leds_sequence_argb32(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_distributor_leds_sequence(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_distributor_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format =
        msg->payload.distributor_leds_sequence.which_data_format;
    uint8_t *bytes;
    uint32_t size;

    switch (data_format) {
    case orb_mcu_main_DistributorLEDsSequence_rgb_uncompressed_tag:;
        bytes = msg->payload.distributor_leds_sequence.data_format
                    .rgb_uncompressed.bytes;
        size = msg->payload.distributor_leds_sequence.data_format
                   .rgb_uncompressed.size;

        ret = operator_leds_set_leds_sequence_rgb24(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    case orb_mcu_main_DistributorLEDsSequence_argb32_uncompressed_tag:;
        bytes = msg->payload.distributor_leds_sequence.data_format
                    .argb32_uncompressed.bytes;
        size = msg->payload.distributor_leds_sequence.data_format
                   .argb32_uncompressed.size;

        ret = operator_leds_set_leds_sequence_argb32(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

#ifdef CONFIG_BOARD_DIAMOND_MAIN
static void
handle_cone_leds_sequence(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_cone_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format = msg->payload.cone_leds_sequence.which_data_format;
    uint8_t *bytes;
    uint32_t size;

#if !defined(CONFIG_DT_HAS_DIAMOND_CONE_ENABLED)
    job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    return;
#endif

    switch (data_format) {
    case orb_mcu_main_ConeLEDsSequence_rgb_uncompressed_tag:;
        bytes =
            msg->payload.cone_leds_sequence.data_format.rgb_uncompressed.bytes;
        size =
            msg->payload.cone_leds_sequence.data_format.rgb_uncompressed.size;

        ret = cone_leds_set_leds_sequence_rgb24(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    case orb_mcu_main_ConeLEDsSequence_argb32_uncompressed_tag:;
        bytes = msg->payload.cone_leds_sequence.data_format.argb32_uncompressed
                    .bytes;
        size = msg->payload.cone_leds_sequence.data_format.argb32_uncompressed
                   .size;

        ret = cone_leds_set_leds_sequence_argb32(bytes, size);
        job_ack(ret == RET_SUCCESS ? orb_mcu_Ack_ErrorCode_SUCCESS
                                   : orb_mcu_Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_cone_leds_pattern(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_cone_leds_pattern_tag);

#if !defined(CONFIG_DT_HAS_DIAMOND_CONE_ENABLED)
    job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
#else
    ConeLEDsPattern_ConeRgbLedPattern pattern =
        msg->payload.cone_leds_pattern.pattern;
    LOG_DBG("Got cone LED pattern: %u", pattern);
    RgbColor color = (msg->payload.cone_leds_pattern.has_custom_color)
                         ? msg->payload.cone_leds_pattern.custom_color
                         : (RgbColor){20, 20, 20};
    cone_leds_set_pattern(pattern, &color);
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
#endif
}

static void
handle_white_leds_brightness(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_white_leds_brightness_tag);

    uint32_t brightness = msg->payload.white_leds_brightness.brightness;
    if (brightness > 1000) {
        LOG_ERR("Got white LED brightness value of %u out of range [0,1000]",
                brightness);
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got white LED brightness value of %u", brightness);
        white_leds_set_brightness(brightness);
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    }
}
#endif // CONFIG_BOARD_DIAMOND_MAIN

static void
handle_user_leds_brightness(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_user_leds_brightness_tag);

    uint32_t brightness = msg->payload.user_leds_brightness.brightness;

    if (brightness > 255) {
        LOG_ERR("Got user LED brightness value of %u out of range [0,255]",
                brightness);
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got user LED brightness value of %u", brightness);
        front_leds_set_brightness(brightness);
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_distributor_leds_pattern(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_distributor_leds_pattern_tag);

    orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern pattern =
        msg->payload.distributor_leds_pattern.pattern;
    uint32_t mask = msg->payload.distributor_leds_pattern.leds_mask;

    LOG_DBG("Got distributor LED pattern: %u, mask 0x%x", pattern, mask);

    if (mask > OPERATOR_LEDS_ALL_MASK) {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        orb_mcu_main_RgbColor *color_ptr = NULL;
        if (msg->payload.distributor_leds_pattern.pattern ==
            orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_RGB) {
            color_ptr = &msg->payload.distributor_leds_pattern.custom_color;
        }
        if (operator_leds_set_pattern(pattern, mask, color_ptr) !=
            RET_SUCCESS) {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_distributor_leds_brightness(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_distributor_leds_brightness_tag);

    uint32_t brightness = msg->payload.distributor_leds_brightness.brightness;
    if (brightness > 255) {
        LOG_ERR("Got user LED brightness value of %u out of range [0,255]",
                brightness);
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got distributor LED brightness: %u", brightness);
        if (operator_leds_set_brightness((uint8_t)brightness) != RET_SUCCESS) {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_fw_img_crc(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_fw_image_check_tag);

    LOG_DBG("Got CRC comparison");

    // must be static to be used by callback
    static struct handle_error_context_s context = {0};
    context.remote = job->remote;
    context.remote_addr = job->remote_addr;
    context.ack_number = job->ack_number;

    int ret = dfu_secondary_check_async(msg->payload.fw_image_check.crc32,
                                        (void *)&context, handle_err_code);
    if (ret == -EINPROGRESS) {
        return;
    }

    if (ret == RET_ERROR_INVALID_STATE) {
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
    } else {
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_fw_img_sec_activate(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_fw_image_secondary_activate_tag);

    LOG_DBG("Got secondary slot activation");
    int ret;
    if (msg->payload.fw_image_secondary_activate.force_permanent) {
        ret = dfu_secondary_activate_permanently();
    } else {
        ret = dfu_secondary_activate_temporarily();
    }

    if (ret) {
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_fw_img_primary_confirm(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_fw_image_primary_confirm_tag);

    LOG_DBG("Got primary slot confirmation");

    // - orb_mcu_Ack_ErrorCode_FAIL: image self-test didn't end up successful,
    // meaning the image shouldn't be confirmed but reverted by using
    // `FirmwareActivateSecondary`
    // - orb_mcu_Ack_ErrorCode_INVALID_STATE: running image already confirmed
    // - orb_mcu_Ack_ErrorCode_VERSION: version in secondary slot higher than
    // version in primary slot meaning the image has not been installed
    // successfully
    struct image_version secondary_version;
    struct image_version primary_version;
    if (dfu_version_secondary_get(&secondary_version) == RET_SUCCESS) {
        // check that image to be confirmed has higher version than previous
        // image
        dfu_version_primary_get(&primary_version);
        if (primary_version.iv_major < secondary_version.iv_major ||
            (primary_version.iv_major == secondary_version.iv_major &&
             primary_version.iv_minor < secondary_version.iv_minor) ||
            (primary_version.iv_major == secondary_version.iv_major &&
             primary_version.iv_minor == secondary_version.iv_minor &&
             primary_version.iv_revision < secondary_version.iv_revision)) {
            job_ack(orb_mcu_Ack_ErrorCode_VERSION, job);
            return;
        }
    }

    if (dfu_primary_is_confirmed()) {
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
    } else {
        int ret = dfu_primary_confirm();
        if (ret) {
            // consider as self-test not successful: in any case image is not
            // able to run
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_fps(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_fps_tag);

    uint16_t fps = (uint16_t)msg->payload.fps.fps;

    LOG_DBG("Got FPS message = %u", fps);

    ret_code_t ret = ir_camera_system_set_fps(fps);
    switch (ret) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    case RET_ERROR_FORBIDDEN:
        job_ack(orb_mcu_Ack_ErrorCode_FORBIDDEN, job);
        break;
    case RET_ERROR_INVALID_PARAM:
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        break;
    }
}

static void
handle_dfu_block_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_dfu_block_tag);

    // must be static to be used by callback
    static struct handle_error_context_s context = {0};
    context.remote = job->remote;
    context.remote_addr = job->remote_addr;
    context.ack_number = job->ack_number;

    LOG_DBG("Got firmware image block");
    int ret = dfu_load(msg->payload.dfu_block.block_number,
                       msg->payload.dfu_block.block_count,
                       msg->payload.dfu_block.image_block.bytes,
                       msg->payload.dfu_block.image_block.size,
                       (void *)&context, handle_err_code);

    // if the operation is not over,
    // the DFU module will handle acknowledgement
    if (ret == -EINPROGRESS) {
        return;
    }

    switch (ret) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;

    case RET_ERROR_NO_MEM:
        /* internal dfu buffer not processed? */
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;

    case RET_ERROR_INVALID_PARAM:
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        break;

    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
        break;

    default:
        LOG_ERR("Unhandled error code %d", ret);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    };
}

static void
handle_do_mirror_homing(job_t *job)
{
    ret_code_t ret = RET_SUCCESS;
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_do_homing_tag);

    orb_mcu_main_PerformMirrorHoming_Mode mode =
        msg->payload.do_homing.homing_mode;
    orb_mcu_main_PerformMirrorHoming_Angle axis = msg->payload.do_homing.angle;
    LOG_DBG("Got do autohoming message, mode = %u, axis = %u", mode, axis);

    if (mirror_auto_homing_in_progress()) {
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
    } else if (mode == orb_mcu_main_PerformMirrorHoming_Mode_STALL_DETECTION) {
        job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
    } else {
#if defined(CONFIG_BOARD_PEARL_MAIN)
        if (mode == orb_mcu_main_PerformMirrorHoming_Mode_ONE_BLOCKING_END) {
            if (axis == orb_mcu_main_PerformMirrorHoming_Angle_BOTH ||
                axis == orb_mcu_main_PerformMirrorHoming_Angle_HORIZONTAL_PHI) {
                const motor_t motor = MOTOR_PHI_ANGLE;
                ret |= mirror_autohoming(&motor);
            }
            if (axis == orb_mcu_main_PerformMirrorHoming_Angle_BOTH ||
                axis == orb_mcu_main_PerformMirrorHoming_Angle_VERTICAL_THETA) {
                const motor_t motor = MOTOR_THETA_ANGLE;
                ret |= mirror_autohoming(&motor);
            }
        } else if (
            mode ==
            orb_mcu_main_PerformMirrorHoming_Mode_WITH_KNOWN_COORDINATES) {

        } else {
            job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
            return;
        }
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
        if (mode == orb_mcu_main_PerformMirrorHoming_Mode_ONE_BLOCKING_END) {
            ret = mirror_autohoming(NULL);
        } else if (
            mode ==
            orb_mcu_main_PerformMirrorHoming_Mode_WITH_KNOWN_COORDINATES) {
            ret = mirror_go_home();
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
            return;
        }
#endif

        // send ack before timeout even though auto-homing not completed
        if (ret) {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_liquid_lens(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_liquid_lens_tag);

    const struct device *ll_dev = DEVICE_DT_GET(DT_NODELABEL(liquid_lens));

    int32_t current = msg->payload.liquid_lens.current;
    bool enable = msg->payload.liquid_lens.enable;

    if (!device_is_ready(ll_dev)) {
        LOG_ERR("Liquid lens device not ready");
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        return;
    }

    if (!IN_RANGE(current, LIQUID_LENS_MIN_CURRENT_MA,
                  LIQUID_LENS_MAX_CURRENT_MA)) {
        LOG_ERR("%d out of range [%d,%d]", current, LIQUID_LENS_MIN_CURRENT_MA,
                LIQUID_LENS_MAX_CURRENT_MA);
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Value: %d", current);
        int err = liquid_lens_set_target_current(ll_dev, current);

        if (err == 0) {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
            if (enable) {
                liquid_lens_enable(ll_dev);
            } else {
                liquid_lens_disable(ll_dev);
            }
        } else if (err == -EBUSY) {
            job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
            LOG_ERR("Unhandled: %d!", err);
        }
    }
}

static void
handle_power_cycle(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_power_cycle_tag);

    const int ret = power_cycle_supply(msg->payload.power_cycle.line,
                                       msg->payload.power_cycle.duration_ms);

    switch (ret) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_NOT_FOUND:
        job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
        break;
    case RET_ERROR_INVALID_PARAM:
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        break;
    case RET_ERROR_FORBIDDEN:
        job_ack(orb_mcu_Ack_ErrorCode_FORBIDDEN, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error code %d", ret);
    }
}

#ifdef CONFIG_BOARD_DIAMOND_MAIN
static void
handle_polarizer(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_polarizer_tag);

    ret_code_t err_code;

    uint32_t frequency_usteps_per_second =
        msg->payload.polarizer.speed == 0
            ? POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT
            : POLARIZER_MICROSTEPS_PER_SECOND(msg->payload.polarizer.speed);

    switch (msg->payload.polarizer.command) {
    case orb_mcu_main_Polarizer_Command_POLARIZER_HOME: {
        err_code = polarizer_wheel_home_async();
        if (err_code == RET_SUCCESS) {
            job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        } else if (err_code == RET_ERROR_BUSY) {
            job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
        } else {
            // no wheel detected during homing or module not initialized
            job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        }
        return;
    } break;
    case orb_mcu_main_Polarizer_Command_POLARIZER_PASS_THROUGH:
        err_code = polarizer_wheel_set_angle(
            frequency_usteps_per_second,
            POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE);
        break;
    case orb_mcu_main_Polarizer_Command_POLARIZER_0_HORIZONTAL:
        err_code = polarizer_wheel_set_angle(
            frequency_usteps_per_second,
            POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE);
        break;
    case orb_mcu_main_Polarizer_Command_POLARIZER_90_VERTICAL:
        err_code = polarizer_wheel_set_angle(
            frequency_usteps_per_second,
            POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
        break;
    case orb_mcu_main_Polarizer_Command_POLARIZER_CUSTOM_ANGLE:
        err_code =
            polarizer_wheel_set_angle(frequency_usteps_per_second,
                                      msg->payload.polarizer.angle_decidegrees);
        break;
    default:
        // not implemented yet
        job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
        return;
    }

    if (err_code == RET_SUCCESS) {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    } else if (err_code == RET_ERROR_BUSY) {
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
    } else if (err_code == RET_ERROR_INVALID_PARAM) {
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
    } else {
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}
#endif

static void
handle_voltage_request(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_voltage_request_tag);

    uint32_t transmit_period_ms =
        msg->payload.voltage_request.transmit_period_ms;

    voltage_measurement_set_publish_period(transmit_period_ms);

    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_heartbeat(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_heartbeat_tag);

    LOG_DBG("Got heartbeat");
    int ret = heartbeat_boom(msg->payload.heartbeat.timeout_seconds);

    if (ret == RET_SUCCESS) {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    } else {
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_mirror_angle_relative_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_mirror_angle_relative_tag);

    int32_t mirror_relative_angle_phi_millidegrees;
    int32_t mirror_relative_angle_theta_millidegrees;

    switch (msg->payload.mirror_angle_relative.angle_type) {
    case orb_mcu_main_MirrorAngleType_HORIZONTAL_VERTICAL: {
        int32_t relative_angle_horizontal_millidegrees =
            msg->payload.mirror_angle_relative.horizontal_angle;
        int32_t relative_angle_vertical_millidegrees =
            msg->payload.mirror_angle_relative.vertical_angle;
        mirror_relative_angle_phi_millidegrees =
            -(relative_angle_horizontal_millidegrees / 2);
        mirror_relative_angle_theta_millidegrees =
            relative_angle_vertical_millidegrees / 2;
        LOG_DBG(
            "Got relative mirror angle message, vertical: %d, horizontal: %d",
            relative_angle_vertical_millidegrees,
            relative_angle_horizontal_millidegrees);
    } break;
    case orb_mcu_main_MirrorAngleType_PHI_THETA:
        mirror_relative_angle_phi_millidegrees =
            msg->payload.mirror_angle_relative.phi_angle_millidegrees;
        mirror_relative_angle_theta_millidegrees =
            msg->payload.mirror_angle_relative.theta_angle_millidegrees;
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        return;
    }

    if (mirror_auto_homing_in_progress()) {
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
        return;
    }

    LOG_DBG("Got relative mirror angle message, theta: %d, phi: %d",
            mirror_relative_angle_theta_millidegrees,
            mirror_relative_angle_phi_millidegrees);

    ret_code_t ret =
        mirror_set_angle_phi_relative(mirror_relative_angle_phi_millidegrees);
    if (ret == RET_SUCCESS) {
        ret = mirror_set_angle_theta_relative(
            mirror_relative_angle_theta_millidegrees);
    }

    if (ret != RET_SUCCESS) {
        if (ret == RET_ERROR_INVALID_PARAM) {
            job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        } else {
            job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        }
        return;
    }

    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_value_get_message(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_value_get_tag);

    front_leds_boot_progress_set(BOOT_PROGRESS_STEP_JETSON_VALUEGET);

    orb_mcu_ValueGet_Value value = msg->payload.value_get.value;
    LOG_DBG("Got ValueGet request: %u", value);

    switch (value) {
    case orb_mcu_ValueGet_Value_FIRMWARE_VERSIONS:
        version_fw_send(job->remote_addr);
        break;
    case orb_mcu_ValueGet_Value_HARDWARE_VERSIONS:
        version_hw_send(job->remote_addr);
        break;
    case orb_mcu_ValueGet_Value_CONE_PRESENT:
        ui_cone_present_send(job->remote_addr);
        break;
    default: {
        // unknown value, respond with error
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        return;
    }
    }

    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_ir_eye_camera_focus_sweep_lens_values(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(
        orb_mcu_main_JetsonToMcu_ir_eye_camera_focus_sweep_lens_values_tag);

    BUILD_ASSERT(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
                 "We assume we are little endian");
    int16_t *focus_values =
        (int16_t *)msg->payload.ir_eye_camera_focus_sweep_lens_values
            .focus_values.bytes;
    size_t num_focus_values =
        msg->payload.ir_eye_camera_focus_sweep_lens_values.focus_values.size /
        sizeof(*focus_values);

    ret_code_t ret = ir_camera_system_set_focus_values_for_focus_sweep(
        focus_values, num_focus_values);

    switch (ret) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    case RET_ERROR_INVALID_PARAM:
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", ret);
    }
}

static void
handle_ir_eye_camera_focus_sweep_values_polynomial(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(
        orb_mcu_main_JetsonToMcu_ir_eye_camera_focus_sweep_values_polynomial_tag);

    orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial p =
        msg->payload.ir_eye_camera_focus_sweep_values_polynomial;
    LOG_DBG("a: %f, b: %f, c: %f, d: %f, e: %f, f: %f, num frames: %u",
            (double)p.coef_a, (double)p.coef_b, (double)p.coef_c,
            (double)p.coef_d, (double)p.coef_e, (double)p.coef_f,
            p.number_of_frames);
    ret_code_t err =
        ir_camera_system_set_polynomial_coefficients_for_focus_sweep(
            msg->payload.ir_eye_camera_focus_sweep_values_polynomial);
    switch (err) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_perform_ir_eye_camera_focus_sweep(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(
        orb_mcu_main_JetsonToMcu_perform_ir_eye_camera_focus_sweep_tag);

    ret_code_t ret = ir_camera_system_perform_focus_sweep();

    if (ret == RET_ERROR_BUSY) {
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
    } else if (ret == RET_ERROR_INVALID_STATE) {
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
    } else if (ret == RET_ERROR_FORBIDDEN) {
        job_ack(orb_mcu_Ack_ErrorCode_FORBIDDEN, job);
    } else if (ret == RET_SUCCESS) {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    } else {
        LOG_ERR("Unexpected error code (%d)!", ret);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_ir_eye_camera_mirror_sweep_values_polynomial(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(
        orb_mcu_main_JetsonToMcu_ir_eye_camera_mirror_sweep_values_polynomial_tag);

    orb_mcu_main_IREyeCameraMirrorSweepValuesPolynomial p =
        msg->payload.ir_eye_camera_mirror_sweep_values_polynomial;
    LOG_DBG(
        "r_a: %f, r_b: %f, r_c: %f, a_a: %f, a_b: %f, a_c: %f, num frames: %u",
        (double)p.radius_coef_a, (double)p.radius_coef_b,
        (double)p.radius_coef_c, (double)p.angle_coef_a, (double)p.angle_coef_b,
        (double)p.angle_coef_c, p.number_of_frames);
    ret_code_t err =
        ir_camera_system_set_polynomial_coefficients_for_mirror_sweep(
            msg->payload.ir_eye_camera_mirror_sweep_values_polynomial);
    switch (err) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_perform_ir_eye_camera_mirror_sweep(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(
        orb_mcu_main_JetsonToMcu_perform_ir_eye_camera_mirror_sweep_tag);

    ret_code_t ret = ir_camera_system_perform_mirror_sweep();

    if (ret == RET_ERROR_BUSY) {
        job_ack(orb_mcu_Ack_ErrorCode_IN_PROGRESS, job);
    } else if (ret == RET_ERROR_INVALID_STATE) {
        job_ack(orb_mcu_Ack_ErrorCode_INVALID_STATE, job);
    } else if (ret == RET_ERROR_FORBIDDEN) {
        job_ack(orb_mcu_Ack_ErrorCode_FORBIDDEN, job);
    } else if (ret == RET_SUCCESS) {
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
    } else {
        LOG_ERR("Unexpected error code (%d)!", ret);
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

#if defined(CONFIG_MEMFAULT_METRICS_CONNECTIVITY_CONNECTED_TIME)

/**
 * @brief Sets the Orb connection state to disconnected.
 */
static void
connection_lost_work_handler(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    LOG_INF("Connection lost");

    memfault_metrics_connectivity_connected_state_change(
        kMemfaultMetricsConnectivityState_ConnectionLost);
}

static K_WORK_DEFINE(connection_lost_work, connection_lost_work_handler);

/**
 * ⚠️ ISR
 *
 * Timer expires when the Orb is disconnected from the internet
 * (Memfault backend not reachable) because the SyncDiagData
 * message is not received within a few intervals.
 *
 * @param timer not used
 */
static void
diag_disconnected(struct k_timer *timer)
{
    UNUSED_PARAMETER(timer);

    // memfault_metrics_connectivity_connected_state_change uses mutex, cannot
    // be used in ISR, so queue work.
    const int ret = k_work_submit(&connection_lost_work);
    if (ret < 0) {
        ASSERT_SOFT(ret);
    }
}

#endif

/**
 * Handle the sync diag data message
 *
 * Note. Sync diag message is only sent when the Orb is connected to
 * the Internet (to be exact: Memfault backend is reachable)
 * When connectivity metrics is enabled, use this function to track internet
 * connectivity status to Memfault device vitals
 *
 * @param job
 */
static void
handle_sync_diag_data(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_sync_diag_data_tag);

    LOG_DBG("Got sync diag data message");

#if defined(CONFIG_MEMFAULT_METRICS_CONNECTIVITY_CONNECTED_TIME)
    static K_TIMER_DEFINE(orb_connection_timer, diag_disconnected, NULL);

    uint32_t interval = msg->payload.sync_diag_data.interval;
    if (interval) {
        // start / reload the timer, acting as a heartbeat
        // and use it to detect orb's internet
        // connectivity
        k_timer_start(&orb_connection_timer, K_SECONDS(interval * 3),
                      K_SECONDS(interval * 3));
    } else {
        k_timer_stop(&orb_connection_timer);
    }

    memfault_metrics_connectivity_connected_state_change(
        kMemfaultMetricsConnectivityState_Connected);
#endif

    publish_flush();

    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
}

static void
handle_diag_test_data(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_diag_test_tag);

    LOG_DBG("Got diag test data message");

#if defined(BUILD_FROM_CI)
    job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
#else
    job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);

    // some of these won't return
    switch (msg->payload.diag_test.action) {
    case orb_mcu_DiagTest_Action_TRIGGER_WATCHDOG:
        fatal_errors_trigger(FATAL_WATCHDOG);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_ASSERT_SOFT:
        ASSERT_SOFT(RET_ERROR_INTERNAL);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_ASSERT_HARD:
        fatal_errors_trigger(USER_ASSERT_HARD);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_LOG:
        LOG_ERR("Triggered test log");
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_BUSFAULT:
        fatal_errors_trigger(FATAL_BUSFAULT);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_HARDFAULT:
        fatal_errors_trigger(FATAL_ILLEGAL_INSTRUCTION);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_MEMMANAGE:
        fatal_errors_trigger(FATAL_MEMMANAGE);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_USAGEFAULT:
        fatal_errors_trigger(FATAL_ACCESS);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_K_PANIC:
        fatal_errors_trigger(FATAL_K_PANIC);
        break;
    case orb_mcu_DiagTest_Action_TRIGGER_K_OOPS:
        fatal_errors_trigger(FATAL_K_OOPS);
        break;
    }
#endif
}

static void
handle_set_time(job_t *job)
{
    orb_mcu_main_JetsonToMcu *msg = &job->message.jetson_cmd;
    MAKE_ASSERTS(orb_mcu_main_JetsonToMcu_set_time_tag);

    front_leds_boot_progress_set(BOOT_PROGRESS_STEP_DATE_SET);

    int ret;
    switch (msg->payload.set_time.which_format) {
    case orb_mcu_Time_human_readable_tag: {
        const orb_mcu_Time_Date *time =
            &msg->payload.set_time.format.human_readable;
        struct tm tm_time = {
            .tm_year = (int)time->year - 1900,
            .tm_mon = (int)time->month - 1,
            .tm_mday = (int)time->day,
            .tm_hour = (int)time->hour,
            .tm_min = (int)time->min,
            .tm_sec = (int)time->sec,
        };
        ret = date_set_time(&tm_time);
    } break;
    case orb_mcu_Time_epoch_time_tag: {
        const time_t time_epoch = msg->payload.set_time.format.epoch_time;
        ret = date_set_time_epoch(time_epoch);
    } break;
    default:
        ret = RET_ERROR_INVALID_PARAM;
        LOG_ERR("Unhandled set_time type: %u",
                msg->payload.set_time.which_format);
    }

    switch (ret) {
    case RET_SUCCESS:
        job_ack(orb_mcu_Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_INVALID_PARAM:
    case -EINVAL:
        job_ack(orb_mcu_Ack_ErrorCode_RANGE, job);
        break;
    default:
        job_ack(orb_mcu_Ack_ErrorCode_FAIL, job);
    }
}

__maybe_unused static void
handle_not_supported(job_t *job)
{
    LOG_ERR("Message not supported: %u", job->message.jetson_cmd.which_payload);
    job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
}

static void
handle_sec_to_main_ping(job_t *job)
{
    orb_mcu_sec_SecToMain *msg = &job->message.sec_cmd;
    MAKE_ASSERTS(orb_mcu_sec_SecToMain_ping_pong_tag);

    ping_received(&msg->payload.ping_pong);
}

typedef void (*hm_callback)(job_t *job);

// These functions ARE NOT allowed to block!
static const hm_callback handle_sec_message_callbacks[] = {
    [orb_mcu_sec_SecToMain_ping_pong_tag] = handle_sec_to_main_ping,
};

static const hm_callback handle_message_callbacks[] = {
    [orb_mcu_main_JetsonToMcu_shutdown_tag] = handle_shutdown,
    [orb_mcu_main_JetsonToMcu_reboot_tag] = handle_reboot_message,
    [orb_mcu_main_JetsonToMcu_mirror_angle_tag] = handle_mirror_angle_message,
    [orb_mcu_main_JetsonToMcu_do_homing_tag] = handle_do_mirror_homing,
    [orb_mcu_main_JetsonToMcu_infrared_leds_tag] = handle_infrared_leds_message,
    [orb_mcu_main_JetsonToMcu_led_on_time_tag] = handle_led_on_time_message,
    [orb_mcu_main_JetsonToMcu_user_leds_pattern_tag] = handle_user_leds_pattern,
    [orb_mcu_main_JetsonToMcu_user_leds_brightness_tag] =
        handle_user_leds_brightness,
    [orb_mcu_main_JetsonToMcu_distributor_leds_pattern_tag] =
        handle_distributor_leds_pattern,
    [orb_mcu_main_JetsonToMcu_distributor_leds_brightness_tag] =
        handle_distributor_leds_brightness,
    [orb_mcu_main_JetsonToMcu_dfu_block_tag] = handle_dfu_block_message,
    [orb_mcu_main_JetsonToMcu_start_triggering_ir_eye_camera_tag] =
        handle_start_triggering_ir_eye_camera_message,
    [orb_mcu_main_JetsonToMcu_stop_triggering_ir_eye_camera_tag] =
        handle_stop_triggering_ir_eye_camera_message,
    [orb_mcu_main_JetsonToMcu_start_triggering_ir_face_camera_tag] =
        handle_start_triggering_ir_face_camera_message,
    [orb_mcu_main_JetsonToMcu_stop_triggering_ir_face_camera_tag] =
        handle_stop_triggering_ir_face_camera_message,
    [orb_mcu_main_JetsonToMcu_start_triggering_2dtof_camera_tag] =
        handle_start_triggering_2dtof_camera_message,
    [orb_mcu_main_JetsonToMcu_stop_triggering_2dtof_camera_tag] =
        handle_stop_triggering_2dtof_camera_message,
    [orb_mcu_main_JetsonToMcu_temperature_sample_period_tag] =
        handle_temperature_sample_period_message,
    [orb_mcu_main_JetsonToMcu_fan_speed_tag] = handle_fan_speed,
    [orb_mcu_main_JetsonToMcu_fps_tag] = handle_fps,
    [orb_mcu_main_JetsonToMcu_liquid_lens_tag] = handle_liquid_lens,
    [orb_mcu_main_JetsonToMcu_voltage_request_tag] = handle_voltage_request,
    [orb_mcu_main_JetsonToMcu_fw_image_check_tag] = handle_fw_img_crc,
    [orb_mcu_main_JetsonToMcu_fw_image_secondary_activate_tag] =
        handle_fw_img_sec_activate,
    [orb_mcu_main_JetsonToMcu_heartbeat_tag] = handle_heartbeat,
    [orb_mcu_main_JetsonToMcu_mirror_angle_relative_tag] =
        handle_mirror_angle_relative_message,
    [orb_mcu_main_JetsonToMcu_value_get_tag] = handle_value_get_message,
    [orb_mcu_main_JetsonToMcu_center_leds_sequence_tag] =
        handle_user_center_leds_sequence,
    [orb_mcu_main_JetsonToMcu_distributor_leds_sequence_tag] =
        handle_distributor_leds_sequence,
    [orb_mcu_main_JetsonToMcu_ring_leds_sequence_tag] =
        handle_user_ring_leds_sequence,
    [orb_mcu_main_JetsonToMcu_fw_image_primary_confirm_tag] =
        handle_fw_img_primary_confirm,
    [orb_mcu_main_JetsonToMcu_ir_eye_camera_focus_sweep_lens_values_tag] =
        handle_ir_eye_camera_focus_sweep_lens_values,
    [orb_mcu_main_JetsonToMcu_ir_eye_camera_focus_sweep_values_polynomial_tag] =
        handle_ir_eye_camera_focus_sweep_values_polynomial,
    [orb_mcu_main_JetsonToMcu_perform_ir_eye_camera_focus_sweep_tag] =
        handle_perform_ir_eye_camera_focus_sweep,
    [orb_mcu_main_JetsonToMcu_ir_eye_camera_mirror_sweep_values_polynomial_tag] =
        handle_ir_eye_camera_mirror_sweep_values_polynomial,
    [orb_mcu_main_JetsonToMcu_perform_ir_eye_camera_mirror_sweep_tag] =
        handle_perform_ir_eye_camera_mirror_sweep,
    [orb_mcu_main_JetsonToMcu_sync_diag_data_tag] = handle_sync_diag_data,
    [orb_mcu_main_JetsonToMcu_diag_test_tag] = handle_diag_test_data,
    [orb_mcu_main_JetsonToMcu_power_cycle_tag] = handle_power_cycle,
    [orb_mcu_main_JetsonToMcu_set_time_tag] = handle_set_time,
    [orb_mcu_main_JetsonToMcu_reboot_orb_tag] = handle_reboot_orb,
    [orb_mcu_main_JetsonToMcu_boot_complete_tag] = handle_boot_complete,
    [orb_mcu_main_JetsonToMcu_start_triggering_rgb_face_camera_tag] =
        handle_start_triggering_rgb_face_camera_message,
    [orb_mcu_main_JetsonToMcu_stop_triggering_rgb_face_camera_tag] =
        handle_stop_triggering_rgb_face_camera_message,
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    [orb_mcu_main_JetsonToMcu_cone_leds_sequence_tag] =
        handle_cone_leds_sequence,
    [orb_mcu_main_JetsonToMcu_cone_leds_pattern_tag] = handle_cone_leds_pattern,
    [orb_mcu_main_JetsonToMcu_white_leds_brightness_tag] =
        handle_white_leds_brightness,
    [orb_mcu_main_JetsonToMcu_polarizer_tag] = handle_polarizer,
#elif defined(CONFIG_BOARD_PEARL_MAIN)
    [orb_mcu_main_JetsonToMcu_cone_leds_sequence_tag] = handle_not_supported,
    [orb_mcu_main_JetsonToMcu_cone_leds_pattern_tag] = handle_not_supported,
    [orb_mcu_main_JetsonToMcu_white_leds_brightness_tag] = handle_not_supported,
#else
#error "Board not supported"
#endif
};

BUILD_ASSERT((ARRAY_SIZE(handle_message_callbacks) <= 55),
             "It seems like the `handle_message_callbacks` array is too large");

_Noreturn static void
runner_process_jobs_thread()
{
    job_t new = {0};
    int ret;

    while (1) {
        memset(&new, 0, sizeof(new));
        ret = k_msgq_get(&process_queue, &new, K_FOREVER);
        if (ret != 0) {
            ASSERT_SOFT(ret);
            continue;
        }

        // filter out jobs from UART for debugging
        if (new.remote_addr != 0) {
            LOG_DBG("⬇️ Received message from remote 0x%03x with payload ID "
                    "%02d, ack #%u",
                    new.remote_addr,
                    new.remote == CAN_SEC_MCU_MESSAGING
                        ? new.message.sec_cmd.which_payload
                        : new.message.jetson_cmd.which_payload,
                    new.ack_number);

            // allow response to this remote
            subscribe_add(new.remote_addr);
        }

        if (new.remote == CAN_SEC_MCU_MESSAGING) {
            if (new.message.sec_cmd.which_payload <
                    ARRAY_SIZE(handle_sec_message_callbacks) &&
                handle_sec_message_callbacks[new.message.sec_cmd
                                                 .which_payload] != NULL) {
                handle_sec_message_callbacks[new.message.sec_cmd.which_payload](
                    &new);
            } else {
                LOG_ERR("A handler for security message with ID of %d is not "
                        "implemented (remote 0x%03x, ack #%u)",
                        new.message.sec_cmd.which_payload, new.remote_addr,
                        new.ack_number);
                job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, &new);
            }
        } else {
            if (new.message.jetson_cmd.which_payload <
                    ARRAY_SIZE(handle_message_callbacks) &&
                handle_message_callbacks[new.message.jetson_cmd
                                             .which_payload] != NULL) {
                handle_message_callbacks[new.message.jetson_cmd.which_payload](
                    &new);
            } else {
                LOG_ERR("A handler for message with a payload ID of %d is not "
                        "implemented (remote 0x%03x, ack #%u)",
                        new.message.jetson_cmd.which_payload, new.remote_addr,
                        new.ack_number);
                job_ack(orb_mcu_Ack_ErrorCode_OPERATION_NOT_SUPPORTED, &new);
            }
        }
    }
}

static K_SEM_DEFINE(new_job_sem, 1, 1);
static job_t new = {0};
static orb_mcu_McuMessage mcu_message;

ret_code_t
runner_handle_new_cli(const orb_mcu_main_JetsonToMcu *const message)
{
    ret_code_t err_code = RET_SUCCESS;

    int ret = k_sem_take(&new_job_sem, K_MSEC(5));
    if (ret == 0) {
        new.remote = CLI;
        new.message.jetson_cmd = *message;
        new.remote_addr = 0;
        new.ack_number = 0;
        ret = k_msgq_put(&process_queue, &new, K_MSEC(5));
        if (ret) {
            ASSERT_SOFT(ret);
            err_code = RET_ERROR_BUSY;
        }
    }
    k_sem_give(&new_job_sem);

    return err_code;
}

ret_code_t
runner_handle_new_can(can_message_t *msg)
{
    ret_code_t err_code = RET_SUCCESS;
    can_message_t *can_msg = (can_message_t *)msg;

    if (runner_tid == NULL) {
        LOG_ERR("Runner thread is not running");
        return RET_ERROR_INVALID_STATE;
    }

    pb_istream_t stream = pb_istream_from_buffer(can_msg->bytes, can_msg->size);

    int ret = k_sem_take(&new_job_sem, K_MSEC(5));
    if (ret == 0) {
        bool decoded = pb_decode_ex(&stream, orb_mcu_McuMessage_fields,
                                    &mcu_message, PB_DECODE_DELIMITED);
        if (decoded) {
            if (mcu_message.which_message == orb_mcu_McuMessage_j_message_tag) {
                // Handle Jetson messages
                new.remote = CAN_JETSON_MESSAGING;
                new.message.jetson_cmd = mcu_message.message.j_message;
                new.ack_number = mcu_message.message.j_message.ack_number;

                if (can_msg->destination & CAN_ADDR_IS_ISOTP) {
                    // keep flags of the received message destination
                    // & invert source and destination
                    new.remote_addr = (can_msg->destination & ~0xFF);
                    new.remote_addr |= (can_msg->destination & 0xF) << 4;
                    new.remote_addr |= (can_msg->destination & 0xF0) >> 4;
                } else {
                    new.remote_addr = CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX;
                }

                ret = k_msgq_put(&process_queue, &new, K_MSEC(5));
                if (ret) {
                    ASSERT_SOFT(ret);
                    err_code = RET_ERROR_BUSY;
                }
            } else if (mcu_message.which_message ==
                       orb_mcu_McuMessage_sec_to_main_message_tag) {
                // Handle messages from security MCU
                new.remote = CAN_SEC_MCU_MESSAGING;
                new.message.sec_cmd = mcu_message.message.sec_to_main_message;
                new.ack_number = 0; // no ack for mcu-to-mcu comms
                new.remote_addr = CONFIG_CAN_ADDRESS_MCU_TO_MCU_TX;

                ret = k_msgq_put(&process_queue, &new, K_MSEC(5));
                if (ret) {
                    ASSERT_SOFT(ret);
                    err_code = RET_ERROR_BUSY;
                }
            } else {
                LOG_INF("Got message not intended for us. Dropping.");
                err_code = RET_ERROR_INVALID_ADDR;
            }
        } else {
            LOG_ERR("Unable to decode %s", PB_GET_ERROR(&stream));
            err_code = RET_ERROR_INVALID_PARAM;
        }

        k_sem_give(&new_job_sem);
    } else {
        LOG_ERR("Handling busy (CAN): %d", ret);
        err_code = RET_ERROR_BUSY;
    }

    return err_code;
}

#if CONFIG_ORB_LIB_UART_MESSAGING

static uart_message_t *uart_msg;

static bool
buf_read_circular(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    if (buf == NULL) {
        return false;
    }

    // get source address from previous state
    pb_byte_t *source = (pb_byte_t *)stream->state;
    // pointer to future source in the circular buffer once `count`
    // bytes are copied
    const pb_byte_t *end_ptr =
        (pb_byte_t *)((size_t)uart_msg->buffer_addr +
                      ((((size_t)source - (size_t)uart_msg->buffer_addr) +
                        count) %
                       (size_t)uart_msg->buffer_size));
    size_t copy_idx = 0;

    // if wrap around in the circular buffer, copy end of the buffer first
    if (end_ptr < source) {
        copy_idx =
            (size_t)((uart_msg->buffer_addr + uart_msg->buffer_size) - source);

        memcpy(buf, source, copy_idx);

        source = (pb_byte_t *)uart_msg->buffer_addr;
        count = count - copy_idx;
    }

    memcpy(&buf[copy_idx], source, count);

    // update next read location
    stream->state = (void *)end_ptr;

    return true;
}

ret_code_t
runner_handle_new_uart(uart_message_t *msg)
{
    ret_code_t err_code = RET_SUCCESS;

    if (runner_tid == NULL) {
        LOG_ERR("Runner thread is not running");
        return RET_ERROR_INVALID_STATE;
    }

    int ret = k_sem_take(&new_job_sem, K_MSEC(5));

#ifdef CONFIG_CI_INTEGRATION_TESTS
    static size_t counter = 0;
    counter++;
    if (counter == 500) {
        counter = 0;
        // some Easter egg to test the communication over UART
        LOG_WRN("My heart is beating");
    }
#endif

    if (ret == 0) {
        uart_msg = (uart_message_t *)msg;
        pb_istream_t stream = pb_istream_from_buffer(
            &uart_msg->buffer_addr[uart_msg->start_idx], uart_msg->length);
        stream.callback = buf_read_circular;

        bool decoded = pb_decode_ex(&stream, orb_mcu_McuMessage_fields,
                                    &mcu_message, PB_DECODE_DELIMITED);
        if (decoded) {
            if (mcu_message.which_message != orb_mcu_McuMessage_j_message_tag) {
                LOG_INF("Got message not intended for us. Dropping.");
                err_code = RET_ERROR_INVALID_ADDR;
            } else {
                new.remote = UART_MESSAGING;
                new.message.jetson_cmd = mcu_message.message.j_message;
                new.remote_addr = 0;
                new.ack_number = 0;
                ret = k_msgq_put(&process_queue, &new, K_MSEC(5));
                if (ret) {
                    ASSERT_SOFT(ret);
                    err_code = RET_ERROR_BUSY;
                }
            }
        } else {
            LOG_ERR("Unable to decode: %s", PB_GET_ERROR(&stream));
            err_code = RET_ERROR_INVALID_PARAM;
        }

        k_sem_give(&new_job_sem);
    } else {
        LOG_ERR("Handling busy (UART): %d", ret);
        err_code = RET_ERROR_BUSY;
    }

    return err_code;
}
#endif

void
runner_init(void)
{
    runner_tid =
        k_thread_create(&runner_process, runner_process_stack,
                        K_THREAD_STACK_SIZEOF(runner_process_stack),
                        (k_thread_entry_t)runner_process_jobs_thread, NULL,
                        NULL, NULL, THREAD_PRIORITY_RUNNER, 0, K_NO_WAIT);
    k_thread_name_set(runner_tid, "runner");

    subscribe_add(
        CONFIG_CAN_ADDRESS_MCU_TO_MCU_TX); // Enable MCU-to-MCU sending

#if defined(CONFIG_MEMFAULT_METRICS_CONNECTIVITY_CONNECTED_TIME)
    memfault_metrics_connectivity_connected_state_change(
        kMemfaultMetricsConnectivityState_Started);
#endif
}
