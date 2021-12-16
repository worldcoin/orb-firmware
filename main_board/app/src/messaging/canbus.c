//
// Created by Cyril on 30/09/2021.
//

#include "canbus.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(canbus);

#include "incoming_message_handling.h"
#include "mcu_messaging.pb.h"
#include "messaging.h"
#include <device.h>
#include <drivers/can.h>
#include <pb_decode.h>
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
    if (ret < 0) {
        LOG_ERR("Error attaching message queue (%d)!", ret);
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
            handle_incoming_message(&data);
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
