#include "tof_1d.h"
#include "app_config.h"
#include "mcu.pb.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"
#include <errors.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(1d_tof, CONFIG_TOF_1D_LOG_LEVEL);

const struct device *tof_1d_device = DEVICE_DT_GET(DT_NODELABEL(tof_sensor));

K_THREAD_STACK_DEFINE(stack_area_tof_1d, THREAD_STACK_SIZE_1DTOF);
static struct k_thread tof_1d_thread_data;

static atomic_t too_close_counter = ATOMIC_INIT(0);
#define TOO_CLOSE_THRESHOLD 3

#define INTER_MEASUREMENT_FREQ_HZ   ((int32_t)3)
#define INTER_MEASUREMENT_PERIOD_MS ((int32_t)1000 / INTER_MEASUREMENT_FREQ_HZ)
#define FETCH_PERIOD_MS             ((int32_t)(INTER_MEASUREMENT_PERIOD_MS * 1.5))
#define DISTANCE_PUBLISH_PERIOD_MS  (FETCH_PERIOD_MS * 2)

BUILD_ASSERT(
    DISTANCE_PUBLISH_PERIOD_MS % FETCH_PERIOD_MS == 0,
    "DISTANCE_PUBLISH_PERIOD_MS must be a multiple of FETCH_PERIOD_MS");
BUILD_ASSERT(INTER_MEASUREMENT_FREQ_HZ > 0,
             "INTER_MEASUREMENT_FREQ_HZ must be greater than 0");

static void (*unsafe_cb)(void) = NULL;

/**
 * Mutex for I2C communication
 * Because the bus is shared between the vl53l1 `tof_sensor` sensor and the
 * pca95xx gpio expander (`gpio_exp_front_unit`) which is itself used by the
 * i2c-mux-gpio-front-unit (i2c multiplexer for als and front unit
 * temperature sensors), we need to lock the bus when accessing the sensor.
 * Otherwise, concurrent access to the i2c bus can cause a bus error.
 * ARBITRATION LOST (ARLO) error have been observed, followed by a NACK.
 */
static struct k_mutex *i2c_mutex = NULL;

bool
distance_is_safe(void)
{
    long counter = atomic_get(&too_close_counter);
    bool is_safe = counter < TOO_CLOSE_THRESHOLD;
    return is_safe;
}

_Noreturn void
tof_1d_thread()
{
    int ret;
    struct sensor_value distance_value;
    orb_mcu_main_ToF_1D tof;
    uint32_t count = 0;

    uint32_t tick = 0;
    while (1) {
        uint32_t tock = k_uptime_get_32();
        uint32_t task_duration = 0;
        if (tick != 0) {
            task_duration = tock - tick;
        }
        LOG_DBG("task duration: %d", task_duration);

        k_msleep(FETCH_PERIOD_MS - task_duration);

        tick = k_uptime_get_32();

        if (i2c_mutex) {
            k_mutex_lock(i2c_mutex, K_FOREVER);
        }
        ret = sensor_sample_fetch_chan(tof_1d_device, SENSOR_CHAN_ALL);
        if (i2c_mutex) {
            k_mutex_unlock(i2c_mutex);
        }
        if (ret != 0) {
            LOG_WRN("Error fetching %d", ret);
            continue;
        }

        ret = sensor_channel_get(tof_1d_device, SENSOR_CHAN_DISTANCE,
                                 &distance_value);
        if (ret != 0) {
            // print error with debug level because the range status
            // can quickly throw an error when nothing in front of the sensor
            LOG_DBG("Error getting distance data %d", ret);
            continue;
        }

        tof.distance_mm = distance_value.val1;

        // limit number of samples sent
        ++count;
        if (count % (DISTANCE_PUBLISH_PERIOD_MS / FETCH_PERIOD_MS) == 0) {
            LOG_INF("Distance in front: %umm", tof.distance_mm);

            publish_new(&tof, sizeof(tof), orb_mcu_main_McuToJetson_tof_1d_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }

        // check proximity from sensor itself
        memset(&distance_value, 0, sizeof(distance_value));
        ret = sensor_channel_get(tof_1d_device, SENSOR_CHAN_PROX,
                                 &distance_value);
        if (ret != 0) {
            LOG_DBG("Error getting prox data %d", ret);
            continue;
        }

        CRITICAL_SECTION_ENTER(k);
        long counter = atomic_get(&too_close_counter);
        // if val1 is 0, we are far away, so we can decrease the counter
        // see SENSOR_CHAN_PROX documentation
        if (distance_value.val1 == 0) {
            if (counter > 0) {
                counter--;
            }
        } else if (counter < TOO_CLOSE_THRESHOLD) {
            counter++;
        }
        atomic_set(&too_close_counter, counter);
        CRITICAL_SECTION_EXIT(k);

        if (unsafe_cb && !distance_is_safe()) {
            unsafe_cb();
        }
    }
}

int
tof_1d_init(void (*distance_unsafe_cb)(void), struct k_mutex *mutex)
{
    if (!device_is_ready(tof_1d_device)) {
        LOG_ERR("VL53L1 not ready!");
        return RET_ERROR_INVALID_STATE;
    }

    if (mutex) {
        i2c_mutex = mutex;
    }

    k_thread_create(&tof_1d_thread_data, stack_area_tof_1d,
                    K_THREAD_STACK_SIZEOF(stack_area_tof_1d),
                    (k_thread_entry_t)tof_1d_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_1DTOF, 0, K_NO_WAIT);
    k_thread_name_set(&tof_1d_thread_data, "tof_1d");

    // set short distance mode
    struct sensor_value distance_config = {.val1 = 1 /* short */, .val2 = 0};
    if (i2c_mutex) {
        k_mutex_lock(i2c_mutex, K_FOREVER);
    }
    sensor_attr_set(tof_1d_device, SENSOR_CHAN_DISTANCE,
                    SENSOR_ATTR_CONFIGURATION, &distance_config);
    if (i2c_mutex) {
        k_mutex_unlock(i2c_mutex);
    }

    if (distance_unsafe_cb) {
        unsafe_cb = distance_unsafe_cb;
    }

    // set to autonomous mode by setting sampling frequency / inter measurement
    // period
    // the driver doesn't allow for sampling frequency below 1Hz
    distance_config.val1 = INTER_MEASUREMENT_FREQ_HZ;
    distance_config.val2 = 0; // fractional part, not used
    if (i2c_mutex) {
        k_mutex_lock(i2c_mutex, K_FOREVER);
    }
    sensor_attr_set(tof_1d_device, SENSOR_CHAN_DISTANCE,
                    SENSOR_ATTR_SAMPLING_FREQUENCY, &distance_config);
    if (i2c_mutex) {
        k_mutex_unlock(i2c_mutex);
    }

    return RET_SUCCESS;
}
