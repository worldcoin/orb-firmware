#include "can_messaging.h"
#include <device.h>
#include <drivers/can.h>
#include <logging/log.h>
#include <pb_decode.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(can_rx);

K_THREAD_STACK_DEFINE(can_rx_thread_stack,
                      CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_RX);
static struct k_thread rx_thread_data = {0};

static const struct device *can_dev;
static struct zcan_frame rx_frame;
static can_message_t rx_message = {0};
static const struct zcan_filter recv_queue_filter = {
    .id_type = CAN_EXTENDED_IDENTIFIER,
    .rtr = CAN_DATAFRAME,
    .id = CONFIG_CAN_ADDRESS_MCU,
    .rtr_mask = 1,
    .id_mask = CAN_EXT_ID_MASK};
CAN_MSGQ_DEFINE(can_recv_queue, 5);

static void (*incoming_message_handler)(can_message_t *msg);

static void
rx_thread()
{
    int ret;

    ret = can_add_rx_filter_msgq(can_dev, &can_recv_queue, &recv_queue_filter);
    if (ret < 0) {
        LOG_ERR("Error attaching message queue (%d)!", ret);
        return;
    }

    while (1) {
        k_msgq_get(&can_recv_queue, &rx_frame, K_FOREVER);
        rx_message.size = can_dlc_to_bytes(rx_frame.dlc);
        rx_message.destination = rx_frame.id;
        memcpy(&rx_message.bytes, rx_frame.data, rx_message.size);

        if (incoming_message_handler != NULL) {
            incoming_message_handler(&rx_message);
        } else {
            LOG_ERR("Cannot handle message");
        }
    }
}

ret_code_t
canbus_rx_init(void (*in_handler)(can_message_t *msg))
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

    k_tid_t tid = k_thread_create(
        &rx_thread_data, can_rx_thread_stack,
        K_THREAD_STACK_SIZEOF(can_rx_thread_stack), rx_thread, NULL, NULL, NULL,
        CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_RX, 0, K_NO_WAIT);
    k_thread_name_set(tid, "can_rx");

    return RET_SUCCESS;
}
