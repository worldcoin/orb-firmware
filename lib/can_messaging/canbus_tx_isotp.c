#include "can_messaging.h"
#include <app_assert.h>
#include <assert.h>
#include <pb_encode.h>
#include <zephyr/canbus/isotp.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(isotp_tx, CONFIG_ISOTP_TX_LOG_LEVEL);

static const struct device *can_dev =
    DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_canbus));

static void
process_tx_messages_thread();
static K_THREAD_STACK_DEFINE(can_tx_isotp_stack_area,
                             CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_TX);
static struct k_thread can_tx_isotp_thread_data;

#define QUEUE_ALIGN 4
BUILD_ASSERT(QUEUE_ALIGN % 2 == 0, "QUEUE_ALIGN must be a multiple of 2");
BUILD_ASSERT(sizeof(can_message_t) % QUEUE_ALIGN == 0,
             "sizeof can_message_t must be a multiple of QUEUE_ALIGN");

// Message queue to send messages
K_MSGQ_DEFINE(isotp_tx_msg_queue, sizeof(can_message_t),
              CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE, QUEUE_ALIGN);

static struct k_heap can_tx_isotp_memory_heap;
static char __aligned(4)
    can_tx_isotp_memory_heap_buffer[CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE *
                                    CONFIG_CAN_ISOTP_MAX_SIZE_BYTES];

static K_SEM_DEFINE(tx_sem, 1, 1);

static ATOMIC_DEFINE(is_init, 1);

static void
tx_complete_cb(int error_nr, void *buffer_to_free)
{
    ARG_UNUSED(error_nr);

    // free heap allocated buffer
    k_heap_free(&can_tx_isotp_memory_heap, buffer_to_free);

    // don't care about the error: failing tx are discarded

    // notify thread data TX is available
    k_sem_give(&tx_sem);
}

_Noreturn static void
process_tx_messages_thread()
{
    ASSERT_SOFT_BOOL(can_dev != NULL);

    can_message_t new;
    struct isotp_send_ctx send_ctx = {0};

    // CAN ISO-TP addressing
    struct isotp_msg_id mcu_to_jetson_dst_addr = {
        .std_id = 0, .ide = 0, .use_ext_addr = 0};
    struct isotp_msg_id mcu_to_jetson_src_addr = {
        .std_id = 0, .ide = 0, .use_ext_addr = 0};
    int ret;

    while (1) {
        // set `is_init` flag if not
        // flag can be reset in function `canbus_isotp_tx_init`
        atomic_set(is_init, 1);

        // wait for semaphore to be released when TX done
        // if tx not done within 5s, consider it as failure
        // and wait for next tx message in the next loop
        ret = k_sem_take(&tx_sem, K_MSEC(5000));
        if (ret != 0) {
            k_sem_give(&tx_sem);
            continue;
        }

        // wait for new message to be queued
        // if queue is purged during init, k_msgq_get will return with error
        // (-ENOMSG = -35), so we use the `is_init` flag to make sure we don't
        // log foreseen error
        ret = k_msgq_get(&isotp_tx_msg_queue, &new, K_FOREVER);
        if (ret != 0 && atomic_get(is_init) != 0) {
            LOG_ERR("msg queue error: %i", ret);
            k_sem_give(&tx_sem);
            continue;
        } else if (atomic_get(is_init) == 0) {
            // queue as been purged, loop back without going further
            k_sem_give(&tx_sem);
            continue;
        }

        // set addresses and send
        mcu_to_jetson_dst_addr.std_id = new.destination;
        mcu_to_jetson_src_addr.std_id = new.destination & ~CAN_ADDR_IS_DEST;
        ret = isotp_send(&send_ctx, can_dev, new.bytes, new.size,
                         &mcu_to_jetson_dst_addr, &mcu_to_jetson_src_addr,
                         tx_complete_cb, new.bytes);

        if (ret != ISOTP_N_OK) {
#ifndef CONFIG_ORB_LIB_LOG_BACKEND_CAN // prevent recursive call
            LOG_WRN("Error sending message");
#else
            printk("<wrn> Error sending ISO-TP message!\r\n");
#endif
            // free heap allocated buffer
            k_heap_free(&can_tx_isotp_memory_heap, new.bytes);

            // release semaphore, we are not waiting for
            // completion
            k_sem_give(&tx_sem);
        }
    }
}

// ⚠️ Do not print log message in this function if
// CONFIG_ORB_LIB_LOG_BACKEND_CAN is defined
ret_code_t
can_isotp_messaging_async_tx(const can_message_t *message)
{
    if (atomic_get(is_init) == 0) {
        return RET_ERROR_INVALID_STATE;
    }

    can_message_t to_send = *message;
    to_send.bytes =
        k_heap_alloc(&can_tx_isotp_memory_heap, message->size, K_NO_WAIT);
    if (to_send.bytes != NULL) {
        memcpy(to_send.bytes, message->bytes, message->size);

        int ret = k_msgq_put(&isotp_tx_msg_queue, &to_send, K_NO_WAIT);
        if (ret) {
            // free heap allocated buffer
            k_heap_free(&can_tx_isotp_memory_heap, to_send.bytes);

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

ret_code_t
canbus_isotp_tx_init(void)
{
    // keep a pointer to the thread data as a flag to know if the thread is
    // initialized
    static k_tid_t tid = NULL;

    atomic_clear(is_init);

    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    if (tid == NULL) {
        tid = k_thread_create(
            &can_tx_isotp_thread_data, can_tx_isotp_stack_area,
            K_THREAD_STACK_SIZEOF(can_tx_isotp_stack_area),
            process_tx_messages_thread, NULL, NULL, NULL,
            CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_TX, 0, K_NO_WAIT);
        k_thread_name_set(&can_tx_isotp_thread_data, "can_tx_isotp");
    }

    // this function might be called while threads are running
    // make sure the thread is waiting for a new message
    // while the queue is purged
    k_sem_give(&tx_sem);
    k_msgq_purge(&isotp_tx_msg_queue);
    k_heap_init(&can_tx_isotp_memory_heap, can_tx_isotp_memory_heap_buffer,
                sizeof(can_tx_isotp_memory_heap_buffer));

    return RET_SUCCESS;
}
