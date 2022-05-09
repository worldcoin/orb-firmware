#include "can_messaging.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(can_rx);
#include <device.h>
#include <drivers/can.h>
#include <pb_decode.h>
#include <zephyr.h>

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

K_THREAD_STACK_DEFINE(rx_thread_stack,
                      CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_RX);
static struct k_thread rx_thread_data = {0};

static const struct device *can_dev;
static struct zcan_frame rx_frame;
static const struct zcan_filter recv_queue_filter = {
    .id_type = CAN_EXTENDED_IDENTIFIER,
    .rtr = CAN_DATAFRAME,
    .id = CONFIG_CAN_ADDRESS_MCU,
    .rtr_mask = 1,
    .id_mask = CAN_EXT_ID_MASK};
CAN_MSGQ_DEFINE(recv_queue, 5);

static void (*incoming_message_handler)(McuMessage *msg);

static void
rx_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int ret;

    ret = can_add_rx_filter_msgq(can_dev, &recv_queue, &recv_queue_filter);
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
            if (incoming_message_handler != NULL) {
                incoming_message_handler(&data);
            } else {
                LOG_ERR("Cannot handle message");
            }
        } else {
            LOG_ERR("Error parsing data, discarding");
        }
    }
}

ret_code_t
canbus_rx_init(void (*in_handler)(McuMessage *msg))
{
    if (in_handler == NULL) {
        return RET_ERROR_INVALID_PARAM;
    } else {
        incoming_message_handler = in_handler;
    }

    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN not ready");
        return RET_ERROR_BUSY;
    } else {
        LOG_INF("CAN ready");
    }

    k_thread_create(&rx_thread_data, rx_thread_stack,
                    K_THREAD_STACK_SIZEOF(rx_thread_stack), rx_thread, NULL,
                    NULL, NULL, CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_RX, 0,
                    K_NO_WAIT);

    return RET_SUCCESS;
}
