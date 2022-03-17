#include "can_messaging.h"
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
K_THREAD_DEFINE(process_tx_messages, CONFIG_ORB_LIB_THREAD_STACK_SIZECANBUS_TX,
                process_tx_messages_thread, NULL, NULL, NULL,
                CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_TX, 0, 0);

#define QUEUE_NUM_ITEMS 8
#define QUEUE_ALIGN     8

static_assert(QUEUE_ALIGN % 2 == 0, "QUEUE_ALIGN must be a multiple of 2");
static_assert(sizeof(McuMessage) % QUEUE_ALIGN == 0,
              "sizeof McuMessage must be a multiple of QUEUE_ALIGN");

// Message queue to send messages
K_MSGQ_DEFINE(tx_msg_queue, sizeof(McuMessage), QUEUE_NUM_ITEMS, QUEUE_ALIGN);

K_SEM_DEFINE(tx_sem, 1, 1);

static bool is_init = false;

ret_code_t
can_messaging_push_tx(McuMessage *message)
{
    if (!is_init) {
        return RET_ERROR_INVALID_STATE;
    }

    // make sure data "header" is correctly set
    message->version = Version_VERSION_0;

    int ret = k_msgq_put(&tx_msg_queue, message, K_NO_WAIT);
    if (ret) {
        LOG_ERR("Too many tx messages");
        return RET_ERROR_BUSY;
    }

    return RET_SUCCESS;
}

static void
tx_complete_cb(int error_nr, void *arg)
{
    ARG_UNUSED(arg);
    ARG_UNUSED(error_nr);

    // don't care about the error: failing tx are discarded

    // notify thread data TX is available
    k_sem_give(&tx_sem);
}

static ret_code_t
send(const char *data, size_t len, void (*tx_complete_cb)(int, void *),
     uint32_t dest)
{
    __ASSERT(CAN_MAX_DLEN >= len, "data too large!");

    struct zcan_frame frame = {.id_type = CAN_EXTENDED_IDENTIFIER,
                               .fd = true,
                               .rtr = CAN_DATAFRAME,
                               .id = dest};

    frame.dlc = can_bytes_to_dlc(len);
    memset(frame.data, 0, sizeof frame.data);
    memcpy(frame.data, data, len);

    return can_send(can_dev, &frame, K_FOREVER, tx_complete_cb, NULL)
               ? RET_ERROR_INTERNAL
               : RET_SUCCESS;
}

_Noreturn static void
process_tx_messages_thread()
{
    McuMessage new;
    static char tx_buffer[McuMessage_size + 1];

    while (1) {
        // wait for semaphore to be released when TX done
        k_sem_take(&tx_sem, K_FOREVER);

        // wait for new message to be queued
        int ret = k_msgq_get(&tx_msg_queue, &new, K_FOREVER);
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
            uint32_t dest = CONFIG_CAN_ADDRESS_DEFAULT_REMOTE;

            ret_code_t err_code =
                send(tx_buffer, stream.bytes_written, tx_complete_cb, dest);
            if (err_code != RET_SUCCESS) {
                LOG_WRN("Error sending message");

                // release semaphore, we are not waiting for
                // completion
                k_sem_give(&tx_sem);
            }
        } else {
            LOG_ERR("Error encoding message!");
        }
    }
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
