#include <device.h>
#include <drivers/gpio.h>
#include "power_sequence/power_sequence.h"
#include "fan/fan.h"
#include "sound/sound.h"
#include "messaging/messaging.h"

#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(main);

#if CONFIG_BOARD_STM32G484_EVAL
#include "messaging/tests.h"
#endif


void main(void)
{
    LOG_INF("Hello from " CONFIG_BOARD " :)");

#ifdef CONFIG_BOARD_ORB
    __ASSERT(wait_for_power_button_press() == 0, "Error waiting for button");
#endif

    LOG_INF("Booting system");

    __ASSERT(turn_on_power_supplies() == 0, "Power sequencing error");
    __ASSERT(turn_on_jetson() == 0, "Jetson power-on error");
    __ASSERT(turn_on_fan() == 0, "Error turning on fan");
    __ASSERT(init_sound() == 0, "Error initializing sound");

    k_msleep(1000);

    messaging_init();

    // the target is now up and running

#ifdef CONFIG_BOARD_STM32G484_EVAL
    LOG_WRN("Running tests");

    tests_messaging_init();
#endif
}
