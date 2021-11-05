//
// Created by Cyril on 05/10/2021.
//

#include "messaging.h"

#include "canbus.h"
#include <sys/__assert.h>
#include <zephyr.h>

#include <app_config.h>
#include <logging/log.h>
#include <pb_encode.h>
LOG_MODULE_REGISTER(messaging);

// TX thread
#define THREAD_PROCESS_TX_MESSAGES_STACKSIZE 1024
static void process_tx_messages_thread();
K_THREAD_DEFINE(process_tx_messages, THREAD_PROCESS_TX_MESSAGES_STACKSIZE,
                process_tx_messages_thread, NULL, NULL, NULL,
                THREAD_PRIORITY_PROCESS_TX_MSG, 0, 0);

// Message queues to pass messages to/from Jetson and Security MCU
K_MSGQ_DEFINE(tx_msg_queue, sizeof(McuMessage), 8, 4);

K_SEM_DEFINE(tx_sem, 1, 1);

ret_code_t messaging_push_tx(McuMessage *message)
{
    // make sure data "header" is correctly set
    message->version = Version_VERSION_0;

    int ret = k_msgq_put(&tx_msg_queue, message, K_NO_WAIT);
    if (ret) {
        LOG_ERR("Too many tx messages");
        return RET_ERROR_BUSY;
    }

    return RET_SUCCESS;
}

static void tx_complete_cb(int error_nr, void *arg)
{
    ARG_UNUSED(arg);
    ARG_UNUSED(error_nr);

    // don't care about the error: failing tx are discarded

    // notify thread data TX is available
    k_sem_give(&tx_sem);
}

_Noreturn static void process_tx_messages_thread()
{
    McuMessage new;
    char tx_buffer[256];

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
        bool encoded = pb_encode(&stream, McuMessage_fields, &new);
        if (encoded) {
            ret_code_t err_code =
                canbus_send(tx_buffer, stream.bytes_written, tx_complete_cb);
            if (err_code != RET_SUCCESS) {
                LOG_WRN("Error sending message");

                // release semaphore, we are not waiting for
                // completion
                k_sem_give(&tx_sem);
            }
        }
    }
}

ret_code_t messaging_init(void)
{
    ret_code_t err_code;

    // init underlying layers: CAN bus
    err_code = canbus_init();
    __ASSERT(err_code == RET_SUCCESS, "Failed init CAN bus");

    return RET_SUCCESS;
}
