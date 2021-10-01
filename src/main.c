#include <zephyr.h>
#include <device.h>
#include <drivers/regulator.h>
#include <sys/printk.h>
#include <string.h>

#include <logging/log.h>
#include <canbus.h>
LOG_MODULE_REGISTER(main);

void
main(void)
{
    /* standard print */
    LOG_INF("Hello from Orb");

    const struct device *vbat_sw_regulator = DEVICE_DT_GET(DT_PATH(vbat_sw));
    const struct device *supply_12v = DEVICE_DT_GET(DT_PATH(supply_12v));

    if (!device_is_ready(vbat_sw_regulator) || !(device_is_ready(supply_12v)))
    {
        LOG_ERR("12V supply not ready!");
        return;
    }

    regulator_enable(vbat_sw_regulator, NULL);
    k_msleep(100);
    regulator_enable(supply_12v, NULL);

    canbus_init();
}
