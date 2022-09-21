#include "tof_1d.h"
#include "app_config.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <device.h>
#include <drivers/sensor.h>
#include <errors.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(1d_tof);

const struct device *tof_1d_device = DEVICE_DT_GET(DT_NODELABEL(tof_sensor));

void
tof_1d_thread()
{
    int ret;
    struct sensor_value distance_value;
    ToF_1D tof;

    while (1) {
        k_msleep(1000);

        ret = sensor_sample_fetch_chan(tof_1d_device, SENSOR_CHAN_DISTANCE);
        if (ret != 0) {
            LOG_WRN("Error fetching %d", ret);
            continue;
        }
        ret = sensor_channel_get(tof_1d_device, SENSOR_CHAN_DISTANCE,
                                 &distance_value);
        if (ret != 0) {
            // print error with debug level because the range status
            // can quickly throw an error when nothing in front of the sensor
            LOG_DBG("Error getting data %d", ret);
            continue;
        }

        tof.distance_mm =
            distance_value.val1 * 1000 + distance_value.val2 / 1000;

        LOG_INF("Distance in front: %umm", tof.distance_mm);

        publish_new(&tof, sizeof(tof), McuToJetson_tof_1d_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    }
}

int
tof_1d_init(void)
{
    if (!device_is_ready(tof_1d_device)) {
        LOG_ERR("VL53L1 not ready!");
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

K_THREAD_DEFINE(tof_1d, THREAD_STACK_SIZE_1DTOF, tof_1d_thread, NULL, NULL,
                NULL, THREAD_PRIORITY_1DTOF, 0, 0);
