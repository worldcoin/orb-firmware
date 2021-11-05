#include <device.h>
#include <drivers/gpio.h>
#include <drivers/regulator.h>
#include <stdio.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(power_sequence);

#include "power_sequence.h"

#define FORMAT_STRING "Checking that %s is ready... "

static int
check_is_ready(const struct device *dev, const char *name)
{
    if (!device_is_ready(dev))
    {
        LOG_ERR(FORMAT_STRING "no", name);
        return 1;
    }

    LOG_INF(FORMAT_STRING "yes", name);
    return 0;
}

#define SUPPLY_5V_PG_NODE DT_PATH(supply_5v, power_good)
#define SUPPLY_5V_PG_CTLR DT_GPIO_CTLR(SUPPLY_5V_PG_NODE, gpios)
#define SUPPLY_5V_PG_PIN DT_GPIO_PIN(SUPPLY_5V_PG_NODE, gpios)
#define SUPPLY_5V_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_5V_PG_NODE, gpios)

#define SUPPLY_3V3_PG_NODE DT_PATH(supply_3v3, power_good)
#define SUPPLY_3V3_PG_CTLR DT_GPIO_CTLR(SUPPLY_3V3_PG_NODE, gpios)
#define SUPPLY_3V3_PG_PIN DT_GPIO_PIN(SUPPLY_3V3_PG_NODE, gpios)
#define SUPPLY_3V3_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_3V3_PG_NODE, gpios)

#define SUPPLY_1V8_PG_NODE DT_PATH(supply_1v8, power_good)
#define SUPPLY_1V8_PG_CTLR DT_GPIO_CTLR(SUPPLY_1V8_PG_NODE, gpios)
#define SUPPLY_1V8_PG_PIN DT_GPIO_PIN(SUPPLY_1V8_PG_NODE, gpios)
#define SUPPLY_1V8_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_1V8_PG_NODE, gpios)

int
turn_on_power_supplies(void)
{
    const struct device *vbat_sw_regulator = DEVICE_DT_GET(DT_PATH(vbat_sw));
    const struct device *supply_12v = DEVICE_DT_GET(DT_PATH(supply_12v));
    const struct device *supply_5v = DEVICE_DT_GET(DT_PATH(supply_5v));
    const struct device *supply_5v_pg = DEVICE_DT_GET(SUPPLY_5V_PG_CTLR);
    const struct device *supply_3v8 = DEVICE_DT_GET(DT_PATH(supply_3v8));
    const struct device *supply_3v3 = DEVICE_DT_GET(DT_PATH(supply_3v3));
    const struct device *supply_3v3_pg = DEVICE_DT_GET(SUPPLY_3V3_PG_CTLR);
    const struct device *supply_1v8 = DEVICE_DT_GET(DT_PATH(supply_1v8));
    const struct device *supply_1v8_pg = DEVICE_DT_GET(SUPPLY_1V8_PG_CTLR);

    if (check_is_ready(vbat_sw_regulator, "VBAT SW") ||
        check_is_ready(supply_12v, "12V supply") ||
        check_is_ready(supply_5v, "5V supply") ||
        check_is_ready(supply_5v_pg, "5V supply power good pin") ||
        check_is_ready(supply_3v8, "3.8V supply") ||
        check_is_ready(supply_3v3, "3.3V supply") ||
        check_is_ready(supply_3v3_pg, "3.3V supply power good pin") ||
        check_is_ready(supply_1v8, "1.8V supply") ||
        check_is_ready(supply_1v8_pg, "1.8V supply power good pin"))
    {
        return 1;
    }

    regulator_enable(vbat_sw_regulator, NULL);
    LOG_INF("VBAT SW enabled");
    k_msleep(100);

    regulator_enable(supply_12v, NULL);
    LOG_INF("12V power supply enabled");
    k_msleep(100);

    if (gpio_pin_configure(supply_5v_pg, SUPPLY_5V_PG_PIN,
                           SUPPLY_5V_PG_FLAGS | GPIO_INPUT))
    {
        LOG_ERR("Error configuring 5v pg pin!");
        return 1;
    }

    regulator_enable(supply_5v, NULL);
    LOG_INF("5V power supply enabled");
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on
    // anything else
    while (!gpio_pin_get(supply_5v_pg, SUPPLY_5V_PG_PIN))
        ;
    LOG_INF("5V power supply good");

    if (gpio_pin_configure(supply_3v3_pg, SUPPLY_3V3_PG_PIN,
                           SUPPLY_3V3_PG_FLAGS | GPIO_INPUT))
    {
        LOG_ERR("Error configuring 3.3v pg pin!");
        return 1;
    }

    regulator_enable(supply_3v3, NULL);
    LOG_INF("3.3V power supply enabled");
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on the
    // fan. If we can't turn on the fan, then we don't want to turn on
    // anything else
    while (!gpio_pin_get(supply_3v3_pg, SUPPLY_3V3_PG_PIN))
        ;
    LOG_INF("3.3V power supply good");

    regulator_enable(supply_3v8, NULL);
    LOG_INF("3.8V power supply enabled");

    if (gpio_pin_configure(supply_1v8_pg, SUPPLY_1V8_PG_PIN,
                           SUPPLY_1V8_PG_FLAGS | GPIO_INPUT))
    {
        LOG_ERR("Error configuring 1.8 pg pin!");
        return 1;
    }

    regulator_enable(supply_1v8, NULL);
    LOG_INF("1.8V power supply enabled");
    LOG_INF("Waiting on power good...");
    while (!gpio_pin_get(supply_1v8_pg, SUPPLY_1V8_PG_PIN))
        ;
    LOG_INF("1.8V power supply good");
    return 0;
}

#define BUTTON_PRESS_TIME_MS 5000
#define BUTTON_SAMPLE_PERIOD_MS 10

#define POWER_BUTTON_NODE DT_PATH(buttons, power_button)
#define POWER_BUTTON_CTLR DT_GPIO_CTLR(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_PIN DT_GPIO_PIN(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_FLAGS DT_GPIO_FLAGS(POWER_BUTTON_NODE, gpios)

int
wait_for_power_button_press(void)
{
    const struct device *power_button = DEVICE_DT_GET(POWER_BUTTON_CTLR);

    if (!device_is_ready(power_button))
    {
        LOG_ERR("power button is not ready!");
        return 1;
    }

    if (gpio_pin_configure(power_button, POWER_BUTTON_PIN,
                           POWER_BUTTON_FLAGS | GPIO_INPUT))
    {
        LOG_ERR("Error configuring power button!");
        return 1;
    }

    LOG_INF("Waiting for button press of " TOSTR(BUTTON_PRESS_TIME_MS) "ms");
    for (size_t i = 0; i < (BUTTON_PRESS_TIME_MS / BUTTON_SAMPLE_PERIOD_MS);
         ++i)
    {
        if (!gpio_pin_get(power_button, POWER_BUTTON_PIN))
        {
            if (i > 1)
            {
                LOG_INF("Press stopped.");
            }
            i = 0;
        }
        if (i == 1)
        {
            LOG_INF("Press started.");
        }
        k_msleep(BUTTON_SAMPLE_PERIOD_MS);
    }

    return 0;
}

#define SLEEP_WAKE_NODE DT_PATH(jetson_power_pins, sleep_wake)
#define SLEEP_WAKE_CTLR DT_GPIO_CTLR(SLEEP_WAKE_NODE, gpios)
#define SLEEP_WAKE_PIN DT_GPIO_PIN(SLEEP_WAKE_NODE, gpios)
#define SLEEP_WAKE_FLAGS DT_GPIO_FLAGS(SLEEP_WAKE_NODE, gpios)
#define SLEEP 0
#define WAKE 1

#define POWER_ENABLE_NODE DT_PATH(jetson_power_pins, power_enable)
#define POWER_ENABLE_CTLR DT_GPIO_CTLR(POWER_ENABLE_NODE, gpios)
#define POWER_ENABLE_PIN DT_GPIO_PIN(POWER_ENABLE_NODE, gpios)
#define POWER_ENABLE_FLAGS DT_GPIO_FLAGS(POWER_ENABLE_NODE, gpios)
#define ENABLE 1
#define DISABLE 0

#define SYSTEM_RESET_NODE DT_PATH(jetson_power_pins, system_reset)
#define SYSTEM_RESET_CTLR DT_GPIO_CTLR(SYSTEM_RESET_NODE, gpios)
#define SYSTEM_RESET_PIN DT_GPIO_PIN(SYSTEM_RESET_NODE, gpios)
#define SYSTEM_RESET_FLAGS DT_GPIO_FLAGS(SYSTEM_RESET_NODE, gpios)
#define RESET 1
#define OUT_OF_RESET 0

#define LTE_GPS_USB_RESET_NODE DT_PATH(lte_gps_usb_reset)
#define LTE_GPS_USB_RESET_CTLR DT_GPIO_CTLR(LTE_GPS_USB_RESET_NODE, gpios)
#define LTE_GPS_USB_RESET_PIN DT_GPIO_PIN(LTE_GPS_USB_RESET_NODE, gpios)
#define LTE_GPS_USB_RESET_FLAGS DT_GPIO_FLAGS(LTE_GPS_USB_RESET_NODE, gpios)
#define LTE_GPS_USB_ON 0

int
turn_on_jetson(void)
{
    const struct device *sleep_wake = DEVICE_DT_GET(SLEEP_WAKE_CTLR);
    const struct device *power_enable = DEVICE_DT_GET(SLEEP_WAKE_CTLR);
    const struct device *system_reset = DEVICE_DT_GET(SYSTEM_RESET_CTLR);
    const struct device *lte_gps_usb_reset =
        DEVICE_DT_GET(LTE_GPS_USB_RESET_CTLR);

    if (check_is_ready(sleep_wake, "sleep wake pin") ||
        check_is_ready(power_enable, "power enable pin") ||
        check_is_ready(system_reset, "system reset pin"))
    {
        return 1;
    }

    if (gpio_pin_configure(sleep_wake, SLEEP_WAKE_PIN,
                           SLEEP_WAKE_FLAGS | GPIO_OUTPUT))
    {
        LOG_ERR("Error configuring sleep wake pin!");
        return 1;
    }

    if (gpio_pin_configure(power_enable, POWER_ENABLE_PIN,
                           POWER_ENABLE_FLAGS | GPIO_OUTPUT))
    {
        LOG_ERR("Error configuring power enable pin!");
        return 1;
    }

    if (gpio_pin_configure(system_reset, SYSTEM_RESET_PIN,
                           SYSTEM_RESET_FLAGS | GPIO_INPUT))
    {
        LOG_ERR("Error configuring system reset pin!");
        return 1;
    }

    if (gpio_pin_configure(lte_gps_usb_reset, LTE_GPS_USB_RESET_PIN,
                           LTE_GPS_USB_RESET_FLAGS | GPIO_OUTPUT))
    {
        LOG_ERR("Error configuring LTE/GPS/USB reset pin!");
        return 1;
    }

    LOG_INF("Setting Jetson to WAKE mode");
    gpio_pin_set(sleep_wake, SLEEP_WAKE_PIN, WAKE);
    k_msleep(300);

    LOG_INF("Enabling LTE, GPS, and USB");
    gpio_pin_set(lte_gps_usb_reset, LTE_GPS_USB_RESET_PIN, LTE_GPS_USB_ON);

    LOG_INF("Enabling Jetson power");
    gpio_pin_set(power_enable, POWER_ENABLE_PIN, ENABLE);

    LOG_INF("Waiting for reset done signal from Jetson");
    while (gpio_pin_get(system_reset, SYSTEM_RESET_PIN) != OUT_OF_RESET)
        ;
    LOG_INF("Reset done");

    return 0;
}
