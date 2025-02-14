/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_ICM40627_I2C_H_
#define ZEPHYR_DRIVERS_SENSOR_ICM40627_I2C_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

/**
 * @brief perform a single I2C write to a ICM40627 register
 *
 * this functions wraps all logic necessary to write to any of the ICM40627
 * registers, regardless of which memory bank the register belongs to.
 *
 * @param bus I2C bus pointer
 * @param reg address of ICM40627 register to write to
 * @param data data byte to write to register
 * @return int 0 on success, negative error code otherwise
 */
int
icm40627_i2c_single_write(const struct i2c_dt_spec *bus, uint16_t reg,
                          uint8_t data);

/**
 * @brief update a single ICM40627 register value
 *
 * this functions wraps all logic necessary to update any of the ICM40627
 * registers, regardless of which memory bank the register belongs to.
 *
 * @param bus I2C bus pointer
 * @param reg address of ICM40627 register to update
 * @param mask bitmask defining which bits of the register to update
 * @param data new value to update register with, respecting the bitmask
 * @return int 0 on success, negative error code otherwise
 */
int
icm40627_i2c_update_register(const struct i2c_dt_spec *bus, uint16_t reg,
                             uint8_t mask, uint8_t data);

/**
 * @brief read from one or more ICM40627 registers
 *
 * this functions wraps all logic necessary to read from any of the ICM40627
 * registers, regardless of which memory bank the register belongs to.
 *
 * @param bus I2C bus pointer
 * @param reg start address of ICM40627 register(s) to read from
 * @param data pointer to byte array to read register values to
 * @param len number of bytes to read from the device
 * @return int 0 on success, negative error code otherwise
 */
int
icm40627_i2c_read(const struct i2c_dt_spec *bus, uint16_t reg, uint8_t *data,
                  size_t len);

#endif /* ZEPHYR_DRIVERS_SENSOR_ICM40627_I2C_H_ */
