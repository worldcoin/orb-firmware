#include "messaging/messaging.h"
#include <device.h>
#include <drivers/gpio.h>

#if CONFIG_BOARD_ORB
#include "distributor_leds/distributor_leds.h"
#include "fan/fan.h"
#include "front_unit_rgb_leds/front_unit_rgb_leds.h"
#include "ir_camera_system/ir_camera_system.h"
#include "messaging/messaging.h"
#include "power_sequence/power_sequence.h"
#ifdef CONFIG_TEST_IR_CAMERA_SYSTEM
#include <ir_camera_system/ir_camera_system_test.h>
#endif
#include "sound/sound.h"
#include "stepper_motors/motors_tests.h"
#include "stepper_motors/stepper_motors.h"
#endif

#if CONFIG_BOARD_STM32G484_EVAL
#include "messaging/messaging_tests.h"
#endif

#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(main);

#if CONFIG_BOARD_STM32G484_EVAL
#include "messaging/tests.h"
#endif

void
main(void)
{
    LOG_INF("Hello from " CONFIG_BOARD " :)");

#ifdef CONFIG_BOARD_ORB
    __ASSERT(power_wait_for_power_button_press() == 0,
             "Error waiting for button");
#endif

    LOG_INF("Booting system");

    __ASSERT(power_turn_on_essential_supplies() == 0, "Power sequencing error");
    __ASSERT(power_turn_on_jetson() == 0, "Jetson power-on error");
    __ASSERT(turn_on_fan() == 0, "Error turning on fan");
    __ASSERT(init_sound() == 0, "Error initializing sound");
    __ASSERT(front_unit_rgb_leds_init() == 0,
             "Error doing front unit RGB LEDs");
    __ASSERT(do_distributor_rgb_leds() == 0,
             "Error doing distributor RGB LEDs");
    __ASSERT(power_turn_on_super_cap_charger() == 0,
             "Error enabling super cap charger");
    __ASSERT(power_turn_on_pvcc() == 0, "Error turning on pvcc");
    __ASSERT(ir_camera_system_init() == 0,
             "Error initializing IR camera system");
    __ASSERT(motors_init() == 0, "Error initializing motors");

#ifdef CONFIG_TEST_IR_CAMERA_SYSTEM
    ir_camera_system_test();
#endif

#ifdef CONFIG_TEST_MOTORS
    motors_tests_init();
#endif

    messaging_init();

    // the target is now up and running

    McuMessage ack = {.which_message = McuMessage_m_message_tag,
                      .message.m_message.which_payload = McuToJetson_ack_tag,
                      .message.m_message.payload.ack.ack_number = 1,
                      .message.m_message.payload.ack.error =
                          Ack_ErrorCode_FAIL};

    messaging_push_tx(&ack);

#ifdef CONFIG_BOARD_STM32G484_EVAL
    LOG_WRN("Running tests");

    messaging_tests_init();
#endif
}
