#include <zephyr.h>
#include <device.h>
#include <drivers/regulator.h>

#include <sys/printk.h>
#include <string.h>

void
main(void)
{
    printk("Hello, world!\n");
    printk("This is the orb\n");

    const struct device *vbat_sw_regulator = DEVICE_DT_GET(DT_PATH(vbat_sw));
    const struct device *supply_12v = DEVICE_DT_GET(DT_PATH(supply_12v));

    if (!device_is_ready(vbat_sw_regulator) || !(device_is_ready(supply_12v))) {
        printk("12V supply not ready!\n");
        return;
    }

    regulator_enable(vbat_sw_regulator, NULL);
    k_msleep(100);
    regulator_enable(supply_12v, NULL);

    while(1);
}
