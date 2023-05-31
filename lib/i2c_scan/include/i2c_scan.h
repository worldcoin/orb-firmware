#pragma once

#include <zephyr/device.h>

void
i2c_scan_and_log(const struct device *i2c_dev);
