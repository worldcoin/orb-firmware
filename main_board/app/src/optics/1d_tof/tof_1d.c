#include "tof_1d.h"
#include "app_config.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <errors.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(1d_tof, CONFIG_TOF_1D_LOG_LEVEL);

const struct device *tof_1d_device = DEVICE_DT_GET(DT_NODELABEL(tof_sensor));

K_THREAD_STACK_DEFINE(stack_area_tof_1d, THREAD_STACK_SIZE_1DTOF);
static struct k_thread tof_1d_thread_data;

struct distance_history {
    uint32_t buffer[4];
    size_t wr_idx;
    size_t rd_idx;
};

static struct distance_history history = {0};
static bool is_safe = false;
#define DISTANCE_MIN_EYE_SAFETY_LOWER_LIMIT_MM 100
#define DISTANCE_MIN_EYE_SAFETY_UPPER_LIMIT_MM 120

#define DISTANCE_PUBLISH_PERIOD_MS (1000)

bool
distance_is_safe(void)
{
    return is_safe;
}

void
tof_1d_thread()
{
    int ret;
    struct sensor_value distance_value;
    ToF_1D tof;
    uint32_t count = 0;

    while (1) {
        k_msleep(CONFIG_VL53L1X_INTER_MEASUREMENT_PERIOD);

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

            // invalid value, reset history
            history.rd_idx = 0;
            history.wr_idx = 0;
            continue;
        }

        tof.distance_mm =
            distance_value.val1 * 1000 + distance_value.val2 / 1000;

        // limit number of samples sent
        ++count;
        if (count % (DISTANCE_PUBLISH_PERIOD_MS /
                     CONFIG_VL53L1X_INTER_MEASUREMENT_PERIOD) ==
            0) {
            LOG_INF("Distance in front: %umm", tof.distance_mm);

            publish_new(&tof, sizeof(tof), McuToJetson_tof_1d_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }

        history.buffer[history.wr_idx] = tof.distance_mm;
        history.wr_idx = (history.wr_idx + 1) % ARRAY_SIZE(history.buffer);
        // check if history is full meaning we have a row of valid values
        // we can use
        if (history.wr_idx == history.rd_idx) {
            // compute average
            uint32_t average = 0;
            for (size_t i = 0; i < ARRAY_SIZE(history.buffer); ++i) {
                average += history.buffer[i] / ARRAY_SIZE(history.buffer);
            }

            if (average < DISTANCE_MIN_EYE_SAFETY_LOWER_LIMIT_MM && is_safe) {
                is_safe = false;
                LOG_WRN("IR LEDs are unsafe to use: %umm", average);
            }

            if (average > DISTANCE_MIN_EYE_SAFETY_UPPER_LIMIT_MM && !is_safe) {
                is_safe = true;
                LOG_WRN("IR LEDs are safe to use: %umm", average);
            }

            history.rd_idx = (history.rd_idx + 1) % ARRAY_SIZE(history.buffer);
        }
    }
}

int
tof_1d_init(void)
{
    if (!device_is_ready(tof_1d_device)) {
        LOG_ERR("VL53L1 not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_thread_create(&tof_1d_thread_data, stack_area_tof_1d,
                    K_THREAD_STACK_SIZEOF(stack_area_tof_1d), tof_1d_thread,
                    NULL, NULL, NULL, THREAD_PRIORITY_1DTOF, 0, K_NO_WAIT);

    return RET_SUCCESS;
}
