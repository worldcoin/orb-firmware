#include "als.h"
#include "app_config.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <errors.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(als);

const struct device *als_device = DEVICE_DT_GET(DT_NODELABEL(front_unit_als));

static void
als_thread()
{
    int ret;
    struct sensor_value als_value;
    AmbientLight als;

    while (1) {
        k_msleep(1000);

        ret = sensor_sample_fetch_chan(als_device, SENSOR_CHAN_LIGHT);
        if (ret != 0) {
            LOG_WRN("Error fetching %d", ret);
            continue;
        }
        ret = sensor_channel_get(als_device, SENSOR_CHAN_LIGHT, &als_value);
        if (ret == -ERANGE) {
            als_value.val1 = 0;
            als.flag = AmbientLight_Flags_ALS_ERR_RANGE;
        } else if (ret != 0) {
            LOG_WRN("Error getting data %d", ret);
            continue;
        } else {
            als.ambient_light_lux = als_value.val1;
            als.flag = AmbientLight_Flags_ALS_OK;
        }

        LOG_INF("Ambient light: %s %u.%06u",
                als.flag == AmbientLight_Flags_ALS_ERR_RANGE ? "out of range"
                                                             : "",
                als_value.val1, als_value.val2);

        publish_new(&als, sizeof(als), McuToJetson_front_als_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    }
}

int
als_init(void)
{
    if (!device_is_ready(als_device)) {
        LOG_ERR("Ambient Light Sensor not ready!");
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

K_THREAD_DEFINE(als, THREAD_STACK_SIZE_ALS, als_thread, NULL, NULL, NULL,
                THREAD_PRIORITY_ALS, 0, 0);
