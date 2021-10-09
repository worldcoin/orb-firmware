#include <device.h>
#include <drivers/gpio.h>
#include "power_sequence/power_sequence.h"
#include "fan/fan.h"
#include "sound/sound.h"
#include "messaging/messaging.h"
#include "stepper_motors/stepper_motors.h"

#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(main);

#if TEST_TARGET
#include "messaging/tests.h"
#endif

void main(void)
{
    LOG_INF("Hello from Orb MCU :)");

   //  __ASSERT(wait_for_power_button_press() == 0, "Error waiting for button");

    LOG_INF("Booting system");

    __ASSERT(turn_on_power_supplies() == 0, "Power sequencing error");
    __ASSERT(turn_on_jetson() == 0, "Jetson power-on error");
    __ASSERT(turn_on_fan() == 0, "Error turning on fan");
    __ASSERT(init_sound() == 0, "Error initializing sound");
    __ASSERT(init_stepper_motors() == 0, "Error initializing sound");

    messaging_init();

    // the target is now up and running

#if TEST_TARGET
    LOG_WRN("Running test target");

    tests_messaging_init();
#endif
}
