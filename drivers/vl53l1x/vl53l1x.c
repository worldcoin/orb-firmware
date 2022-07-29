/* vl53l1x.c - Driver for ST VL53L1X time of flight sensor */

#define DT_DRV_COMPAT st_vl53l1x

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/types.h>

#include "vl53l1_api.h"
#include "vl53l1_platform.h"

LOG_MODULE_REGISTER(VL53L1X, CONFIG_SENSOR_LOG_LEVEL);

/* All the values used in this driver are coming from ST datasheet and examples.
 * It can be found here:
 *   https://www.st.com/en/embedded-software/stsw-img007.html
 */
#define VL53L1X_CHIP_ID 0xEACC

struct vl53l1x_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec xshut;
};

struct vl53l1x_data {
    bool started;
    VL53L1_Dev_t vl53l1x;
    VL53L1_RangingMeasurementData_t RangingMeasurementData;
};

static int
vl53l1x_start(const struct device *dev)
{
    const struct vl53l1x_config *const config = dev->config;
    struct vl53l1x_data *drv_data = dev->data;
    int r;
    VL53L1_Error ret;
    uint16_t vl53l1x_id = 0U;
    VL53L1_DeviceInfo_t vl53l1x_dev_info;

    LOG_DBG("[%s] Starting", dev->name);

    /* Pull XSHUT high to start the sensor */
    if (config->xshut.port) {
        r = gpio_pin_set_dt(&config->xshut, 1);
        if (r < 0) {
            LOG_ERR("[%s] Unable to set XSHUT gpio (error %d)", dev->name, r);
            return -EIO;
        }
        k_sleep(K_MSEC(2));
    }

    /* Get info from sensor */
    (void)memset(&vl53l1x_dev_info, 0, sizeof(VL53L1_DeviceInfo_t));

    ret = VL53L1_GetDeviceInfo(&drv_data->vl53l1x, &vl53l1x_dev_info);
    if (ret < 0) {
        LOG_ERR("[%s] Could not get info from device.", dev->name);
        return -ENODEV;
    }

    LOG_DBG("[%s] VL53L1X_GetDeviceInfo = %d", dev->name, ret);
    LOG_DBG("   Device Name : %s", vl53l1x_dev_info.Name);
    LOG_DBG("   Device Type : %s", vl53l1x_dev_info.Type);
    LOG_DBG("   Device ID : %s", vl53l1x_dev_info.ProductId);
    LOG_DBG("   ProductRevisionMajor : %d",
            vl53l1x_dev_info.ProductRevisionMajor);
    LOG_DBG("   ProductRevisionMinor : %d",
            vl53l1x_dev_info.ProductRevisionMinor);

    ret = VL53L1_RdWord(&drv_data->vl53l1x, VL53L1_IDENTIFICATION__MODEL_ID,
                        (uint16_t *)&vl53l1x_id);
    if ((ret < 0) || (vl53l1x_id != VL53L1X_CHIP_ID)) {
        LOG_ERR("[%s] Issue on device identification", dev->name);
        return -ENOTSUP;
    }

    ret = VL53L1_WaitDeviceBooted(&drv_data->vl53l1x);
    if (ret < 0) {
        LOG_ERR("[%s] VL53L1_WaitDeviceBooted return error (%d)", dev->name,
                ret);
        return -ENOTSUP;
    }

    /* sensor init */
    ret = VL53L1_DataInit(&drv_data->vl53l1x);
    if (ret < 0) {
        LOG_ERR("[%s] VL53L1X_DataInit return error (%d)", dev->name, ret);
        return -ENOTSUP;
    }

    ret = VL53L1_StaticInit(&drv_data->vl53l1x);
    if (ret) {
        LOG_ERR("[%s] VL53L1_StaticInit failed: %d", dev->name, ret);
        goto exit;
    }

    ret = VL53L1_SetDistanceMode(&drv_data->vl53l1x, VL53L1_DISTANCEMODE_SHORT);
    if (ret) {
        LOG_ERR("[%s] VL53L1_SetDistanceMode failed: %d", dev->name, ret);
        goto exit;
    }

    ret = VL53L1_PerformRefSpadManagement(&drv_data->vl53l1x);
    if (ret) {
        LOG_ERR("[%s] VL53L1_PerformRefSpadManagement failed: %d", dev->name,
                ret);
        goto exit;
    }

    ret = VL53L1_StartMeasurement(&drv_data->vl53l1x);
    if (ret) {
        LOG_ERR("[%s] VL53L1_StartMeasurement failed: %d", dev->name, ret);
        goto exit;
    }

    drv_data->started = true;
    LOG_DBG("[%s] Started", dev->name);

exit:
    return ret;
}

static int
vl53l1x_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct vl53l1x_data *drv_data = dev->data;
    VL53L1_Error ret;
    int r;

    __ASSERT_NO_MSG((chan == SENSOR_CHAN_ALL) ||
                    (chan == SENSOR_CHAN_DISTANCE) ||
                    (chan == SENSOR_CHAN_PROX));

    if (!drv_data->started) {
        r = vl53l1x_start(dev);
        if (r < 0) {
            return r;
        }
    }

    uint8_t ready;
    ret = VL53L1_GetMeasurementDataReady(&drv_data->vl53l1x, &ready);
    if (ret < 0) {
        LOG_ERR("[%s] VL53L1_GetMeasurementDataReady (error=%d)", dev->name,
                ret);
        return -EINVAL;
    }

    if (!ready) {
        return -ENODATA;
    }

    ret = VL53L1_GetRangingMeasurementData(&drv_data->vl53l1x,
                                           &drv_data->RangingMeasurementData);
    if (ret < 0) {
        LOG_ERR("[%s] Could not perform measurement (error=%d)", dev->name,
                ret);
        return -EINVAL;
    }

    ret = VL53L1_ClearInterruptAndStartMeasurement(&drv_data->vl53l1x);
    if (ret < 0) {
        LOG_ERR("[%s] Could not perform measurement (error=%d)", dev->name,
                ret);
        return -EINVAL;
    }

    return 0;
}

static int
vl53l1x_channel_get(const struct device *dev, enum sensor_channel chan,
                    struct sensor_value *val)
{
    struct vl53l1x_data *drv_data = dev->data;

    if (chan == SENSOR_CHAN_PROX) {
        if (drv_data->RangingMeasurementData.RangeMilliMeter <=
            CONFIG_VL53L1X_PROXIMITY_THRESHOLD) {
            val->val1 = 1;
        } else {
            val->val1 = 0;
        }
        val->val2 = 0;
    } else if (chan == SENSOR_CHAN_DISTANCE) {
        val->val1 = drv_data->RangingMeasurementData.RangeMilliMeter / 1000;
        val->val2 =
            (drv_data->RangingMeasurementData.RangeMilliMeter % 1000) * 1000;
    } else {
        return -ENOTSUP;
    }

    return 0;
}

static const struct sensor_driver_api vl53l1x_api_funcs = {
    .sample_fetch = vl53l1x_sample_fetch,
    .channel_get = vl53l1x_channel_get,
};

static int
vl53l1x_init(const struct device *dev)
{
    int r;
    struct vl53l1x_data *drv_data = dev->data;
    const struct vl53l1x_config *const config = dev->config;

    // get address from i2c peripheral
    drv_data->vl53l1x.i2c_slave_address = config->i2c.addr;
    drv_data->vl53l1x.i2c = config->i2c.bus;

    if (config->xshut.port) {
        r = gpio_pin_configure_dt(&config->xshut, GPIO_OUTPUT);
        if (r < 0) {
            LOG_ERR("[%s] Unable to configure GPIO as output", dev->name);
        }
    }

    r = vl53l1x_start(dev);
    if (r) {
        return r;
    }

    LOG_DBG("[%s] Initialized", dev->name);
    return 0;
}

#define VL53L1X_INIT(inst)                                                     \
    static struct vl53l1x_config vl53l1x_##inst##_config = {                   \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                     \
        .xshut = GPIO_DT_SPEC_INST_GET_OR(inst, xshut_gpios, {})};             \
                                                                               \
    static struct vl53l1x_data vl53l1x_##inst##_driver;                        \
                                                                               \
    DEVICE_DT_INST_DEFINE(inst, vl53l1x_init, NULL, &vl53l1x_##inst##_driver,  \
                          &vl53l1x_##inst##_config, POST_KERNEL,               \
                          CONFIG_SENSOR_INIT_PRIORITY, &vl53l1x_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(VL53L1X_INIT)
