#define DT_DRV_COMPAT rohm_bu27030

#include "bu27030.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(BU27030, CONFIG_SENSOR_LOG_LEVEL);

#define DATA_TRANSFER_COEF (100 * 256) // 100ms, 256x
#define SENSOR_MEAS_MODE   100
#define SENSOR_GAIN        1

#ifndef SENSOR_GAIN
#define SENSOR_GAIN 1
#endif

#if SENSOR_GAIN == 32
#define GAIN_REG_VALUE 0xAA
#else
#define GAIN_REG_VALUE 0x22 // default gain
#endif

const float data_coefficient[] = {
    0.29f,      0.001646f, -0.000253f, -0.29f,  0.0f,      0.35f,     0.001646f,
    -0.000253f, -0.29f,    5.833f,     0.40f,   0.001646f, -0.00253f, -0.285f,
    -10.0f,     0.001646f, -0.00253f,  -0.294f, -1.417f};

static int
bu27030_reg_read(const struct device *dev, uint8_t reg, uint8_t *val)
{
    struct i2c_msg msgs[2] = {
        {
            .buf = &reg,
            .len = 1,
            .flags = I2C_MSG_WRITE,
        },
        {.buf = val,
         .len = 1,
         .flags = I2C_MSG_READ | I2C_MSG_RESTART | I2C_MSG_STOP},
    };

    struct bu27030_config *config = (struct bu27030_config *)dev->config;
    if (i2c_transfer(config->i2c.bus, msgs, ARRAY_SIZE(msgs),
                     config->i2c.addr) != 0) {
        return -EIO;
    }

    return 0;
}

static int
bu27030_reg_write(const struct device *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx_buf[2] = {reg, val};

    struct bu27030_config *config = (struct bu27030_config *)dev->config;
    return i2c_write(config->i2c.bus, tx_buf, sizeof(tx_buf), config->i2c.addr);
}

static int
bu27030_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct bu27030_data *drv_data = dev->data;
    uint8_t val_h, val_l, status;

    __ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_LIGHT);

    drv_data->data0 = 0U;
    drv_data->data1 = 0U;

    if (bu27030_reg_read(dev, BU27030_MODE_CONTROL3, &status) != 0) {
        return -EIO;
    }

    // check if valid data
    if (status & 0x80) {
        if (bu27030_reg_read(dev, BU27030_DATA0_HIGH_BYTE, &val_h) != 0) {
            return -EIO;
        }
        if (bu27030_reg_read(dev, BU27030_DATA0_LOW_BYTE, &val_l) != 0) {
            return -EIO;
        }

        drv_data->data0 = (((uint16_t)val_h) << 8) + val_l;

        if (bu27030_reg_read(dev, BU27030_DATA1_HIGH_BYTE, &val_h) != 0) {
            return -EIO;
        }
        if (bu27030_reg_read(dev, BU27030_DATA1_LOW_BYTE, &val_l) != 0) {
            return -EIO;
        }

        drv_data->data1 = (((uint16_t)val_h) << 8) + val_l;
    } else {
        return -EBUSY;
    }

    return 0;
}

/// Conversion to lux based on
/// https://github.com/MAVProxyUser/athena_drivers_st/blob/devel/k91_main_source.0x08040000/Core/Src/bu27030_driver.c
/// \param dev BU27030 device
/// \param chan supported channel is SENSOR_CHAN_LIGHT
/// \param val pointer to use to set the sensor value into
/// \return
///     * 0 success
///     * -ENOTSUP channel not supported
///     * -ERANGE value out of range, consider modifying the gain
static int
bu27030_channel_get(const struct device *dev, enum sensor_channel chan,
                    struct sensor_value *val)
{
    struct bu27030_data *drv_data = dev->data;

    // reset values, 0 lux is considered an error
    val->val1 = 0;
    val->val2 = 0;

    if (chan != SENSOR_CHAN_LIGHT) {
        return -ENOTSUP;
    }

    if (drv_data->data0 == 0xFFFF) {
        LOG_WRN("Value maxed out, consider decreasing the gain");
        return -ERANGE;
    }

    // Prevent divide by zero
    if (drv_data->data0 == 0) {
        LOG_WRN("Value at zero, consider increasing the gain");
        return -ERANGE;
    }

    // scale value as if it were measured using x256 gain and 100ms period
    uint32_t data0 =
        drv_data->data0 * (DATA_TRANSFER_COEF / SENSOR_MEAS_MODE / SENSOR_GAIN);
    uint32_t data1 =
        drv_data->data1 * (DATA_TRANSFER_COEF / SENSOR_MEAS_MODE / SENSOR_GAIN);

    float tmp1, tmp2;
    if (data1 < data0 * data_coefficient[0]) {
        tmp1 = data_coefficient[1] * data0 + data_coefficient[2] * data1;
        tmp2 = ((data1 / data0 - data_coefficient[3]) * data_coefficient[4] +
                1.0f);
    } else if (data1 < data0 * data_coefficient[5]) {
        tmp1 = data_coefficient[6] * data0 + data_coefficient[7] * data1;
        tmp2 = ((data1 / data0 - data_coefficient[8]) * data_coefficient[9] +
                1.0f);
    } else if (data1 < data0 * data_coefficient[10]) {
        tmp1 = data_coefficient[11] * data0 + data_coefficient[12] * data1;
        tmp2 = ((data1 / data0 - data_coefficient[13]) * data_coefficient[14] +
                1.0f);
    } else {
        tmp1 = data_coefficient[15] * data0 + data_coefficient[16] * data1;
        tmp2 = ((data1 / data0 - data_coefficient[17]) * data_coefficient[18] +
                1.0f);
    }

    float lx = tmp1 * tmp2;

    if (lx < 0.0) {
        return -ERANGE;
    }

    // integer part and fractional part (in millionth)
    val->val1 = (int32_t)lx;
    val->val2 = (int32_t)(lx * 1000000.0) % 1000000;

    return 0;
}

static const struct sensor_driver_api bu27030_driver_api = {
    .attr_set = NULL,
    .sample_fetch = bu27030_sample_fetch,
    .channel_get = bu27030_channel_get,
};

int
bu27030_init(const struct device *dev)
{
    int err_code;

    // software reset
    (void)bu27030_reg_write(dev, BU27030_SYSTEM_CONTROL, 0x80);

    // read part ID
    uint8_t part_id = 0;
    err_code = bu27030_reg_read(dev, BU27030_SYSTEM_CONTROL, &part_id);

    if (err_code == 0 && part_id == BU27030_PART_ID) {
        LOG_INF("BU27030 initialized");
    } else {
        LOG_ERR("Error initializing BU27030, ret %d, part ID: 0x%x", err_code,
                part_id);
    }

    // set sensor gain
    err_code = bu27030_reg_write(dev, BU27030_MODE_CONTROL2, GAIN_REG_VALUE);
    __ASSERT(err_code == 0, "Unable to change settings of BU27030");

    // enable measurement
    err_code = bu27030_reg_write(dev, BU27030_MODE_CONTROL3, 0x01);
    __ASSERT(err_code == 0, "Unable to start BU27030");

    return err_code;
}

#define BU27030_INIT(inst)                                                     \
    static struct bu27030_config bu27030_##inst##_config = {                   \
        .i2c = I2C_DT_SPEC_INST_GET(inst)};                                    \
                                                                               \
    static struct bu27030_data bu27030_##inst##_data;                          \
                                                                               \
    DEVICE_DT_INST_DEFINE(inst, bu27030_init, NULL, &bu27030_##inst##_data,    \
                          &bu27030_##inst##_config, POST_KERNEL,               \
                          CONFIG_SENSOR_INIT_PRIORITY, &bu27030_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BU27030_INIT)
