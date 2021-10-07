//
// Created by Cyril on 05/10/2021.
//

#include "messaging.h"

#include "canbus.h"
#include <sys/__assert.h>
#include <zephyr.h>

#include <logging/log.h>
#include <pb_encode.h>
LOG_MODULE_REGISTER(messaging);

// RX thread
#define THREAD_PROCESS_RX_MESSAGES_STACKSIZE      1024
#define THREAD_PROCESS_RX_MESSAGES_PRIORITY       7
static void process_rx_messages_thread();
K_THREAD_DEFINE(process_rx_messages,
                THREAD_PROCESS_RX_MESSAGES_STACKSIZE, process_rx_messages_thread, NULL, NULL, NULL,
                THREAD_PROCESS_RX_MESSAGES_PRIORITY, 0, 0);

// TX thread
#define THREAD_PROCESS_TX_MESSAGES_STACKSIZE      1024
#define THREAD_PROCESS_TX_MESSAGES_PRIORITY       8
static void process_tx_messages_thread();
K_THREAD_DEFINE(process_tx_messages,
                THREAD_PROCESS_TX_MESSAGES_STACKSIZE, process_tx_messages_thread, NULL, NULL, NULL,
                THREAD_PROCESS_TX_MESSAGES_PRIORITY, 0, 0);

// Message queues to pass messages to/from Jetson and Security MCU
K_MSGQ_DEFINE(rx_msg_queue, sizeof(McuMessage), 8, 4);
K_MSGQ_DEFINE(tx_msg_queue, sizeof(McuMessage), 8, 4);

K_SEM_DEFINE(tx_sem, 1, 1);

void
messaging_push_tx(McuMessage *message)
{
    // make sure data "header" is correctly set
    message->version = Version_VERSION_0;

    int ret = k_msgq_put(&tx_msg_queue, message, K_NO_WAIT);
    if (ret) {
        LOG_ERR("Too many tx messages");
    }
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

_Noreturn static void
process_tx_messages_thread()
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
        pb_ostream_t stream = pb_ostream_from_buffer(tx_buffer, sizeof(tx_buffer));
        bool encoded = pb_encode(&stream, McuMessage_fields, &new);
        if (encoded) {
            ret_code_t err_code = canbus_send(tx_buffer, stream.bytes_written, tx_complete_cb);
            if (err_code != RET_SUCCESS) {
                LOG_WRN("Error sending message");

                // release semaphore, we are not waiting for completion
                k_sem_give(&tx_sem);
            }
        }
    }
}

void
messaging_push_rx(McuMessage *message)
{
    int ret = k_msgq_put(&rx_msg_queue, message, K_NO_WAIT);

    if (ret) {
        LOG_ERR("Too many rx messages");
    }
}

_Noreturn static void
process_rx_messages_thread()
{
    McuMessage new;

    static uint32_t test_value = 0;
    static uint32_t missed = 0;

    while (1) {
        // wait for new message
        int ret = k_msgq_get(&rx_msg_queue, &new, K_FOREVER);
        if (ret != 0) {
            // error
            continue;
        }

        // handle new message
        switch (new.message.j_message.which_payload) {
        case JetsonToMcu_shutdown_tag: {
            LOG_INF("Shutdown command");
        }
            break;

        case JetsonToMcu_ir_leds_tag: {
//            LOG_INF("IR led command wavelength: %u, on_duration: %u",
//                    new.message.j_message.payload.ir_leds.wavelength,
//                    new.message.j_message.payload.ir_leds.on_duration);
            if (new.message.j_message.payload.ir_leds.on_duration != test_value + 1) {
                missed++;
                LOG_ERR("%u != %u, c %u", new.message.j_message.payload.ir_leds.on_duration, test_value, missed);
            }
            test_value = new.message.j_message.payload.ir_leds.on_duration;

        }
            break;

        case JetsonToMcu_brightness_front_leds_tag: {
            LOG_INF("Brightness: %u",
                    new.message.j_message.payload.brightness_front_leds.white_leds);
        }
            break;

        default: {
            LOG_ERR("Unhandled control data type: 0x%x",
                    new.message.j_message.which_payload);
        }

        }
    }

}

ret_code_t
messaging_init(void)
{
    ret_code_t err_code;

    // init underlying layers: CAN bus
    err_code = canbus_init();
    __ASSERT(err_code == RET_SUCCESS, "Failed init CAN bus");

    return RET_SUCCESS;
}
