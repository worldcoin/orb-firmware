//
// Created by Cyril on 05/10/2021.
//

#include "tests.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(cantest);

#include <mcu_messaging.pb.h>
#include <zephyr.h>
#include <errors.h>
#include <pb_encode.h>
#include "canbus.h"

K_THREAD_STACK_DEFINE(test_thread_stack, 10214);
static struct k_thread test_thread_data;

_Noreturn void
test_can_send()
{
    char buffer[512];
    ret_code_t err_code;
    int packet = 0;

    // pretend to send Jetson messages
    McuMessage data_to_serialize = McuMessage_init_zero;
    data_to_serialize.version = Version_VERSION_0;
    data_to_serialize.which_message = McuMessage_j_message_tag;

    while(1)
    {
        LOG_INF("Sending new message");

        k_msleep(2000);

        memset(buffer, 0, sizeof(buffer));
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        data_to_serialize.message.j_message.which_payload = JetsonToMcu_ir_leds_tag;
        data_to_serialize.message.j_message.payload.ir_leds.on_duration = packet;
        data_to_serialize.message.j_message.payload.ir_leds.wavelength = InfraredLEDs_Wavelength_WAVELENGTH_850NM;

        bool encoded = pb_encode(&stream, McuMessage_fields, &data_to_serialize);
        if (encoded)
        {
            err_code = canbus_send(buffer, stream.bytes_written);
            LOG_INF("Sent data #%d [%u]", packet, err_code);
        }

        packet++;
    }
}

void
tests_init(void)
{
    k_tid_t tid = k_thread_create(&test_thread_data, test_thread_stack,
                                  K_THREAD_STACK_SIZEOF(test_thread_stack),
                                  test_can_send, NULL, NULL, NULL,
                                  7, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning test_thread thread");
    }
}

