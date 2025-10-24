#include "tof_1d.h"
#include "app_config.h"
#include "mcu.pb.h"
#include "orb_state.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <errors.h>
#include <string.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "orb_logs.h"

/**
 * 1D ToF sensor module
 *
 * On Diamond main board, the 1D ToF is only checked if ready: driver is
 * initialized, but completely disabled to prevent IR light leakage that
 * interferes with the RGB/IR face camera (into RGB frames).
 */

LOG_MODULE_REGISTER(tof_1d, CONFIG_TOF_1D_LOG_LEVEL);

ORB_STATE_REGISTER(tof_1d);
#define ORB_STATE_1D_TOF_THRESHOLD_ERROR_COUNT 3

const struct device *tof_1d_device = DEVICE_DT_GET(DT_NODELABEL(tof_sensor));

#ifdef CONFIG_BOARD_DIAMOND_MAIN
const struct device *tof_1d_device_dvt =
    DEVICE_DT_GET(DT_NODELABEL(tof_sensor_dvt));

#else

K_THREAD_STACK_DEFINE(stack_area_tof_1d, THREAD_STACK_SIZE_1DTOF);
static struct k_thread tof_1d_thread_data;
#endif

#if CONFIG_PROXIMITY_DETECTION_FOR_IR_SAFETY
static atomic_t too_close_counter = ATOMIC_INIT(0);
#define TOO_CLOSE_THRESHOLD 3
#endif

#define INTER_MEASUREMENT_FREQ_HZ   ((int32_t)3)
#define INTER_MEASUREMENT_PERIOD_MS ((int32_t)1000 / INTER_MEASUREMENT_FREQ_HZ)
#define FETCH_PERIOD_MS                                                        \
    ((int32_t)(INTER_MEASUREMENT_PERIOD_MS + INTER_MEASUREMENT_PERIOD_MS / 2))
#define DISTANCE_PUBLISH_PERIOD_MS (FETCH_PERIOD_MS * 2)

BUILD_ASSERT(
    DISTANCE_PUBLISH_PERIOD_MS % FETCH_PERIOD_MS == 0,
    "DISTANCE_PUBLISH_PERIOD_MS must be a multiple of FETCH_PERIOD_MS");
BUILD_ASSERT(INTER_MEASUREMENT_FREQ_HZ > 0,
             "INTER_MEASUREMENT_FREQ_HZ must be greater than 0");

#if CONFIG_PROXIMITY_DETECTION_FOR_IR_SAFETY
static void (*unsafe_event_cb)(void) = NULL;
#endif
/**
 * Mutex for I2C communication
 * Because the bus is shared between the vl53l1 `tof_sensor` sensor and the
 * pca95xx gpio expander (`gpio_exp_front_unit`) which is itself used by the
 * i2c-mux-gpio-front-unit (i2c multiplexer for als and front unit
 * temperature sensors), we need to lock the bus when accessing the sensor.
 * Otherwise, concurrent access to the i2c bus can cause a bus error.
 * ARBITRATION LOST (ARLO) error have been observed, followed by a NACK.
 */
static struct k_mutex *i2c1_mutex = NULL;

bool
distance_is_safe(void)
{
#ifdef CONFIG_PROXIMITY_DETECTION_FOR_IR_SAFETY
    long counter = atomic_get(&too_close_counter);
    bool is_safe = counter < TOO_CLOSE_THRESHOLD;
    return is_safe;
#else
    return true;
#endif
}

#ifndef CONFIG_BOARD_DIAMOND_MAIN

static ret_code_t
tof_init_config(void)
{
    int ret;
    // set short distance mode
    struct sensor_value distance_config = {.val1 = 1 /* short */, .val2 = 0};
    if (i2c1_mutex) {
        k_mutex_lock(i2c1_mutex, K_FOREVER);
    }
    ret = sensor_attr_set(tof_1d_device, SENSOR_CHAN_DISTANCE,
                          SENSOR_ATTR_CONFIGURATION, &distance_config);
    if (i2c1_mutex) {
        k_mutex_unlock(i2c1_mutex);
    }
    if (ret != 0) {
        return RET_ERROR_INTERNAL;
    }

    // set to autonomous mode by setting sampling frequency / inter measurement
    // period
    // the driver doesn't allow for sampling frequency below 1Hz
    distance_config.val1 = INTER_MEASUREMENT_FREQ_HZ;
    distance_config.val2 = 0; // fractional part, not used
    if (i2c1_mutex) {
        k_mutex_lock(i2c1_mutex, K_FOREVER);
    }
    ret = sensor_attr_set(tof_1d_device, SENSOR_CHAN_DISTANCE,
                          SENSOR_ATTR_SAMPLING_FREQUENCY, &distance_config);
    if (i2c1_mutex) {
        k_mutex_unlock(i2c1_mutex);
    }
    if (ret != 0) {
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

static void
set_states(bool tof_1d_is_error)
{
    /* tof 1d sensor state */
    if (ORB_STATE_GET(tof_1d) == RET_SUCCESS && tof_1d_is_error) {
        ORB_STATE_SET(tof_1d, RET_ERROR_INTERNAL, "error reading");
    } else if (ORB_STATE_GET(tof_1d) != RET_SUCCESS && !tof_1d_is_error) {
        ORB_STATE_SET(tof_1d, RET_SUCCESS);
    }
}

_Noreturn void
tof_1d_thread(void *a, void *b, void *c)
{
    UNUSED_PARAMETER(a);
    UNUSED_PARAMETER(b);
    UNUSED_PARAMETER(c);

    int ret;
    struct sensor_value distance_value;
    orb_mcu_main_ToF_1D tof;
    uint32_t count = 0;
    uint32_t tick = 0;
    uint32_t err_count = 0;

    /* initialize sensor config (short mode, sampling frequency) */
    ret = tof_init_config();
    /* initialize internal states */
    set_states(ret != RET_SUCCESS);

    while (1) {
        uint32_t tock = k_uptime_get_32();
        uint32_t task_duration = 0;
        if (tick != 0) {
            task_duration = tock - tick;
        }
        LOG_DBG("task duration: %u ms", task_duration);

        // `ORB_STATE_1D_TOF_THRESHOLD_ERROR_COUNT` errors in a row for 1d-tof
        // sensor means something is wrong
        set_states(err_count >= ORB_STATE_1D_TOF_THRESHOLD_ERROR_COUNT);

        /* wait until the sensor is back online */
        if (err_count > ORB_STATE_1D_TOF_THRESHOLD_ERROR_COUNT) {
            LOG_WRN("reconfiguring 1d-tof sensor after errors");
            while (tof_init_config() != RET_SUCCESS) {
                k_msleep(5000);
            }
            err_count = 0;
        }

        if (task_duration > FETCH_PERIOD_MS) {
            LOG_WRN("Task took longer (%u ms) than fetch period (%u ms)",
                    task_duration, FETCH_PERIOD_MS);
        } else {
            k_msleep(FETCH_PERIOD_MS - task_duration);
        }

        tick = k_uptime_get_32();

        if (i2c1_mutex) {
            k_mutex_lock(i2c1_mutex, K_FOREVER);
        }
        ret = sensor_sample_fetch_chan(tof_1d_device, SENSOR_CHAN_ALL);
        if (i2c1_mutex) {
            k_mutex_unlock(i2c1_mutex);
        }
        if (ret != 0) {
            LOG_WRN("Error fetching %d", ret);
            err_count++;
            continue;
        }

        if (i2c1_mutex) {
            k_mutex_lock(i2c1_mutex, K_FOREVER);
        }
        ret = sensor_channel_get(tof_1d_device, SENSOR_CHAN_DISTANCE,
                                 &distance_value);
        if (i2c1_mutex) {
            k_mutex_unlock(i2c1_mutex);
        }
        if (ret != 0) {
            // print error with debug level because the range status
            // can quickly throw an error when nothing in front of the sensor
            LOG_DBG("Error getting distance data %d", ret);
            err_count++;
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
        if (i2c1_mutex) {
            k_mutex_lock(i2c1_mutex, K_FOREVER);
        }
        ret = sensor_channel_get(tof_1d_device, SENSOR_CHAN_PROX,
                                 &distance_value);
        if (i2c1_mutex) {
            k_mutex_unlock(i2c1_mutex);
        }
        if (ret != 0) {
            LOG_DBG("Error getting prox data %d", ret);
            err_count++;
            continue;
        }

#if CONFIG_PROXIMITY_DETECTION_FOR_IR_SAFETY
        const long counter_prev = atomic_get(&too_close_counter);
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

        // only call unsafe_cb once on new event
        if (unsafe_event_cb && counter_prev != atomic_get(&too_close_counter) &&
            !distance_is_safe()) {
            unsafe_event_cb();
        }
#endif

        // if reached here, we successfully fetched the data
        set_states(false);
        err_count = 0;
    }
}

#endif // !CONFIG_BOARD_DIAMOND_MAIN

int
tof_1d_init(void (*distance_unsafe_cb)(void), struct k_mutex *mutex,
            const orb_mcu_Hardware *hw_version)
{
    if (mutex) {
        i2c1_mutex = mutex;
    }

#ifdef CONFIG_BOARD_DIAMOND_MAIN
    UNUSED_PARAMETER(distance_unsafe_cb);

    /* on diamond, select correct 1d-tof device from device tree as it differs
     * from evt to dvt and initialize it (deferred, to prevent init failures)
     */
    if (hw_version->version >=
        orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_5) {
        tof_1d_device = tof_1d_device_dvt;
    }

    if (i2c1_mutex) {
        k_mutex_lock(i2c1_mutex, K_FOREVER);
    }
    int ret = device_init(tof_1d_device);
    ASSERT_SOFT(ret);
    if (i2c1_mutex) {
        k_mutex_unlock(i2c1_mutex);
    }
#else
    UNUSED_PARAMETER(hw_version);
#endif

    if (!device_is_ready(tof_1d_device)) {
        LOG_ERR("VL53L1 not ready!");
        ORB_STATE_SET(tof_1d, RET_ERROR_INVALID_STATE, "device not ready");
        return RET_ERROR_INVALID_STATE;
    }

#ifdef CONFIG_BOARD_DIAMOND_MAIN
    /* device is ready and since we don't spawn the thread, set state now */
    ORB_STATE_SET(tof_1d, RET_SUCCESS, "initialized but disabled");
#else

    k_thread_create(&tof_1d_thread_data, stack_area_tof_1d,
                    K_THREAD_STACK_SIZEOF(stack_area_tof_1d),
                    (k_thread_entry_t)tof_1d_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_1DTOF, 0, K_NO_WAIT);
    k_thread_name_set(&tof_1d_thread_data, "tof_1d");

#if CONFIG_PROXIMITY_DETECTION_FOR_IR_SAFETY
    if (distance_unsafe_cb) {
        unsafe_event_cb = distance_unsafe_cb;
    }
#else
    UNUSED(distance_unsafe_cb);
#endif

#endif
    return RET_SUCCESS;
}
