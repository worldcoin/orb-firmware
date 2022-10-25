#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_scan, CONFIG_I2C_SCAN_LOG_LEVEL);

// not re-entrant
static const char *
int_to_binary_str(uint8_t n)
{
    static char buffer[] = "0bxxxxxxxx";

    for (int i = 7, j = 2; i >= 0; --i, j++) {
        buffer[j] = n & BIT(i) ? '1' : '0';
    }

    return buffer;
}

void
i2c_scan_and_log(const struct device *i2c_dev)
{
    int ret;

    LOG_INF("Starting I2C scan...");
    LOG_INF("Showing addresses in hex and binary...");
    if (device_is_ready(i2c_dev)) {
        for (uint8_t i = 0; i < 0x7f; ++i) {
            uint8_t byte;
            ret = i2c_read(i2c_dev, &byte, 1, i);
            if (!ret) {
                LOG_INF("Found device at address 0x%02x / %s", i,
                        int_to_binary_str(i));
                k_msleep(200);
            }
        }
        LOG_INF("I2C scan done.");
    } else {
        LOG_ERR("i2c device not ready!");
    }
}
