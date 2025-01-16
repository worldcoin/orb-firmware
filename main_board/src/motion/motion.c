#include "motion.h"
#include "orb_logs.h"
#include <zephyr/device.h>

#include <app_assert.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(motion, CONFIG_MOTION_LOG_LEVEL);

static const struct device *const accel_gyro_dev =
    DEVICE_DT_GET(DT_NODELABEL(icm40627));

#define MOTION_SAMPLE_RATE_HZ 50

/**
 *  FIXME
 *  This code is disabled because it is not intended to be used in production
 *  and serves only as a proof of concept.
 */
#if 0

__maybe_unused static K_THREAD_STACK_DEFINE(motion_stack_area, 3024);
__maybe_unused static struct k_thread motion_thread_data;

__maybe_unused _Noreturn static void
motion_thread()
{
    // set output data rate
    const struct sensor_value odr = {
        .val1 = MOTION_SAMPLE_RATE_HZ,
        .val2 = 0,
    };
    int ret = sensor_attr_set(accel_gyro_dev, SENSOR_CHAN_ACCEL_XYZ,
                              SENSOR_ATTR_SAMPLING_FREQUENCY,
                              &odr);
    ASSERT_SOFT(ret);

    while (1) {
        k_msleep(1000 / MOTION_SAMPLE_RATE_HZ);

        int ret = sensor_sample_fetch(accel_gyro_dev);
        if (ret < 0) {
            LOG_ERR("Error fetching (period) data, ret %d", ret);
            continue;
        }

        struct sensor_value val[3] = {0};
        ret = sensor_channel_get(accel_gyro_dev, SENSOR_CHAN_ACCEL_XYZ, val);
        if (ret < 0) {
            LOG_ERR("Error getting data, ret %d", ret);
            continue;
        }

        LOG_INF(
            "Accel data for X: %" PRId32 ".%" PRId32 " Y: %" PRId32 ".%" PRId32
            " Z: %" PRId32 ".%" PRId32, val[0].val1, val[0].val2, val[1].val1,
            val[1].val2, val[2].val1, val[2].val2);
    }
}

#endif

int
motion_init(void)
{
    if (!device_is_ready(accel_gyro_dev)) {
        LOG_ERR("ICM40627 not ready!");
        return -ENODEV;
    }

    return 0;
}
