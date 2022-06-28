#include "can_messaging.h"
#include <app_assert.h>
#include <assert.h>
#include <canbus/isotp.h>
#include <drivers/can.h>
#include <logging/log.h>
#include <pb_encode.h>
#include <sys/__assert.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(isotp_tx);

static const struct device *can_dev;

static void
process_tx_messages_thread();
K_THREAD_DEFINE(process_isotp_tx_messages,
                CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_TX,
                process_tx_messages_thread, NULL, NULL, NULL,
                CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_TX, 0, 0);

#define QUEUE_ALIGN 8
static_assert(QUEUE_ALIGN % 2 == 0, "QUEUE_ALIGN must be a multiple of 2");
static_assert(sizeof(McuMessage) % QUEUE_ALIGN == 0,
              "sizeof McuMessage must be a multiple of QUEUE_ALIGN");

// Message queue to send messages
K_MSGQ_DEFINE(isotp_tx_msg_queue, sizeof(McuMessage),
              CONFIG_ORB_LIB_CANBUS_TX_QUEUE_SIZE, QUEUE_ALIGN);

static K_SEM_DEFINE(tx_sem, 1, 1);

// Buffer to store the message to be sent
static char tx_buffer[McuMessage_size + 1];
static bool is_init = false;

// CAN ISO-TP addressing
const struct isotp_msg_id mcu_to_jetson_dst_addr = {
    .std_id = CAN_ISOTP_STDID_DESTINATION(CONFIG_CAN_ISOTP_LOCAL_ID,
                                          CONFIG_CAN_ISOTP_REMOTE_ID),
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0};
const struct isotp_msg_id mcu_to_jetson_src_addr = {
    .std_id = CAN_ISOTP_STDID_SOURCE(CONFIG_CAN_ISOTP_LOCAL_ID,
                                     CONFIG_CAN_ISOTP_REMOTE_ID),
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0};

static void
tx_complete_cb(int error_nr, void *arg)
{
    ARG_UNUSED(error_nr);
    ARG_UNUSED(arg);

    // don't care about the error: failing tx are discarded

    // notify thread data TX is available
    k_sem_give(&tx_sem);
}

/// @brief Send a message to Jetson
/// @param[in] data The message to send
/// @param[in] len Length of \c data
/// @param[in] tx_complete_cb Callback to call when the message has been sent
/// @return ret_code_t
static ret_code_t
send(const char *data, size_t len, void (*tx_complete_cb)(int, void *))
{
    ASSERT_HARD_BOOL(len < McuMessage_size + 1);

    static struct isotp_send_ctx send_ctx = {0};

    int ret = isotp_send(&send_ctx, can_dev, data, len, &mcu_to_jetson_dst_addr,
                         &mcu_to_jetson_src_addr, tx_complete_cb, NULL);
    if (ret != ISOTP_N_OK) {
        LOG_ERR("Error while sending data to 0x%x: %d",
                mcu_to_jetson_src_addr.std_id, ret);

        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

_Noreturn static void
process_tx_messages_thread()
{
    McuMessage new;

    while (1) {
        // wait for semaphore to be released when TX done
        k_sem_take(&tx_sem, K_FOREVER);

        // wait for new message to be queued
        int ret = k_msgq_get(&isotp_tx_msg_queue, &new, K_FOREVER);
        if (ret != 0) {
            // error
            continue;
        }

        // encode protobuf format
        pb_ostream_t stream =
            pb_ostream_from_buffer(tx_buffer, sizeof(tx_buffer));
        bool encoded =
            pb_encode_ex(&stream, McuMessage_fields, &new, PB_ENCODE_DELIMITED);
        if (encoded) {
            ret_code_t err_code =
                send(tx_buffer, stream.bytes_written, tx_complete_cb);
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
        } else {
#ifndef CONFIG_ORB_LIB_LOG_BACKEND_CAN // prevent recursive call
            LOG_ERR("Error encoding message!");
#else
            printk("<err> Error encoding message!\r\n");
#endif
        }
    }
}

// ⚠️ Do not print log message in this function if
// CONFIG_ORB_LIB_LOG_BACKEND_CAN is defined
ret_code_t
can_isotp_messaging_async_tx(McuMessage *message)
{
    if (!is_init) {
        return RET_ERROR_INVALID_STATE;
    }

    // make sure data "header" is correctly set
    message->version = Version_VERSION_0;

    int ret = k_msgq_put(&isotp_tx_msg_queue, message, K_NO_WAIT);
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

ret_code_t
canbus_isotp_tx_init(void)
{
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    // this function might be called while threads are running
    // so purge before resetting the semaphore to make sure tx thread
    // blocks on the empty queue once the semaphore is freed
    k_msgq_purge(&isotp_tx_msg_queue);
    k_sem_init(&tx_sem, 1, 1);

    is_init = true;

    return RET_SUCCESS;
}
