#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#include "power_sequence/power_sequence.h"
#include "fan/fan.h"
#include "sound/sound.h"

void main(void)
{
    LOG_INF("Hello from Orb MCU :)");

    __ASSERT(wait_for_power_button_press() == 0, "Error waiting for button");

    LOG_INF("Booting system");

    __ASSERT(turn_on_power_supplies() == 0, "Power sequencing error");
    __ASSERT(turn_on_jetson() == 0, "Jetson power-on error");
    __ASSERT(turn_on_fan() == 0, "Error turning on fan");
    __ASSERT(init_sound() == 0, "Error initializing sound");
}
