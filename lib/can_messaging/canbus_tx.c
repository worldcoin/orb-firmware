#include "can_messaging.h"
#include <app_assert.h>
#include <assert.h>
#include <pb_encode.h>
#include <utils.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(can_tx, CONFIG_CAN_TX_LOG_LEVEL);

static const struct device *can_dev =
    DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_canbus));

static void
process_tx_messages_thread();
static K_THREAD_STACK_DEFINE(can_tx_stack_area,
                             CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_TX);
static struct k_thread can_tx_thread_data;

#define QUEUE_ALIGN 4
BUILD_ASSERT(QUEUE_ALIGN % 2 == 0, "QUEUE_ALIGN must be a multiple of 2");
BUILD_ASSERT(sizeof(can_message_t) % QUEUE_ALIGN == 0,
             "sizeof can_message_t must be a multiple of QUEUE_ALIGN");

// Message queue to send messages
K_MSGQ_DEFINE(can_tx_msg_queue, sizeof(can_message_t),
              CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE, QUEUE_ALIGN);
static struct k_mem_slab can_tx_memory_slab;
#define SLAB_BUFFER_ALIGNMENT 4
static char __aligned(SLAB_BUFFER_ALIGNMENT)
    can_tx_memory_slab_buffer[CAN_FRAME_MAX_SIZE *
                              CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE];
BUILD_ASSERT(CAN_FRAME_MAX_SIZE % SLAB_BUFFER_ALIGNMENT == 0 &&
                 CAN_FRAME_MAX_SIZE > SLAB_BUFFER_ALIGNMENT,
             "Each block must be at least SLAB_BUFFER_ALIGNMENT*N bytes long "
             "and aligned on this boundary");
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

static int
send(const char *data, size_t len,
     void (*tx_complete_cb)(const struct device *, int, void *), uint32_t dest)
{
    ASSERT_HARD_BOOL(len <= CAN_FRAME_MAX_SIZE);

    struct can_frame frame = {
        .flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS, .id = dest};

    frame.dlc = can_bytes_to_dlc(len);
    memset(frame.data, 0, sizeof frame.data);
    memcpy(frame.data, data, len);

    int ret = can_send(can_dev, &frame, K_MSEC(1000), tx_complete_cb, NULL);
    if (ret == -ENETDOWN) {
        // CAN bus in off state
        if (can_dev != NULL && can_recover(can_dev, K_MSEC(2000)) != 0) {
            ASSERT_HARD(RET_ERROR_OFFLINE);
        }
    }

    return ret;
}

_Noreturn static void
process_tx_messages_thread()
{
    can_message_t new;
    int ret;

    while (1) {
        // wait for semaphore to be released when TX done
        // if tx not done within 5s, consider it as failure
        // and wait for next tx message in the next loop
        ret = k_sem_take(&tx_sem, K_MSEC(5000));
        if (ret != 0) {
            LOG_ERR("tx isotp semaphore error: %i", ret);
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

        int err_code =
            send(new.bytes, new.size, tx_complete_cb, new.destination);

        k_mem_slab_free(&can_tx_memory_slab, (void **)&new.bytes);

        if (err_code != RET_SUCCESS) {
#ifndef CONFIG_ORB_LIB_LOG_BACKEND_CAN // prevent recursive call
            LOG_WRN("Error sending message");
#elifndef CONFIG_NO_JETSON_BOOT
            printk("<wrn> Error sending raw CAN message, err %i!\r\n",
                   err_code);
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
    ASSERT_HARD_BOOL(can_dev != NULL);

    if (!is_init) {
        return RET_ERROR_INVALID_STATE;
    }

    struct can_bus_err_cnt can_err;
    enum can_state can_state;
    can_get_state(can_dev, &can_state, &can_err);
    if (can_state == CAN_STATE_BUS_OFF || can_state == CAN_STATE_STOPPED) {
        return RET_ERROR_INVALID_STATE;
    }

    if (message->size > CAN_FRAME_MAX_SIZE) {
        return RET_ERROR_INVALID_PARAM;
    }

    can_message_t to_send = *message;
    LOG_DBG("Num slabs used: %" PRIu32,
            k_mem_slab_num_used_get(&can_tx_memory_slab));
    if (k_mem_slab_alloc(&can_tx_memory_slab, (void **)&to_send.bytes,
                         K_NO_WAIT) == 0) {
        memcpy(to_send.bytes, message->bytes, message->size);

        int ret = k_msgq_put(&can_tx_msg_queue, &to_send, K_NO_WAIT);
        if (ret) {
            k_mem_slab_free(&can_tx_memory_slab, (void **)&to_send.bytes);

#ifndef CONFIG_ORB_LIB_LOG_BACKEND_CAN // prevent recursive call
            LOG_ERR("Too many tx messages");
#else
            printk("<err> too many tx messages\r\n");
#endif
            return RET_ERROR_BUSY;
        }
    } else {
        return RET_ERROR_NO_MEM;
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
        return RET_ERROR_FORBIDDEN;
    }

    struct can_bus_err_cnt can_err;
    enum can_state can_state;
    can_get_state(can_dev, &can_state, &can_err);

    if (can_state == CAN_STATE_BUS_OFF || can_state == CAN_STATE_STOPPED) {
        return RET_ERROR_INVALID_STATE;
    }

    return send(message->bytes, message->size, NULL,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

ret_code_t
canbus_tx_init(void)
{
    // keep a pointer to the thread data as a flag to know if the thread is
    // initialized
    static k_tid_t tid = NULL;

    int ret;

    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    if (tid == NULL) {
        tid = k_thread_create(&can_tx_thread_data, can_tx_stack_area,
                              K_THREAD_STACK_SIZEOF(can_tx_stack_area),
                              process_tx_messages_thread, NULL, NULL, NULL,
                              CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_TX, 0,
                              K_NO_WAIT);
        k_thread_name_set(&can_tx_thread_data, "can_tx");
    }

    // this function might be called while threads are running,
    // and we don't want to have other, higher priority, threads
    // woken up while we are reinitializing the semaphore, queue & mem_slab,
    // so we create a critical section to make these operations atomic
    CRITICAL_SECTION_ENTER(k);
    k_msgq_purge(&can_tx_msg_queue);
    k_sem_give(&tx_sem);
    ret = k_mem_slab_init(&can_tx_memory_slab, can_tx_memory_slab_buffer,
                          CAN_FRAME_MAX_SIZE,
                          CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE);
    is_init = true;
    CRITICAL_SECTION_EXIT(k);

    ASSERT_SOFT(ret);

    return RET_SUCCESS;
}
