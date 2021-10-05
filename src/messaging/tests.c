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
#include "messaging.h"

K_THREAD_STACK_DEFINE(test_thread_stack, 10214);
static struct k_thread test_thread_data;

/// This function allows the test of the full CAN bus data pipe using two boards
/// Below, we test the TX thread while a remote Orb will receive data in its RX thread
/// \return never
_Noreturn void
test_can_send()
{
    int packet = 0;

    // pretend to send Jetson messages
    McuMessage data_to_serialize = McuMessage_init_zero;
    data_to_serialize.version = Version_VERSION_0;
    data_to_serialize.which_message = McuMessage_j_message_tag;

    while(1)
    {
        LOG_INF("Sending new message");

        k_msleep(2000);

        data_to_serialize.message.j_message.which_payload = JetsonToMcu_ir_leds_tag;
        data_to_serialize.message.j_message.payload.ir_leds.on_duration = packet;
        data_to_serialize.message.j_message.payload.ir_leds.wavelength = InfraredLEDs_Wavelength_WAVELENGTH_850NM;

        // queue new tx message to test the full TX thread
        messaging_push_tx(&data_to_serialize);

        packet++;
    }
}

void
tests_messaging_init(void)
{
    k_tid_t tid = k_thread_create(&test_thread_data, test_thread_stack,
                                  K_THREAD_STACK_SIZEOF(test_thread_stack),
                                  test_can_send, NULL, NULL, NULL,
                                  7, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning test_thread thread");
    }
}

