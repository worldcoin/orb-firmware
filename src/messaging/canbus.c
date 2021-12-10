//
// Created by Cyril on 30/09/2021.
//

#include "canbus.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(canbus);

#include "mcu_messaging.pb.h"
#include "messaging.h"
#include <assert.h>
#include <device.h>
#include <drivers/can.h>
#include <ir_camera_system/ir_camera_system.h>
#include <pb.h>
#include <pb_decode.h>
#include <stepper_motors/stepper_motors.h>
#include <temperature/temperature.h>
#include <zephyr.h>

#include <app_config.h>

#ifndef McuMessage_size
// Nanopb allows us to specify sizes in order to know the maximum
// size of an McuMessage at compile time
// see comments in mcu_messaging.pb.h to fix that error
#error                                                                         \
    "Please define a maximum size to any field that can have a dynamic size, see NanoPb option file"
#endif

// we add a byte indicating message size to the front of an encoded message
#if McuMessage_size > (CAN_MAX_DLEN - 1)
#error McuMessage_size must be <= (CAN_MAX_DLEN - 1)
#endif

#define RX_BUF_SIZE McuMessage_size

const struct device *can_dev;

#define THREAD_STACK_SIZE_CAN_RX 2048

K_THREAD_STACK_DEFINE(rx_thread_stack, THREAD_STACK_SIZE_CAN_RX);
static struct k_thread rx_thread_data = {0};

static Ack_ErrorCode
handle_infrared_leds_message(InfraredLEDs leds)
{
    LOG_DBG("Got LED wavelength message = %d", leds.wavelength);
    ir_camera_system_enable_leds(leds.wavelength);
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_led_on_time_message(LedOnTimeUs on_time)
{
    LOG_DBG("Got LED on time message = %u", on_time.on_duration_us);
    return ir_camera_system_set_on_time_us(on_time.on_duration_us);
}

static Ack_ErrorCode
handle_start_triggering_ir_eye_camera_message(void)
{
    LOG_DBG("");
    ir_camera_system_enable_ir_eye_camera();
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_stop_triggering_ir_eye_camera_message(void)
{
    LOG_DBG("");
    ir_camera_system_disable_ir_eye_camera();
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_start_triggering_ir_face_camera_message(void)
{
    LOG_DBG("");
    ir_camera_system_enable_ir_face_camera();
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_stop_triggering_ir_face_camera_message(void)
{
    LOG_DBG("");
    ir_camera_system_disable_ir_face_camera();
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_start_triggering_2dtof_camera_message(void)
{
    LOG_DBG("");
    ir_camera_system_enable_2d_tof_camera();
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_stop_triggering_2dtof_camera_message(void)
{
    LOG_DBG("");
    ir_camera_system_disable_2d_tof_camera();
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_740nm_brightness_message(Brightness740Nm brightness)
{
    LOG_DBG("");
    return ir_camera_system_set_740nm_led_brightness(brightness.brightness);
}

static Ack_ErrorCode
handle_mirror_angle_message(MirrorAngle mirror_angle)
{
    LOG_DBG("");
    if (motors_angle_horizontal(mirror_angle.horizontal_angle) != RET_SUCCESS) {
        return Ack_ErrorCode_FAIL;
    }
    if (motors_angle_vertical(mirror_angle.vertical_angle) != RET_SUCCESS) {
        return Ack_ErrorCode_FAIL;
    }
    return Ack_ErrorCode_SUCCESS;
}

static Ack_ErrorCode
handle_temperature_sample_period_message(TemperatureSamplePeriod sample_period)
{
    LOG_DBG("Got new temperature sampling period: %ums",
            sample_period.sample_period_ms);
    temperature_set_sampling_period_ms(sample_period.sample_period_ms);
    return Ack_ErrorCode_SUCCESS;
}

static void
handle_message(McuMessage *m)
{
    McuMessage ack = {.which_message = McuMessage_m_message_tag,
                      .message.m_message.which_payload = McuToJetson_ack_tag,
                      .message.m_message.payload.ack.ack_number =
                          m->message.j_message.ack_number,
                      .message.m_message.payload.ack.error =
                          Ack_ErrorCode_FAIL};

    if (m->which_message != McuMessage_j_message_tag) {
        LOG_INF("Got message not intended for MCU. Dropping.");
        return;
    }

    LOG_DBG("Got a message: %d", m->message.j_message.which_payload);

    switch (m->message.j_message.which_payload) {
    case JetsonToMcu_infrared_leds_tag:
        ack.message.m_message.payload.ack.error = handle_infrared_leds_message(
            m->message.j_message.payload.infrared_leds);
        break;
    case JetsonToMcu_led_on_time_tag:
        ack.message.m_message.payload.ack.error = handle_led_on_time_message(
            m->message.j_message.payload.led_on_time);
        break;
    case JetsonToMcu_start_triggering_ir_eye_camera_tag:
        ack.message.m_message.payload.ack.error =
            handle_start_triggering_ir_eye_camera_message();
        break;
    case JetsonToMcu_stop_triggering_ir_eye_camera_tag:
        ack.message.m_message.payload.ack.error =
            handle_stop_triggering_ir_eye_camera_message();
        break;
    case JetsonToMcu_start_triggering_ir_face_camera_tag:
        ack.message.m_message.payload.ack.error =
            handle_start_triggering_ir_face_camera_message();
        break;
    case JetsonToMcu_stop_triggering_ir_face_camera_tag:
        ack.message.m_message.payload.ack.error =
            handle_stop_triggering_ir_face_camera_message();
        break;
    case JetsonToMcu_start_triggering_2dtof_camera_tag:
        ack.message.m_message.payload.ack.error =
            handle_start_triggering_2dtof_camera_message();
        break;
    case JetsonToMcu_stop_triggering_2dtof_camera_tag:
        ack.message.m_message.payload.ack.error =
            handle_stop_triggering_2dtof_camera_message();
        break;
    case JetsonToMcu_brightness_740nm_leds_tag:
        ack.message.m_message.payload.ack.error =
            handle_740nm_brightness_message(
                m->message.j_message.payload.brightness_740nm_leds);
        break;
    case JetsonToMcu_mirror_angle_tag:
        ack.message.m_message.payload.ack.error = handle_mirror_angle_message(
            m->message.j_message.payload.mirror_angle);
        break;
    case JetsonToMcu_temperature_sample_period_tag:
        ack.message.m_message.payload.ack.error =
            handle_temperature_sample_period_message(
                m->message.j_message.payload.temperature_sample_period);
        break;
    default:
        LOG_ERR("Unhandled message payload %d!",
                m->message.j_message.which_payload);
    }

    messaging_push_tx(&ack);
}

static const struct zcan_filter recv_queue_filter = {
    .id_type = CAN_EXTENDED_IDENTIFIER,
    .rtr = CAN_DATAFRAME,
    .id = CONFIG_CAN_ADDRESS_MCU,
    .rtr_mask = 1,
    .id_mask = CAN_EXT_ID_MASK};

static struct zcan_frame rx_frame;

CAN_DEFINE_MSGQ(recv_queue, 5);

static void
rx_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int ret;

    ret = can_attach_msgq(can_dev, &recv_queue, &recv_queue_filter);
    if (ret == CAN_NO_FREE_FILTER) {
        LOG_ERR("Error attaching message queue!");
        return;
    }

    while (1) {
        k_msgq_get(&recv_queue, &rx_frame, K_FOREVER);
        pb_istream_t stream =
            pb_istream_from_buffer(rx_frame.data, sizeof rx_frame.data);
        McuMessage data = {0};

        bool decoded = pb_decode_ex(&stream, McuMessage_fields, &data,
                                    PB_DECODE_DELIMITED);
        if (decoded) {
            handle_message(&data);
        } else {
            LOG_ERR("Error parsing data, discarding");
        }
    }
}

struct zcan_frame frame = {.id_type = CAN_EXTENDED_IDENTIFIER,
                           .fd = true,
                           .rtr = CAN_DATAFRAME,
                           .id = CONFIG_CAN_ADDRESS_JETSON};

ret_code_t
canbus_send(const char *data, size_t len,
            void (*tx_complete_cb)(uint32_t, void *))
{
    __ASSERT(CAN_MAX_DLEN >= len, "data too large!");
    frame.dlc = can_bytes_to_dlc(len);
    memset(frame.data, 0, sizeof frame.data);
    memcpy(frame.data, data, len);

    return can_send(can_dev, &frame, K_FOREVER, tx_complete_cb, NULL)
               ? RET_ERROR_INTERNAL
               : RET_SUCCESS;
}

ret_code_t
canbus_init(void)
{
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    k_tid_t tid =
        k_thread_create(&rx_thread_data, rx_thread_stack,
                        K_THREAD_STACK_SIZEOF(rx_thread_stack), rx_thread, NULL,
                        NULL, NULL, THREAD_PRIORITY_CAN_RX, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning rx thread");
        return RET_ERROR_NO_MEM;
    }

    LOG_INF("CAN bus init ok");

    return RET_SUCCESS;
}
