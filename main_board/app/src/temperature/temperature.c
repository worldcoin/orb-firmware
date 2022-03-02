#include "temperature.h"
#include <app_config.h>
#include <assert.h>
#include <can_messaging.h>
#include <device.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <sys_clock.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(temperature);

struct sensor_and_channel {
    const struct device *sensor;
    enum sensor_channel channel;
    Temperature_TemperatureSource temperature_source;
};

const struct sensor_and_channel sensors_and_channels[] = {
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_FRONT_UNIT},

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_MAIN_BOARD},
#endif

    {.sensor = DEVICE_DT_GET(DT_PATH(stm_tmp)),
     .channel = SENSOR_CHAN_DIE_TEMP,
     .temperature_source = Temperature_TemperatureSource_MAIN_MCU},
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_LIQUID_LENS}};

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_TEMPERATURE);
static struct k_thread thread_data;
static k_tid_t thread_id;

static volatile k_timeout_t global_sample_period;

static McuMessage msg = {
    .which_message = McuMessage_m_message_tag,
    .message.m_message.which_payload = McuToJetson_temperature_tag,
};

void
temperature_set_sampling_period_ms(uint32_t sample_period)
{
    global_sample_period = K_MSEC(sample_period);
    k_wakeup(thread_id);
}

static int
get_ambient_temperature(const struct device *dev, int32_t *temp,
                        enum sensor_channel channel)
{
    int ret;
    struct sensor_value temp_value;
    ret = sensor_sample_fetch(dev);
    if (ret) {
        LOG_ERR("Error fetching sensor sample from %s!", dev->name);
        return 1;
    }
    ret = sensor_channel_get(dev, channel, &temp_value);
    if (ret) {
        LOG_ERR("Error getting ambient temperature from %s (%d)!", dev->name,
                ret);
        return 1;
    }
    LOG_DBG("%s: %uC", dev->name, temp_value.val1);
    *temp = temp_value.val1;

    return 0;
}

static void
sample_and_report_temperature(
    const struct sensor_and_channel *sensor_and_channel)
{
    int ret;
    int32_t temp;

    ret = get_ambient_temperature(sensor_and_channel->sensor, &temp,
                                  sensor_and_channel->channel);
    if (!ret) {
        msg.message.m_message.payload.temperature.source =
            sensor_and_channel->temperature_source;
        msg.message.m_message.payload.temperature.temperature_c = temp;
        can_messaging_push_tx(&msg);
    }
}

static void
thread_entry_point(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    for (;;) {
        k_sleep(global_sample_period);

        for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
            if (device_is_ready(sensors_and_channels[i].sensor)) {
                sample_and_report_temperature(&sensors_and_channels[i]);
            }
        }
    }
}

static void
check_ready(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
        if (!device_is_ready(sensors_and_channels[i].sensor)) {
            LOG_ERR("Could not initialize temperature sensor '%s'",
                    sensors_and_channels[i].sensor->name);
        } else {
            LOG_INF("initialized %s", sensors_and_channels[i].sensor->name);
        }
    }
}

void
temperature_init(void)
{
    check_ready();

    global_sample_period = K_SECONDS(1);

    thread_id = k_thread_create(&thread_data, stack_area,
                                K_THREAD_STACK_SIZEOF(stack_area),
                                thread_entry_point, NULL, NULL, NULL,
                                THREAD_PRIORITY_TEMPERATURE, 0, K_NO_WAIT);
}
