/*
 * Copyright (c) 2022 Intel Corporation
 * Copyright (c) 2022 Esco Medical ApS
 * Copyright (c) 2020 TDK Invensense
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT invensense_icm40627

#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>

#include "icm40627.h"
#include "icm40627_i2c.h"
#include "icm40627_reg.h"
#include "icm40627_trigger.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ICM40627, CONFIG_SENSOR_LOG_LEVEL);

struct icm40627_sensor_data {
    struct icm40627_dev_data dev_data;

    int16_t readings[7];
};

struct icm40627_sensor_config {
    struct icm40627_dev_cfg dev_cfg;
};

static void
icm40627_convert_accel(struct sensor_value *val, int16_t raw_val,
                       struct icm40627_cfg *cfg)
{
    icm40627_accel_ms(cfg, (int32_t)raw_val, &val->val1, &val->val2);
}

static void
icm40627_convert_gyro(struct sensor_value *val, int16_t raw_val,
                      struct icm40627_cfg *cfg)
{
    icm40627_gyro_rads(cfg, (int32_t)raw_val, &val->val1, &val->val2);
}

static inline void
icm40627_convert_temp(struct sensor_value *val, int16_t raw_val)
{
    icm40627_temp_c((int32_t)raw_val, &val->val1, &val->val2);
}

static int
icm40627_channel_get(const struct device *dev, enum sensor_channel chan,
                     struct sensor_value *val)
{
    struct icm40627_sensor_data *data = dev->data;

    switch (chan) {
    case SENSOR_CHAN_ACCEL_XYZ:
        icm40627_convert_accel(&val[0], data->readings[1], &data->dev_data.cfg);
        icm40627_convert_accel(&val[1], data->readings[2], &data->dev_data.cfg);
        icm40627_convert_accel(&val[2], data->readings[3], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_ACCEL_X:
        icm40627_convert_accel(val, data->readings[1], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_ACCEL_Y:
        icm40627_convert_accel(val, data->readings[2], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_ACCEL_Z:
        icm40627_convert_accel(val, data->readings[3], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_GYRO_XYZ:
        icm40627_convert_gyro(&val[0], data->readings[4], &data->dev_data.cfg);
        icm40627_convert_gyro(&val[1], data->readings[5], &data->dev_data.cfg);
        icm40627_convert_gyro(&val[2], data->readings[6], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_GYRO_X:
        icm40627_convert_gyro(val, data->readings[4], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_GYRO_Y:
        icm40627_convert_gyro(val, data->readings[5], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_GYRO_Z:
        icm40627_convert_gyro(val, data->readings[6], &data->dev_data.cfg);
        break;
    case SENSOR_CHAN_DIE_TEMP:
        icm40627_convert_temp(val, data->readings[0]);
        break;
    default:
        return -ENOTSUP;
    }

    return 0;
}

static int
icm40627_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    uint8_t status;
    struct icm40627_sensor_data *data = dev->data;
    const struct icm40627_sensor_config *cfg = dev->config;

    int res = icm40627_i2c_read(&cfg->dev_cfg.i2c, REG_INT_STATUS, &status, 1);

    if (res) {
        return res;
    }

    if (!FIELD_GET(BIT_INT_STATUS_DATA_RDY, status)) {
        return -EBUSY;
    }

    uint8_t readings[14];

    res = icm40627_read_all(dev, readings);

    if (res) {
        return res;
    }

    for (int i = 0; i < 7; i++) {
        data->readings[i] =
            sys_le16_to_cpu((readings[i * 2] << 8) | readings[i * 2 + 1]);
    }

    return 0;
}

static int
icm40627_attr_set(const struct device *dev, enum sensor_channel chan,
                  enum sensor_attribute attr, const struct sensor_value *val)
{
    const struct icm40627_sensor_data *data = dev->data;
    struct icm40627_cfg new_config = data->dev_data.cfg;
    int res = 0;

    __ASSERT_NO_MSG(val != NULL);

    switch (chan) {
    case SENSOR_CHAN_ACCEL_X:
    case SENSOR_CHAN_ACCEL_Y:
    case SENSOR_CHAN_ACCEL_Z:
    case SENSOR_CHAN_ACCEL_XYZ:
        if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
            new_config.accel_odr = icm40627_accel_hz_to_reg(val->val1);
        } else if (attr == SENSOR_ATTR_FULL_SCALE) {
            new_config.accel_fs =
                icm40627_accel_fs_to_reg(sensor_ms2_to_g(val));
        } else {
            LOG_ERR("Unsupported attribute");
            res = -ENOTSUP;
        }
        break;
    case SENSOR_CHAN_GYRO_X:
    case SENSOR_CHAN_GYRO_Y:
    case SENSOR_CHAN_GYRO_Z:
    case SENSOR_CHAN_GYRO_XYZ:
        if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
            new_config.gyro_odr = icm40627_gyro_odr_to_reg(val->val1);
        } else if (attr == SENSOR_ATTR_FULL_SCALE) {
            new_config.gyro_fs =
                icm40627_gyro_fs_to_reg(sensor_rad_to_degrees(val));
        } else {
            LOG_ERR("Unsupported attribute");
            res = -EINVAL;
        }
        break;
    default:
        LOG_ERR("Unsupported channel");
        res = -EINVAL;
        break;
    }

    if (res) {
        return res;
    }
    return icm40627_safely_configure(dev, &new_config);
}

static int
icm40627_attr_get(const struct device *dev, enum sensor_channel chan,
                  enum sensor_attribute attr, struct sensor_value *val)
{
    const struct icm40627_sensor_data *data = dev->data;
    const struct icm40627_cfg *cfg = &data->dev_data.cfg;
    int res = 0;

    __ASSERT_NO_MSG(val != NULL);

    switch (chan) {
    case SENSOR_CHAN_ACCEL_X:
    case SENSOR_CHAN_ACCEL_Y:
    case SENSOR_CHAN_ACCEL_Z:
    case SENSOR_CHAN_ACCEL_XYZ:
        if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
            icm40627_accel_reg_to_hz(cfg->accel_odr, val);
        } else if (attr == SENSOR_ATTR_FULL_SCALE) {
            icm40627_accel_reg_to_fs(cfg->accel_fs, val);
        } else {
            LOG_ERR("Unsupported attribute");
            res = -EINVAL;
        }
        break;
    case SENSOR_CHAN_GYRO_X:
    case SENSOR_CHAN_GYRO_Y:
    case SENSOR_CHAN_GYRO_Z:
    case SENSOR_CHAN_GYRO_XYZ:
        if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
            icm40627_gyro_reg_to_odr(cfg->gyro_odr, val);
        } else if (attr == SENSOR_ATTR_FULL_SCALE) {
            icm40627_gyro_reg_to_fs(cfg->gyro_fs, val);
        } else {
            LOG_ERR("Unsupported attribute");
            res = -EINVAL;
        }
        break;
    default:
        LOG_ERR("Unsupported channel");
        res = -EINVAL;
        break;
    }

    return res;
}

static const struct sensor_driver_api icm40627_driver_api = {
    .sample_fetch = icm40627_sample_fetch,
    .channel_get = icm40627_channel_get,
    .attr_set = icm40627_attr_set,
    .attr_get = icm40627_attr_get,
#ifdef CONFIG_ICM40627_TRIGGER
    .trigger_set = icm40627_trigger_set,
#endif
};

int
icm40627_init(const struct device *dev)
{
    struct icm40627_sensor_data *data = dev->data;
    const struct icm40627_sensor_config *cfg = dev->config;
    int res;

    if (!i2c_is_ready_dt(&cfg->dev_cfg.i2c)) {
        LOG_ERR("I2C bus is not ready");
        return -ENODEV;
    }

    if (icm40627_reset(dev)) {
        LOG_ERR("could not initialize sensor");
        return -EIO;
    }

#ifdef CONFIG_ICM40627_TRIGGER
    res = icm40627_trigger_init(dev);
    if (res != 0) {
        LOG_ERR("Failed to initialize triggers");
        return res;
    }

    res = icm40627_trigger_enable_interrupt(dev);
    if (res != 0) {
        LOG_ERR("Failed to enable triggers");
        return res;
    }
#endif

    data->dev_data.cfg.accel_mode = ICM40627_ACCEL_LN;
    data->dev_data.cfg.gyro_mode = ICM40627_GYRO_LN;
    data->dev_data.cfg.accel_fs = ICM40627_ACCEL_FS_2G;
    data->dev_data.cfg.gyro_fs = ICM40627_GYRO_FS_125;
    data->dev_data.cfg.accel_odr = ICM40627_ACCEL_ODR_1000;
    data->dev_data.cfg.gyro_odr = ICM40627_GYRO_ODR_1000;

    res = icm40627_configure(dev, &data->dev_data.cfg);
    if (res != 0) {
        LOG_ERR("Failed to configure");
        return res;
    }

    return 0;
}

#ifndef CONFIG_ICM40627_TRIGGER
void
icm40627_lock(const struct device *dev)
{
    ARG_UNUSED(dev);
}
void
icm40627_unlock(const struct device *dev)
{
    ARG_UNUSED(dev);
}
#endif

#define ICM40627_INIT(inst)                                                    \
    static struct icm40627_sensor_data icm40627_driver_##inst = {0};           \
                                                                               \
    static const struct icm40627_sensor_config icm40627_cfg_##inst = {         \
        .dev_cfg =                                                             \
            {                                                                  \
                .i2c = I2C_DT_SPEC_INST_GET(inst),                             \
                .gpio_int1 =                                                   \
                    GPIO_DT_SPEC_INST_GET_BY_IDX_OR(inst, int_gpios, 0, {0}),  \
                .gpio_int2 =                                                   \
                    GPIO_DT_SPEC_INST_GET_BY_IDX_OR(inst, int_gpios, 1, {0}),  \
            },                                                                 \
    };                                                                         \
                                                                               \
    SENSOR_DEVICE_DT_INST_DEFINE(                                              \
        inst, icm40627_init, NULL, &icm40627_driver_##inst,                    \
        &icm40627_cfg_##inst, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,        \
        &icm40627_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ICM40627_INIT)
