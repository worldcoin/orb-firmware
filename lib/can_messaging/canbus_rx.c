#include "can_messaging.h"
#include <app_assert.h>
#include <pb_decode.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(can_rx, CONFIG_CAN_RX_LOG_LEVEL);

K_THREAD_STACK_DEFINE(can_rx_thread_stack,
                      CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_RX);
static struct k_thread rx_thread_data = {0};

static const struct device *can_dev =
    DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_canbus));
static struct can_frame rx_frame;
static const struct can_filter recv_queue_filter = {
    .id = CONFIG_CAN_ADDRESS_MCU,
    .mask = CAN_EXT_ID_MASK,
    .flags = CAN_FILTER_IDE | CAN_FILTER_FDF | CAN_FILTER_DATA};
CAN_MSGQ_DEFINE(can_recv_queue, 5);

static void (*incoming_message_handler)(void *msg);

static void
rx_thread()
{
    int ret;
    static can_message_t rx_message = {0};

    ASSERT_HARD_BOOL(can_dev != NULL);

    ret = can_add_rx_filter_msgq(can_dev, &can_recv_queue, &recv_queue_filter);
    if (ret < 0) {
        LOG_ERR("Error attaching message queue (%d)!", ret);
        return;
    }

    while (1) {
        k_msgq_get(&can_recv_queue, &rx_frame, K_FOREVER);
        rx_message.size = can_dlc_to_bytes(rx_frame.dlc);
        rx_message.destination = rx_frame.id;
        rx_message.bytes = rx_frame.data;

        if (incoming_message_handler != NULL) {
            incoming_message_handler((void *)&rx_message);
        } else {
            LOG_ERR("No message handler installed!");
        }
    }
}

ret_code_t
canbus_rx_init(void (*in_handler)(void *msg))
{
    if (in_handler == NULL) {
        return RET_ERROR_INVALID_PARAM;
    } else {
        incoming_message_handler = in_handler;
    }

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
