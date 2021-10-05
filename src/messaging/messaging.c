//
// Created by Cyril on 05/10/2021.
//

#include "messaging.h"

#include "canbus.h"
#include <sys/__assert.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(messaging);

#define THREAD_MESSAGE_PROCESS_STACKSIZE      1024
#define THREAD_MESSAGE_PROCESS_PRIORITY       7
static void process_messages_thread();
K_THREAD_DEFINE(process_messages,
                THREAD_MESSAGE_PROCESS_STACKSIZE, process_messages_thread, NULL, NULL, NULL,
                THREAD_MESSAGE_PROCESS_PRIORITY, 0, 0);

// Message queue to pass incoming messages from other components
K_MSGQ_DEFINE(my_msgq, sizeof(McuMessage), 4, 4);

void
messaging_push(McuMessage *message)
{
    int ret = k_msgq_put(&my_msgq, message, K_NO_WAIT);

    if (ret) {
        LOG_ERR("Too many messages");
    }
}

_Noreturn static void
process_messages_thread()
{
    McuMessage new;

    while (1) {
        // wait for new message
        int ret = k_msgq_get(&my_msgq, &new, K_FOREVER);
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
            LOG_INF("IR led command wavelength: %u, on_duration: %u",
                    new.message.j_message.payload.ir_leds.wavelength,
                    new.message.j_message.payload.ir_leds.on_duration);
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