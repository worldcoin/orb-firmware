#include "als.h"
#include "app_config.h"
#include "mcu.pb.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"
#include "system/diag.h"
#include <errors.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(als, CONFIG_ALS_LOG_LEVEL);

const struct device *als_device =
    DEVICE_DT_GET_OR_NULL(DT_NODELABEL(front_unit_als));

K_THREAD_STACK_DEFINE(stack_area_als, THREAD_STACK_SIZE_ALS);
static struct k_thread als_thread_data;

static struct k_mutex *als_i2c_mux_mutex;

static void
als_thread()
{
    int ret;
    struct sensor_value als_value;
    orb_mcu_main_AmbientLight als;

    while (1) {
        k_msleep(1000);

        if (k_mutex_lock(als_i2c_mux_mutex, K_MSEC(100)) == 0) {
            ret = sensor_sample_fetch_chan(als_device, SENSOR_CHAN_LIGHT);
            k_mutex_unlock(als_i2c_mux_mutex);
            if (ret != 0) {
                LOG_WRN("Error fetching %d", ret);
                continue;
            }
            ret = sensor_channel_get(als_device, SENSOR_CHAN_LIGHT, &als_value);
            if (ret == -ERANGE) {
                als_value.val1 = 0;
                als.flag = orb_mcu_main_AmbientLight_Flags_ALS_ERR_RANGE;
            } else if (ret != 0) {
                LOG_WRN("Error getting data %d", ret);
                continue;
            } else {
                als.ambient_light_lux = als_value.val1;
                als.flag = orb_mcu_main_AmbientLight_Flags_ALS_OK;
            }

            LOG_INF("Ambient light: %s %u.%06u",
                    als.flag == orb_mcu_main_AmbientLight_Flags_ALS_ERR_RANGE
                        ? "out of range"
                        : "",
                    als_value.val1, als_value.val2);

            publish_new(&als, sizeof(als),
                        orb_mcu_main_McuToJetson_front_als_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
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
        diag_set_status(orb_mcu_HardwareDiagnostic_Source_UI_ALS,
                        orb_mcu_HardwareDiagnostic_Status_STATUS_NOT_SUPPORTED);
        return RET_SUCCESS;
    }

    if (!device_is_ready(als_device)) {
        LOG_ERR("ALS not ready");
        diag_set_status(
            orb_mcu_HardwareDiagnostic_Source_UI_ALS,
            orb_mcu_HardwareDiagnostic_Status_STATUS_INITIALIZATION_ERROR);
        return RET_ERROR_INTERNAL;
    } else {
        diag_set_status(orb_mcu_HardwareDiagnostic_Source_UI_ALS,
                        orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    }

    k_thread_create(&als_thread_data, stack_area_als,
                    K_THREAD_STACK_SIZEOF(stack_area_als),
                    (k_thread_entry_t)als_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_ALS, 0, K_NO_WAIT);
    k_thread_name_set(&als_thread_data, "als");

    return RET_SUCCESS;
}
