#include "can_messaging.h"
#include <app_assert.h>
#include <assert.h>
#include <drivers/can.h>
#include <logging/log.h>
#include <pb_encode.h>
#include <sys/__assert.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(can_tx);

static const struct device *can_dev;

static void
process_tx_messages_thread();
K_THREAD_DEFINE(can_tx, CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_TX,
                process_tx_messages_thread, NULL, NULL, NULL,
                CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_TX, 0, 0);

#define QUEUE_ALIGN 8
static_assert(QUEUE_ALIGN % 2 == 0, "QUEUE_ALIGN must be a multiple of 2");
static_assert(sizeof(can_message_t) % QUEUE_ALIGN == 0,
              "sizeof McuMessage must be a multiple of QUEUE_ALIGN");

// Message queue to send messages
K_MSGQ_DEFINE(can_tx_msg_queue, sizeof(can_message_t),
              CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE, QUEUE_ALIGN);

static K_SEM_DEFINE(tx_sem, 1, 1);

static bool is_init = false;

static void
tx_complete_cb(const struct device *dev, int error_nr, void *arg)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(arg);
    ARG_UNUSED(error_nr);

    // don't care about the error: failing tx are discarded

    // notify thread data TX is available
    k_sem_give(&tx_sem);
}

static ret_code_t
send(const char *data, size_t len,
     void (*tx_complete_cb)(const struct device *, int, void *), uint32_t dest)
{
    ASSERT_HARD_BOOL(len < CAN_MAX_DLEN);

    struct zcan_frame frame = {.id_type = CAN_EXTENDED_IDENTIFIER,
                               .fd = true,
                               .rtr = CAN_DATAFRAME,
                               .id = dest};

    frame.dlc = can_bytes_to_dlc(len);
    memset(frame.data, 0, sizeof frame.data);
    memcpy(frame.data, data, len);

    return (can_send(can_dev, &frame, K_MSEC(1000), tx_complete_cb, NULL) == 0)
               ? RET_SUCCESS
               : RET_ERROR_INTERNAL;
}

_Noreturn static void
process_tx_messages_thread()
{
    can_message_t new;
    int ret;

    while (1) {
        // wait for semaphore to be released when TX done
        // if tx not done within 5s, consider it as failure
        ret = k_sem_take(&tx_sem, K_MSEC(5000));
        if (ret != 0) {
            k_sem_give(&tx_sem);
            continue;
        }

        // wait for new message to be queued
        // here we can wait forever
        ret = k_msgq_get(&can_tx_msg_queue, &new, K_FOREVER);
        if (ret != 0) {
            k_sem_give(&tx_sem);
            continue;
        }

        ret_code_t err_code =
            send(new.bytes, new.size, tx_complete_cb, new.destination);
        if (err_code != RET_SUCCESS) {
#ifndef CONFIG_ORB_LIB_LOG_BACKEND_CAN // prevent recursive call
            LOG_WRN("Error sending message");
#else
            printk("<wrn> Error sending message!\r\n");
#endif
            // release semaphore, we are not waiting for
            // completion
            k_sem_give(&tx_sem);
        }
    }
}

// ⚠️ Do not print log message in this function if
// CONFIG_ORB_LIB_LOG_BACKEND_CAN is defined
ret_code_t
can_messaging_async_tx(const can_message_t *message)
{
    if (!is_init) {
        return RET_ERROR_INVALID_STATE;
    }

    if (message->size > CAN_MAX_DLEN) {
        return RET_ERROR_INVALID_PARAM;
    }

    int ret = k_msgq_put(&can_tx_msg_queue, message, K_NO_WAIT);
    if (ret) {
#ifndef CONFIG_ORB_LIB_LOG_BACKEND_CAN // prevent recursive call
        LOG_ERR("Too many tx messages");
#else
        printk("<err> too many tx messages\r\n");
#endif
        return RET_ERROR_BUSY;
    }

    return RET_SUCCESS;
}

// ⚠️ Cannot be used in ISR context
// ⚠️ Do not print log message in this function if
// CONFIG_ORB_LIB_LOG_BACKEND_CAN is defined
ret_code_t
can_messaging_blocking_tx(const can_message_t *message)
{
    if (k_is_in_isr()) {
        return RET_ERROR_INVALID_STATE;
    }

    return send(message->bytes, message->size, NULL,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

ret_code_t
canbus_tx_init(void)
{
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    // this function might be called while threads are running
    // so purge before resetting the semaphore to make sure tx thread
    // blocks on the empty queue once the semaphore is freed
    k_msgq_purge(&can_tx_msg_queue);
    k_sem_init(&tx_sem, 1, 1);
    is_init = true;

    return RET_SUCCESS;
}
