/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "icm40627_i2c.h"
#include "icm40627_reg.h"
#include <zephyr/sys/util.h>

static inline int
i2c_write_register(const struct i2c_dt_spec *bus, uint8_t reg, uint8_t data)
{
    uint8_t tx_buf[2] = {reg, data};

    return i2c_write_dt(bus, tx_buf, sizeof(tx_buf));
}

static inline int
i2c_read_register(const struct i2c_dt_spec *bus, uint8_t reg, uint8_t *data,
                  size_t len)
{
    struct i2c_msg msgs[2] = {
        {
            .buf = &reg,
            .len = 1,
            .flags = I2C_MSG_WRITE,
        },
        {.buf = data,
         .len = len,
         .flags = I2C_MSG_READ | I2C_MSG_RESTART | I2C_MSG_STOP},
    };

    return i2c_transfer_dt(bus, msgs, ARRAY_SIZE(msgs));
}

int
icm40627_i2c_read(const struct i2c_dt_spec *bus, uint16_t reg, uint8_t *data,
                  size_t len)
{
    int res = 0;
    uint8_t address = FIELD_GET(REG_ADDRESS_MASK, reg);

    res = i2c_read_register(bus, address, data, len);

    return res;
}

int
icm40627_i2c_update_register(const struct i2c_dt_spec *bus, uint16_t reg,
                             uint8_t mask, uint8_t data)
{
    uint8_t temp = 0;
    int res = icm40627_i2c_read(bus, reg, &temp, 1);

    if (res) {
        return res;
    }

    temp &= ~mask;
    temp |= FIELD_PREP(mask, data);

    return icm40627_i2c_single_write(bus, reg, temp);
}

int
icm40627_i2c_single_write(const struct i2c_dt_spec *bus, uint16_t reg,
                          uint8_t data)
{
    int res = 0;
    uint8_t address = FIELD_GET(REG_ADDRESS_MASK, reg);

    res = i2c_write_register(bus, address, data);

    return res;
}
