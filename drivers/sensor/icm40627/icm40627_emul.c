/*
 * Copyright (c) 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT invensense_icm40627

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/logging/log.h>

#include <icm40627_reg.h>

LOG_MODULE_DECLARE(ICM40627, CONFIG_SENSOR_LOG_LEVEL);

#define NUM_REGS (UINT8_MAX >> 1)

struct icm40627_emul_data {
    uint8_t reg[NUM_REGS];
};

struct icm40627_emul_cfg {
};

void
icm40627_emul_set_reg(const struct emul *target, uint8_t reg_addr,
                      const uint8_t *val, size_t count)
{
    struct icm40627_emul_data *data = target->data;

    __ASSERT_NO_MSG(reg_addr + count < NUM_REGS);
    memcpy(data->reg + reg_addr, val, count);
}

void
icm40627_emul_get_reg(const struct emul *target, uint8_t reg_addr, uint8_t *val,
                      size_t count)
{
    struct icm40627_emul_data *data = target->data;

    __ASSERT_NO_MSG(reg_addr + count < NUM_REGS);
    memcpy(val, data->reg + reg_addr, count);
}

static void
icm40627_emul_handle_write(const struct emul *target, uint8_t regn,
                           uint8_t value)
{
    struct icm40627_emul_data *data = target->data;

    switch (regn) {
    case REG_DEVICE_CONFIG:
        if (FIELD_GET(BIT_SOFT_RESET, value) == 1) {
            /* Perform a soft reset */
            memset(data->reg, 0, NUM_REGS);
            /* Initialized the who-am-i register */
            data->reg[REG_WHO_AM_I] = WHO_AM_I_ICM40627;
            /* Set the bit for the reset being done */
            data->reg[REG_INT_STATUS] |= BIT_INT_STATUS_RESET_DONE;
        }
        break;
    }
}

static int
icm40627_emul_io_i2c(const struct emul *target, const struct i2c_config *config,
                     const struct i2c_buf_set *tx_bufs,
                     const struct i2c_buf_set *rx_bufs)
{
    struct icm40627_emul_data *data = target->data;
    const struct i2c_buf *tx, *rx;
    uint8_t regn;
    bool is_read;

    ARG_UNUSED(config);
    __ASSERT_NO_MSG(tx_bufs != NULL);

    tx = tx_bufs->buffers;
    __ASSERT_NO_MSG(tx != NULL);
    __ASSERT_NO_MSG(tx->len > 0);

    regn = *(uint8_t *)tx->buf;
    is_read = FIELD_GET(REG_SPI_READ_BIT, regn);
    regn &= GENMASK(6, 0);
    if (is_read) {
        __ASSERT_NO_MSG(rx_bufs != NULL);
        __ASSERT_NO_MSG(rx_bufs->count > 1);

        rx = &rx_bufs->buffers[1];
        __ASSERT_NO_MSG(rx->buf != NULL);
        __ASSERT_NO_MSG(rx->len > 0);
        for (uint16_t i = 0; i < rx->len; ++i) {
            ((uint8_t *)rx->buf)[i] = data->reg[regn + i];
        }
    } else {
        /* Writing to regn */
        uint8_t value;

        __ASSERT_NO_MSG(tx_bufs->count > 1);
        tx = &tx_bufs->buffers[1];

        __ASSERT_NO_MSG(tx->len > 0);
        value = ((uint8_t *)tx->buf)[0];
        icm40627_emul_handle_write(target, regn, value);
    }

    return 0;
}

static int
icm40627_emul_init(const struct emul *target, const struct device *parent)
{
    struct icm40627_emul_data *data = target->data;

    /* Initialized the who-am-i register */
    data->reg[REG_WHO_AM_I] = WHO_AM_I_ICM40627;

    return 0;
}

static const struct i2c_emul_api icm40627_emul_i2c_api = {
    .io = icm40627_emul_io_i2c,
};

#define ICM40627_EMUL_DEFINE(n, api)                                           \
    EMUL_DT_INST_DEFINE(n, icm40627_emul_init, &icm40627_emul_data_##n,        \
                        &icm40627_emul_cfg_##n, &api, NULL)

#define ICM40627_EMUL_SPI(n)                                                   \
    static struct icm40627_emul_data icm40627_emul_data_##n;                   \
    static const struct icm40627_emul_cfg icm40627_emul_cfg_##n;               \
    ICM40627_EMUL_DEFINE(n, icm40627_emul_i2c_api)

DT_INST_FOREACH_STATUS_OKAY(ICM40627_EMUL_SPI)
