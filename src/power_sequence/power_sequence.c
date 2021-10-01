#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/regulator.h>
#include <stdio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(power_sequence);

#include "power_sequence.h"

#define SUPPLY_5V_PG_NODE DT_PATH(supply_5v, power_good)
#define SUPPLY_5V_PG_CTLR DT_GPIO_CTLR(SUPPLY_5V_PG_NODE, gpios)
#define SUPPLY_5V_PG_PIN DT_GPIO_PIN(SUPPLY_5V_PG_NODE, gpios)
#define SUPPLY_5V_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_5V_PG_NODE, gpios)

#define SUPPLY_3V3_PG_NODE DT_PATH(supply_3v3, power_good)
#define SUPPLY_3V3_PG_CTLR DT_GPIO_CTLR(SUPPLY_3V3_PG_NODE, gpios)
#define SUPPLY_3V3_PG_PIN DT_GPIO_PIN(SUPPLY_3V3_PG_NODE, gpios)
#define SUPPLY_3V3_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_3V3_PG_NODE, gpios)

#define FORMAT_STRING "Checking that %s is ready... "

static int check_is_ready(const struct device *dev, const char *name)
{
    if (!device_is_ready(dev)) {
        LOG_ERR(FORMAT_STRING "no", name);
        return 1;
    }

    LOG_INF(FORMAT_STRING "yes", name);
    return 0;
}

int turn_on_essential_power_supplies(void)
{
    const struct device *vbat_sw_regulator = DEVICE_DT_GET(DT_PATH(vbat_sw));
    const struct device *supply_12v = DEVICE_DT_GET(DT_PATH(supply_12v));
    const struct device *supply_5v = DEVICE_DT_GET(DT_PATH(supply_5v));
    const struct device *supply_5v_pg = DEVICE_DT_GET(SUPPLY_5V_PG_CTLR);
    const struct device *supply_3v3 = DEVICE_DT_GET(DT_PATH(supply_3v3));
    const struct device *supply_3v3_pg = DEVICE_DT_GET(SUPPLY_3V3_PG_CTLR);

    if (check_is_ready(vbat_sw_regulator, "VBAT SW")
        || check_is_ready(supply_12v, "12V supply")
        || check_is_ready(supply_5v, "5V supply")
		|| check_is_ready(supply_5v_pg, "5V supply power good pin")
        || check_is_ready(supply_3v3, "3.3V supply")
        || check_is_ready(supply_3v3_pg, "3.3V supply power good pin")) {
        return 1;
    }

    regulator_enable(vbat_sw_regulator, NULL);
    LOG_INF("VBAT SW enabled");
    k_msleep(100);

    regulator_enable(supply_12v, NULL);
    LOG_INF("12V power supply enabled");
    k_msleep(100);

    if (gpio_pin_configure(supply_5v_pg, SUPPLY_5V_PG_PIN, SUPPLY_5V_PG_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring 5v pg pin!");
        return 1;
	}

    regulator_enable(supply_5v, NULL);
    LOG_INF("5V power supply enabled");
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on anything else
    while (!gpio_pin_get(supply_5v_pg, SUPPLY_5V_PG_PIN))
        ;
    LOG_INF("5V power supply good");

    if (gpio_pin_configure(supply_3v3_pg, SUPPLY_3V3_PG_PIN, SUPPLY_3V3_PG_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring 3.3v pg pin!");
        return 1;
	}

    regulator_enable(supply_3v3, NULL);
    LOG_INF("3.3V power supply enabled");
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on the fan
    // And if we can't turn on the fan, then we don't want to turn on anything else
    while (!gpio_pin_get(supply_3v3_pg, SUPPLY_3V3_PG_PIN))
        ;
    LOG_INF("3.3V power supply good");

    return 0;
}
