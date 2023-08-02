#include "runner.h"
#include "app_config.h"
#include "can_messaging.h"
#include "dfu.h"
#include "mcu_messaging.pb.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/liquid_lens/liquid_lens.h"
#include "optics/mirrors/mirrors.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "system/logs.h"
#include "system/version/version.h"
#include "temperature/fan/fan.h"
#include "temperature/sensors/temperature.h"
#include "ui/operator_leds/operator_leds.h"
#include "ui/rgb_leds.h"
#include <app_assert.h>
#include <heartbeat.h>
#include <pb_decode.h>
#include <stdlib.h>
#include <uart_messaging.h>
#include <ui/front_leds/front_leds.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(runner, CONFIG_RUNNER_LOG_LEVEL);

K_THREAD_STACK_DEFINE(runner_process_stack, THREAD_STACK_SIZE_RUNNER);
static struct k_thread runner_process;
static k_tid_t runner_tid = NULL;

#define MAKE_ASSERTS(tag) ASSERT_SOFT_BOOL(msg->which_payload == tag)

static uint32_t job_counter = 0;

enum remote_type_e {
    CAN_MESSAGING,
    UART_MESSAGING,
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
    JetsonToMcu message;
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
    // ack only messages sent using CAN
    if (job->remote == CAN_MESSAGING) {
        // get ack number from job
        uint32_t ack_number = job->ack_number;

        Ack ack = {.ack_number = ack_number, .error = error};

        publish_new(&ack, sizeof(ack), McuToJetson_ack_tag, job->remote_addr);
    }

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

    if (context->remote == CAN_MESSAGING) {
        Ack ack = {.ack_number = context->ack_number,
                   .error = Ack_ErrorCode_FAIL};
        switch (err) {
        case RET_SUCCESS:
            ack.error = Ack_ErrorCode_SUCCESS;
            break;

        case RET_ERROR_INVALID_PARAM:
        case RET_ERROR_NOT_FOUND:
            ack.error = Ack_ErrorCode_RANGE;
            break;

        case RET_ERROR_BUSY:
        case RET_ERROR_INVALID_STATE:
            ack.error = Ack_ErrorCode_IN_PROGRESS;
            break;

        case RET_ERROR_FORBIDDEN:
            ack.error = Ack_ErrorCode_OPERATION_NOT_SUPPORTED;
            break;

        default:
            /* nothing */
            break;
        }

        publish_new(&ack, sizeof(ack), McuToJetson_ack_tag,
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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_infrared_leds_tag);

    InfraredLEDs_Wavelength wavelength = msg->payload.infrared_leds.wavelength;

    LOG_DBG("Got LED wavelength message = %d", wavelength);
    ret_code_t err = ir_camera_system_enable_leds(wavelength);
    switch (err) {
    case RET_SUCCESS:
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_led_on_time_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_led_on_time_tag);

    uint32_t on_time_us = msg->payload.led_on_time.on_duration_us;

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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_led_on_time_740nm_tag);

    uint32_t on_time_us = msg->payload.led_on_time_740nm.on_duration_us;

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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_eye_camera_tag);

    ret_code_t err = ir_camera_system_enable_ir_eye_camera();
    switch (err) {
    case RET_SUCCESS:
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_stop_triggering_ir_eye_camera_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_eye_camera_tag);

    ret_code_t err = ir_camera_system_disable_ir_eye_camera();

    switch (err) {
    case RET_SUCCESS:
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_start_triggering_ir_face_camera_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_start_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_ir_face_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_ir_face_camera_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_ir_face_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_ir_face_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_start_triggering_2dtof_camera_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_start_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_enable_2d_tof_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_stop_triggering_2dtof_camera_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_stop_triggering_2dtof_camera_tag);

    LOG_DBG("");
    ir_camera_system_disable_2d_tof_camera();
    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_shutdown(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_shutdown_tag);

    uint32_t delay = msg->payload.shutdown.delay_s;
    LOG_DBG("Got shutdown in %us", delay);

    if (delay > 30) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        int ret = reboot(delay);

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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_reboot_tag);

    uint32_t delay = msg->payload.reboot.delay;

    LOG_DBG("Got reboot in %us", delay);

    if (delay > 60) {
        job_ack(Ack_ErrorCode_RANGE, job);
        LOG_ERR("Reboot with delay > 60 seconds: %u", delay);
    } else {
        int ret = reboot(delay);

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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_mirror_angle_tag);

    uint32_t horizontal_angle = msg->payload.mirror_angle.horizontal_angle;
    int32_t vertical_angle = msg->payload.mirror_angle.vertical_angle;

    if (mirrors_auto_homing_in_progress()) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
        return;
    }

    if (horizontal_angle > MIRRORS_ANGLE_HORIZONTAL_MAX ||
        horizontal_angle < MIRRORS_ANGLE_HORIZONTAL_MIN) {
        LOG_ERR("Horizontal angle of %u out of range [%u;%u]", horizontal_angle,
                MIRRORS_ANGLE_HORIZONTAL_MIN, MIRRORS_ANGLE_HORIZONTAL_MAX);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }

    if (vertical_angle > MIRRORS_ANGLE_VERTICAL_MAX ||
        vertical_angle < MIRRORS_ANGLE_VERTICAL_MIN) {
        LOG_ERR("Vertical angle of %d out of range [%d;%d]", vertical_angle,
                MIRRORS_ANGLE_VERTICAL_MIN, MIRRORS_ANGLE_VERTICAL_MAX);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }

    LOG_DBG("Got mirror angle message, vert: %d, horiz: %u", vertical_angle,
            horizontal_angle);

    if (mirrors_angle_horizontal(horizontal_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else if (mirrors_angle_vertical(vertical_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_temperature_sample_period_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_temperature_sample_period_tag);

    uint32_t sample_period_ms =
        msg->payload.temperature_sample_period.sample_period_ms;

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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_fan_speed_tag);

    // value and percentage have the same representation,
    // so there's no point switching on which one
    uint32_t fan_speed = msg->payload.fan_speed.payload.value;

    if (temperature_is_in_overtemp()) {
        LOG_WRN("Overtemperature: fan speed command rejected");
        job_ack(Ack_ErrorCode_OVER_TEMPERATURE, job);
    } else {
        switch (msg->payload.fan_speed.which_payload) {
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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_user_leds_pattern_tag);

    UserLEDsPattern_UserRgbLedPattern pattern =
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
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        RgbColor *color_ptr = NULL;
        if (msg->payload.user_leds_pattern.pattern ==
                UserLEDsPattern_UserRgbLedPattern_RGB ||
            msg->payload.user_leds_pattern.pattern ==
                UserLEDsPattern_UserRgbLedPattern_PULSING_RGB) {
            color_ptr = &msg->payload.user_leds_pattern.custom_color;
        }
        ret_code_t ret =
            front_leds_set_pattern(pattern, start_angle, angle_length,
                                   color_ptr, pulsing_period_ms, pulsing_scale);

        job_ack(ret == RET_SUCCESS ? Ack_ErrorCode_SUCCESS : Ack_ErrorCode_FAIL,
                job);
    }
}

static void
handle_user_center_leds_sequence(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_center_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format = msg->payload.center_leds_sequence.which_data_format;

    switch (data_format) {
    case UserCenterLEDsSequence_rgb_uncompressed_tag:;
        uint8_t *bytes = msg->payload.center_leds_sequence.data_format
                             .rgb_uncompressed.bytes;
        uint32_t size =
            msg->payload.center_leds_sequence.data_format.rgb_uncompressed.size;

        ret = front_leds_set_center_leds_sequence(bytes, size);
        job_ack(ret == RET_SUCCESS ? Ack_ErrorCode_SUCCESS : Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_user_ring_leds_sequence(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_ring_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format = msg->payload.ring_leds_sequence.which_data_format;

    switch (data_format) {
    case UserRingLEDsSequence_rgb_uncompressed_tag:;
        uint8_t *bytes =
            msg->payload.ring_leds_sequence.data_format.rgb_uncompressed.bytes;
        uint32_t size =
            msg->payload.ring_leds_sequence.data_format.rgb_uncompressed.size;

        ret = front_leds_set_ring_leds_sequence(bytes, size);
        job_ack(ret == RET_SUCCESS ? Ack_ErrorCode_SUCCESS : Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_distributor_leds_sequence(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_sequence_tag);

    ret_code_t ret;
    uint32_t data_format =
        msg->payload.distributor_leds_sequence.which_data_format;

    switch (data_format) {
    case DistributorLEDsSequence_rgb_uncompressed_tag:;
        uint8_t *bytes = msg->payload.distributor_leds_sequence.data_format
                             .rgb_uncompressed.bytes;
        uint32_t size = msg->payload.distributor_leds_sequence.data_format
                            .rgb_uncompressed.size;

        ret = operator_leds_set_leds_sequence(bytes, size);
        job_ack(ret == RET_SUCCESS ? Ack_ErrorCode_SUCCESS : Ack_ErrorCode_FAIL,
                job);
        break;
    default:
        LOG_WRN("Unkown data format: %" PRIu32, data_format);
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_user_leds_brightness(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_user_leds_brightness_tag);

    uint32_t brightness = msg->payload.user_leds_brightness.brightness;

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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_pattern_tag);

    DistributorLEDsPattern_DistributorRgbLedPattern pattern =
        msg->payload.distributor_leds_pattern.pattern;
    uint32_t mask = msg->payload.distributor_leds_pattern.leds_mask;

    LOG_DBG("Got distributor LED pattern: %u, mask 0x%x", pattern, mask);

    if (mask > OPERATOR_LEDS_ALL_MASK) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        RgbColor *color_ptr = NULL;
        if (msg->payload.distributor_leds_pattern.pattern ==
            DistributorLEDsPattern_DistributorRgbLedPattern_RGB) {
            color_ptr = &msg->payload.distributor_leds_pattern.custom_color;
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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_distributor_leds_brightness_tag);

    uint32_t brightness = msg->payload.distributor_leds_brightness.brightness;
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
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_fw_image_check_tag);

    LOG_DBG("Got CRC comparison");
    int ret = dfu_secondary_check(msg->payload.fw_image_check.crc32);
    if (ret) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_fw_img_sec_activate(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_fw_image_secondary_activate_tag);

    LOG_DBG("Got secondary slot activation");
    int ret;
    if (msg->payload.fw_image_secondary_activate.force_permanent) {
        ret = dfu_secondary_activate_permanently();
    } else {
        ret = dfu_secondary_activate_temporarily();
    }

    if (ret) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_fw_img_primary_confirm(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_fw_image_primary_confirm_tag);

    LOG_DBG("Got primary slot confirmation");

    // - Ack_ErrorCode_FAIL: image self-test didn't end up successful, meaning
    // the image shouldn't be confirmed but reverted by using
    // `FirmwareActivateSecondary`
    // - Ack_ErrorCode_INVALID_STATE: running image already confirmed
    // - Ack_ErrorCode_VERSION: version in secondary slot higher than version in
    // primary slot meaning the image has not been installed successfully
    struct image_version secondary_version;
    struct image_version primary_version;
    if (dfu_version_secondary_get(&secondary_version) == 0) {
        // check that image to be confirmed has higher version than previous
        // image
        dfu_version_primary_get(&primary_version);
        if (primary_version.iv_major < secondary_version.iv_major ||
            (primary_version.iv_major == secondary_version.iv_major &&
             primary_version.iv_minor < secondary_version.iv_minor) ||
            (primary_version.iv_major == secondary_version.iv_major &&
             primary_version.iv_minor == secondary_version.iv_minor &&
             primary_version.iv_revision < secondary_version.iv_revision)) {
            job_ack(Ack_ErrorCode_VERSION, job);
            return;
        }
    }

    if (dfu_primary_is_confirmed()) {
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
    } else {
        int ret = dfu_primary_confirm();
        if (ret) {
            // consider as self-test not successful: in any case image is not
            // able to run
            job_ack(Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_fps(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_fps_tag);

    uint16_t fps = (uint16_t)msg->payload.fps.fps;

    LOG_DBG("Got FPS message = %u", fps);

    ret_code_t ret = ir_camera_system_set_fps(fps);
    if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else if (ret == RET_ERROR_INVALID_PARAM) {
        job_ack(Ack_ErrorCode_RANGE, job);
    } else if (ret == RET_ERROR_BUSY) {
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
    } else {
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_dfu_block_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_dfu_block_tag);

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
    ret_code_t ret = RET_SUCCESS;
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_do_homing_tag);

    PerformMirrorHoming_Mode mode = msg->payload.do_homing.homing_mode;
    PerformMirrorHoming_Angle angle = msg->payload.do_homing.angle;
    LOG_DBG("Got do autohoming message, mode = %u, angle = %u", mode, angle);

    if (mirrors_auto_homing_in_progress()) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
    } else if (mode == PerformMirrorHoming_Mode_STALL_DETECTION) {
        job_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED, job);
    } else {
        if (angle == PerformMirrorHoming_Angle_BOTH ||
            angle == PerformMirrorHoming_Angle_HORIZONTAL) {
            ret |= mirrors_auto_homing_one_end(MIRROR_HORIZONTAL_ANGLE);
        }
        if (angle == PerformMirrorHoming_Angle_BOTH ||
            angle == PerformMirrorHoming_Angle_VERTICAL) {
            ret |= mirrors_auto_homing_one_end(MIRROR_VERTICAL_ANGLE);
        }

        // send ack before timeout even though auto-homing not completed
        if (ret) {
            job_ack(Ack_ErrorCode_FAIL, job);
        } else {
            job_ack(Ack_ErrorCode_SUCCESS, job);
        }
    }
}

static void
handle_liquid_lens(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_liquid_lens_tag);

    int32_t current = msg->payload.liquid_lens.current;
    bool enable = msg->payload.liquid_lens.enable;

    if (!IN_RANGE(current, LIQUID_LENS_MIN_CURRENT_MA,
                  LIQUID_LENS_MAX_CURRENT_MA)) {
        LOG_ERR("Got liquid lens current value of %d out of range [%d,%d]",
                current, LIQUID_LENS_MIN_CURRENT_MA,
                LIQUID_LENS_MAX_CURRENT_MA);
        job_ack(Ack_ErrorCode_RANGE, job);
    } else {
        LOG_DBG("Got liquid lens current value of %d", current);
        ret_code_t err = liquid_set_target_current_ma(current);

        switch (err) {
        case RET_SUCCESS:
            job_ack(Ack_ErrorCode_SUCCESS, job);
            if (enable) {
                liquid_lens_enable();
            } else {
                liquid_lens_disable();
            }
            break;
        case RET_ERROR_BUSY:
            job_ack(Ack_ErrorCode_INVALID_STATE, job);
            break;
        default:
            job_ack(Ack_ErrorCode_FAIL, job);
            LOG_ERR("Unhandled error (%d)!", err);
        }
    }
}

static void
handle_heartbeat(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_heartbeat_tag);

    LOG_DBG("Got heartbeat");
    int ret = heartbeat_boom(msg->payload.heartbeat.timeout_seconds);

    if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else {
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_mirror_angle_relative_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_mirror_angle_relative_tag);

    int32_t horizontal_angle =
        msg->payload.mirror_angle_relative.horizontal_angle;
    int32_t vertical_angle = msg->payload.mirror_angle_relative.vertical_angle;

    if (mirrors_auto_homing_in_progress()) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
        return;
    }

    if (abs(horizontal_angle) > MIRRORS_ANGLE_HORIZONTAL_RANGE) {
        LOG_ERR("Horizontal angle of %u out of range (max %u)",
                horizontal_angle, MIRRORS_ANGLE_HORIZONTAL_RANGE);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }
    if (abs(vertical_angle) > MIRRORS_ANGLE_VERTICAL_RANGE) {
        LOG_ERR("Vertical angle of %u out of range (max %u)", vertical_angle,
                MIRRORS_ANGLE_VERTICAL_RANGE);
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }

    LOG_DBG("Got relative mirror angle message, vert: %d, horiz: %u",
            vertical_angle, horizontal_angle);

    if (mirrors_angle_horizontal_relative(horizontal_angle) != RET_SUCCESS ||
        mirrors_angle_vertical_relative(vertical_angle) != RET_SUCCESS) {
        job_ack(Ack_ErrorCode_FAIL, job);
    } else {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    }
}

static void
handle_value_get_message(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_value_get_tag);

    ValueGet_Value value = msg->payload.value_get.value;
    LOG_DBG("Got ValueGet request: %u", value);

    switch (value) {
    case ValueGet_Value_FIRMWARE_VERSIONS:
        version_fw_send(job->remote_addr);
        break;
    case ValueGet_Value_HARDWARE_VERSIONS:
        version_hw_send(job->remote_addr);
        break;
    default: {
        // unknown value, respond with error
        job_ack(Ack_ErrorCode_RANGE, job);
        return;
    }
    }

    job_ack(Ack_ErrorCode_SUCCESS, job);
}

static void
handle_ir_eye_camera_focus_sweep_lens_values(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_ir_eye_camera_focus_sweep_lens_values_tag);

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
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
        break;
    case RET_ERROR_INVALID_PARAM:
        job_ack(Ack_ErrorCode_RANGE, job);
        break;
    default:
        job_ack(Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", ret);
    }
}

static void
handle_ir_eye_camera_focus_sweep_values_polynomial(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_ir_eye_camera_focus_sweep_values_polynomial_tag);

    IREyeCameraFocusSweepValuesPolynomial p =
        msg->payload.ir_eye_camera_focus_sweep_values_polynomial;
    LOG_DBG("a: %f, b: %f, c: %f, d: %f, e: %f, f: %f, num frames: %u",
            p.coef_a, p.coef_b, p.coef_c, p.coef_d, p.coef_e, p.coef_f,
            p.number_of_frames);
    ret_code_t err =
        ir_camera_system_set_polynomial_coefficients_for_focus_sweep(
            msg->payload.ir_eye_camera_focus_sweep_values_polynomial);
    switch (err) {
    case RET_SUCCESS:
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_perform_ir_eye_camera_focus_sweep(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_perform_ir_eye_camera_focus_sweep_tag);

    ret_code_t ret = ir_camera_system_perform_focus_sweep();

    if (ret == RET_ERROR_BUSY) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
    } else if (ret == RET_ERROR_INVALID_STATE) {
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
    } else if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else {
        LOG_ERR("Unexpected error code (%d)!", ret);
        job_ack(Ack_ErrorCode_FAIL, job);
    }
}

static void
handle_ir_eye_camera_mirror_sweep_values_polynomial(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_ir_eye_camera_mirror_sweep_values_polynomial_tag);

    IREyeCameraMirrorSweepValuesPolynomial p =
        msg->payload.ir_eye_camera_mirror_sweep_values_polynomial;
    LOG_DBG(
        "r_a: %f, r_b: %f, r_c: %f, a_a: %f, a_b: %f, a_c: %f, num frames: %u",
        p.radius_coef_a, p.radius_coef_b, p.radius_coef_c, p.angle_coef_a,
        p.angle_coef_b, p.angle_coef_c, p.number_of_frames);
    ret_code_t err =
        ir_camera_system_set_polynomial_coefficients_for_mirror_sweep(
            msg->payload.ir_eye_camera_mirror_sweep_values_polynomial);
    switch (err) {
    case RET_SUCCESS:
        job_ack(Ack_ErrorCode_SUCCESS, job);
        break;
    case RET_ERROR_BUSY:
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
        break;
    default:
        job_ack(Ack_ErrorCode_FAIL, job);
        LOG_ERR("Unhandled error (%d)!", err);
    }
}

static void
handle_perform_ir_eye_camera_mirror_sweep(job_t *job)
{
    JetsonToMcu *msg = &job->message;
    MAKE_ASSERTS(JetsonToMcu_perform_ir_eye_camera_mirror_sweep_tag);

    ret_code_t ret = ir_camera_system_perform_mirror_sweep();

    if (ret == RET_ERROR_BUSY) {
        job_ack(Ack_ErrorCode_IN_PROGRESS, job);
    } else if (ret == RET_ERROR_INVALID_STATE) {
        job_ack(Ack_ErrorCode_INVALID_STATE, job);
    } else if (ret == RET_SUCCESS) {
        job_ack(Ack_ErrorCode_SUCCESS, job);
    } else {
        LOG_ERR("Unexpected error code (%d)!", ret);
        job_ack(Ack_ErrorCode_FAIL, job);
    }
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
    [JetsonToMcu_center_leds_sequence_tag] = handle_user_center_leds_sequence,
    [JetsonToMcu_distributor_leds_sequence_tag] =
        handle_distributor_leds_sequence,
    [JetsonToMcu_ring_leds_sequence_tag] = handle_user_ring_leds_sequence,
    [JetsonToMcu_fw_image_primary_confirm_tag] = handle_fw_img_primary_confirm,
    [JetsonToMcu_ir_eye_camera_focus_sweep_lens_values_tag] =
        handle_ir_eye_camera_focus_sweep_lens_values,
    [JetsonToMcu_ir_eye_camera_focus_sweep_values_polynomial_tag] =
        handle_ir_eye_camera_focus_sweep_values_polynomial,
    [JetsonToMcu_perform_ir_eye_camera_focus_sweep_tag] =
        handle_perform_ir_eye_camera_focus_sweep,
    [JetsonToMcu_ir_eye_camera_mirror_sweep_values_polynomial_tag] =
        handle_ir_eye_camera_mirror_sweep_values_polynomial,
    [JetsonToMcu_perform_ir_eye_camera_mirror_sweep_tag] =
        handle_perform_ir_eye_camera_mirror_sweep,
};

static_assert(
    ARRAY_SIZE(handle_message_callbacks) <= 43,
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

        // filter out jobs from UART for debugging
        if (new.remote_addr != 0) {
            LOG_DBG("⬇️ Received message from remote 0x%03x with payload ID "
                    "%02d, ack #%u",
                    new.remote_addr, new.message.which_payload, new.ack_number);
        }

        // remote is up
        publish_start();

        if (new.message.which_payload < ARRAY_SIZE(handle_message_callbacks) &&
            handle_message_callbacks[new.message.which_payload] != NULL) {
            handle_message_callbacks[new.message.which_payload](&new);
        } else {
            LOG_ERR("A handler for message with a payload ID of %d is not "
                    "implemented",
                    new.message.which_payload);
            job_ack(Ack_ErrorCode_OPERATION_NOT_SUPPORTED, &new);
        }
    }
}

static K_SEM_DEFINE(new_job_sem, 1, 1);
static job_t new = {0};
static McuMessage mcu_message;

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
        bool decoded = pb_decode_ex(&stream, McuMessage_fields, &mcu_message,
                                    PB_DECODE_DELIMITED);
        if (decoded) {
            if (mcu_message.which_message != McuMessage_j_message_tag) {
                LOG_INF("Got message not intended for us. Dropping.");
                err_code = RET_ERROR_INVALID_ADDR;
            } else {
                new.remote = CAN_MESSAGING;
                new.message = mcu_message.message.j_message;
                new.ack_number = mcu_message.message.j_message.ack_number;

                if (can_msg->destination & CAN_ADDR_IS_ISOTP) {
                    // keep flags of the received message destination
                    // & invert source and destination
                    new.remote_addr = (can_msg->destination & ~0xFF);
                    new.remote_addr |= (can_msg->destination & 0xF) << 4;
                    new.remote_addr |= (can_msg->destination & 0xF0) >> 4;
                } else {
                    new.remote_addr = CONFIG_CAN_ADDRESS_DEFAULT_REMOTE;
                }

                ret = k_msgq_put(&process_queue, &new, K_MSEC(5));
                if (ret) {
                    ASSERT_SOFT(ret);
                    err_code = RET_ERROR_BUSY;
                }
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

#ifdef CONFIG_MCU_UTIL_UART_TESTS
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

        bool decoded = pb_decode_ex(&stream, McuMessage_fields, &mcu_message,
                                    PB_DECODE_DELIMITED);
        if (decoded) {
            if (mcu_message.which_message != McuMessage_j_message_tag) {
                LOG_INF("Got message not intended for us. Dropping.");
                err_code = RET_ERROR_INVALID_ADDR;
            } else {
                new.remote = UART_MESSAGING;
                new.message = mcu_message.message.j_message;
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
    runner_tid = k_thread_create(&runner_process, runner_process_stack,
                                 K_THREAD_STACK_SIZEOF(runner_process_stack),
                                 runner_process_jobs_thread, NULL, NULL, NULL,
                                 THREAD_PRIORITY_RUNNER, 0, K_NO_WAIT);
}
