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
K_THREAD_DEFINE(process_can_tx_messages,
                CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_TX,
                process_tx_messages_thread, NULL, NULL, NULL,
                CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_TX, 0, 0);

#define QUEUE_ALIGN 8
static_assert(QUEUE_ALIGN % 2 == 0, "QUEUE_ALIGN must be a multiple of 2");
static_assert(sizeof(McuMessage) % QUEUE_ALIGN == 0,
              "sizeof McuMessage must be a multiple of QUEUE_ALIGN");

// Message queue to send messages
K_MSGQ_DEFINE(can_tx_msg_queue, sizeof(McuMessage),
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

    return can_send(can_dev, &frame,
                    (tx_complete_cb != NULL) ? K_FOREVER : K_MSEC(1000),
                    tx_complete_cb, NULL)
               ? RET_ERROR_INTERNAL
               : RET_SUCCESS;
}

_Noreturn static void
process_tx_messages_thread()
{
    McuMessage new;
    char tx_buffer[CAN_MAX_DLEN];

    while (1) {
        // wait for semaphore to be released when TX done
        k_sem_take(&tx_sem, K_FOREVER);

        // wait for new message to be queued
        int ret = k_msgq_get(&can_tx_msg_queue, &new, K_FOREVER);
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
                send(tx_buffer, stream.bytes_written, tx_complete_cb,
                     CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
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
can_messaging_async_tx(McuMessage *message)
{
    if (!is_init) {
        return RET_ERROR_INVALID_STATE;
    }

    // make sure data "header" is correctly set
    message->version = Version_VERSION_0;

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
can_messaging_blocking_tx(McuMessage *message)
{
    char tx_buffer[CAN_MAX_DLEN];

    if (k_is_in_isr()) {
        return RET_ERROR_INVALID_STATE;
    }

    // encode protobuf format
    pb_ostream_t stream = pb_ostream_from_buffer(tx_buffer, sizeof(tx_buffer));
    bool encoded =
        pb_encode_ex(&stream, McuMessage_fields, &message, PB_ENCODE_DELIMITED);
    if (encoded) {
        ret_code_t err_code = send(tx_buffer, stream.bytes_written, NULL,
                                   CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        return err_code;
    }

    return RET_ERROR_INVALID_PARAM;
}

ret_code_t
canbus_tx_init(void)
{
    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    is_init = true;

    return RET_SUCCESS;
}
