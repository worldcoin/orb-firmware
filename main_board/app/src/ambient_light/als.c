#include "als.h"
#include "app_config.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <drivers/sensor.h>
#include <errors.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(als);

const struct device *als_device = DEVICE_DT_GET(DT_NODELABEL(front_unit_als));

void
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
        if (ret != 0) {
            LOG_WRN("Error getting data %d", ret);
            continue;
        }

        als.ambient_light_lux = als_value.val1;
        LOG_INF("Ambient light: %u.%06u", als_value.val1, als_value.val2);

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

K_THREAD_DEFINE(als, THREAD_STACK_SIZE_1DTOF, als_thread, NULL, NULL, NULL,
                THREAD_PRIORITY_1DTOF, 0, 0);
