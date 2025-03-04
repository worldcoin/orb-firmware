#pragma once

#include <common.pb.h>
#include <zephyr/kernel.h>

int
als_init(const orb_mcu_Hardware *hw_version, struct k_mutex *i2c_mux_mutex);
