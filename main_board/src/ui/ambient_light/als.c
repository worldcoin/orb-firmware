#include "als.h"
#include "app_config.h"
#include "mcu.pb.h"
#include "orb_logs.h"
#include "orb_state.h"
#include "pubsub/pubsub.h"
#include <errors.h>
#include <ui/rgb_leds/front_leds/front_leds.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(als, CONFIG_ALS_LOG_LEVEL);
ORB_STATE_REGISTER(als);

const struct device *als_device =
    DEVICE_DT_GET_OR_NULL(DT_NODELABEL(front_unit_als));

K_THREAD_STACK_DEFINE(stack_area_als, THREAD_STACK_SIZE_ALS);
static struct k_thread als_thread_data;

static struct k_mutex *als_i2c_mux_mutex;

#define ERROR_STATE_COUNT 3

static void
als_thread()
{
    int ret;
    struct sensor_value als_value;
    orb_mcu_main_AmbientLight als;
    size_t error_count = 0;

    while (1) {
        k_msleep(1000);

        if (k_mutex_lock(als_i2c_mux_mutex, K_MSEC(100)) == 0) {
            als.flag = orb_mcu_main_AmbientLight_Flags_ALS_OK;

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
            // on Diamond EVT, ALS sensor is located on the front unit, close
            // to the front LEDs and interferes with the ALS readings.
            // Make sure to mark the ALS reading as invalid if the front LEDs
            // are on.
            if (front_leds_is_shroud_on()) {
                als.flag =
                    orb_mcu_main_AmbientLight_Flags_ALS_ERR_LEDS_INTERFERENCE;
            }
#endif

            ret = sensor_sample_fetch_chan(als_device, SENSOR_CHAN_LIGHT);
            k_mutex_unlock(als_i2c_mux_mutex);
            if (ret != 0) {
                LOG_WRN("Error fetching %d", ret);
                if (++error_count > ERROR_STATE_COUNT) {
                    ORB_STATE_SET_CURRENT(RET_ERROR_INTERNAL,
                                          "sensor fetch: ret: %d", ret);
                }
                continue;
            }
            ret = sensor_channel_get(als_device, SENSOR_CHAN_LIGHT, &als_value);
            if (ret == -ERANGE) {
                als_value.val1 = 0;
                als.flag = orb_mcu_main_AmbientLight_Flags_ALS_ERR_RANGE;
            } else if (ret != 0) {
                LOG_WRN("Error getting data %d", ret);
                if (++error_count > ERROR_STATE_COUNT) {
                    ORB_STATE_SET_CURRENT(RET_ERROR_INTERNAL,
                                          "sensor get: ret: %d", ret);
                }
                error_count++;
                continue;
            } else {
                als.ambient_light_lux = als_value.val1;
            }

            LOG_INF("Ambient light: %s %u.%06u lux",
                    als.flag == orb_mcu_main_AmbientLight_Flags_ALS_ERR_RANGE
                        ? "out of range"
                        : "",
                    als_value.val1, als_value.val2);

            publish_new(&als, sizeof(als),
                        orb_mcu_main_McuToJetson_front_als_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

            // reset error counter and state if we had errors
            if (error_count > 0) {
                ORB_STATE_SET_CURRENT(RET_SUCCESS);
                error_count = 0;
            }
        } else {
            LOG_ERR("Could not lock mutex.");
        }
    }
}

int
als_init(const orb_mcu_Hardware *hw_version, struct k_mutex *i2c_mux_mutex)
{
    als_i2c_mux_mutex = i2c_mux_mutex;

    // skip if als_device is not defined, or
    // on front unit 6.3A to 6.3C as the ALS is not assembled
    if (als_device == NULL ||
        (hw_version->front_unit >=
             orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3A &&
         hw_version->front_unit <=
             orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3C)) {
        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_SUPPORTED,
                              "no als on that front pcb v: %u",
                              hw_version->front_unit);
        return RET_SUCCESS;
    }

    if (!device_is_ready(als_device)) {
        LOG_ERR("ALS not ready");
        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED,
                              "als not ready (driver init failed?)");
        return RET_ERROR_INTERNAL;
    } else {
        ORB_STATE_SET_CURRENT(RET_SUCCESS);
    }

    k_thread_create(&als_thread_data, stack_area_als,
                    K_THREAD_STACK_SIZEOF(stack_area_als),
                    (k_thread_entry_t)als_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_ALS, 0, K_NO_WAIT);
    k_thread_name_set(&als_thread_data, "als");

    return RET_SUCCESS;
}
