#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include "power_sequence/power_sequence.h"
#include "fan/fan.h"
#include "messaging/messaging.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(main);

#if TEST_TARGET
#include "messaging/tests.h"
#endif

#define BUTTON_PRESS_TIME_MS 5000
#define BUTTON_SAMPLE_PERIOD_MS 10

#define POWER_BUTTON_NODE DT_PATH(buttons, power_button)
#define POWER_BUTTON_CTLR DT_GPIO_CTLR(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_PIN DT_GPIO_PIN(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_FLAGS DT_GPIO_FLAGS(POWER_BUTTON_NODE, gpios)

static int wait_for_button_press(void)
{
    const struct device *power_button = DEVICE_DT_GET(POWER_BUTTON_CTLR);

    if (!device_is_ready(power_button)) {
        LOG_ERR("power button is not ready!");
        return 1;
    }

    if (gpio_pin_configure(power_button, POWER_BUTTON_PIN, POWER_BUTTON_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring power button!");
        return 1;
    }

    LOG_INF("Waiting for button press of " TOSTR(BUTTON_PRESS_TIME_MS) "ms");
    for (size_t i = 0; i < (BUTTON_PRESS_TIME_MS / BUTTON_SAMPLE_PERIOD_MS); ++i) {
        if (!gpio_pin_get(power_button, POWER_BUTTON_PIN)) {
            if (i > 1) {
                LOG_INF("Press stopped.");
            }
            i = 0;
        }
        if (i == 1) {
            LOG_INF("Press started.");
        }
        k_msleep(BUTTON_SAMPLE_PERIOD_MS);
    }

    return 0;
}

void
main(void)
{
    LOG_INF("Hello from Orb MCU :)");

    if (wait_for_button_press()) {
        LOG_ERR("Error waiting for button. Halting application");
        return;
    }

    LOG_INF("Booting system");

    if (turn_on_essential_power_supplies()) {
        LOG_ERR("Power sequencing error. Halting application");
        return;
    }

    if (turn_on_fan()) {
        LOG_ERR("Error turning on fan. Halting application");
        return;
    }

    messaging_init();

    // the target is now up and running

#if TEST_TARGET
    LOG_WRN("Running test target");

    tests_messaging_init();
#endif
}
