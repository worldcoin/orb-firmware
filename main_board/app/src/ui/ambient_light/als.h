#pragma once

#include <zephyr/kernel.h>

int
als_init(struct k_mutex *i2c_mux_mutex);
