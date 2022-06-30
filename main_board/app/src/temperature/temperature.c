#include "temperature.h"
#include "fan/fan.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys_clock.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(temperature);

static bool send_temperature_messages = false;

#define MAIN_BOARD_OVERTEMP_C 65
#define FRONT_UNIT_OVERTEMP_C 90
#define OVERTEMP_DROP_C       5 // drop in temperature needed to stop over-temp mode

static_assert((int)(MAIN_BOARD_OVERTEMP_C - OVERTEMP_DROP_C) > 0 &&
                  (int)(FRONT_UNIT_OVERTEMP_C - OVERTEMP_DROP_C) > 0,
              "Unsigned integer will underflow");

struct sensor_and_channel; // forward declaration

typedef void (*temperature_callback)(
    struct sensor_and_channel *sensor_and_channel, uint32_t temperature);
static void
overtemp_callback(struct sensor_and_channel *sensor_and_channel,
                  uint32_t temperature);

struct overtemp_info {
    uint32_t overtemp_c;
    uint32_t overtemp_drop_c;
    bool in_overtemp;
};

struct sensor_and_channel {
    const struct device *sensor;
    const enum sensor_channel channel;
    const Temperature_TemperatureSource temperature_source;
    temperature_callback cb;
    struct overtemp_info *cb_data;
};

static struct sensor_and_channel sensors_and_channels[] = {
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_FRONT_UNIT,
     .cb = overtemp_callback,
     .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                        .overtemp_drop_c = OVERTEMP_DROP_C,
                                        .in_overtemp = false}},

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_MAIN_BOARD,
     .cb = overtemp_callback,
     .cb_data = &(struct overtemp_info){.overtemp_c = MAIN_BOARD_OVERTEMP_C,
                                        .overtemp_drop_c = OVERTEMP_DROP_C,
                                        .in_overtemp = false}},
#endif

    {.sensor = DEVICE_DT_GET(DT_PATH(stm_tmp)),
     .channel = SENSOR_CHAN_DIE_TEMP,
     .temperature_source = Temperature_TemperatureSource_MAIN_MCU,
     .cb = NULL,
     .cb_data = NULL},
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_LIQUID_LENS,
     .cb = NULL,
     .cb_data = NULL}};

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_TEMPERATURE);
static struct k_thread thread_data;
static k_tid_t thread_id = NULL;

static volatile k_timeout_t global_sample_period;

void
temperature_set_sampling_period_ms(uint32_t sample_period)
{
    global_sample_period = K_MSEC(sample_period);
    k_wakeup(thread_id);
}

static ret_code_t
get_ambient_temperature(const struct device *dev, int32_t *temp,
                        enum sensor_channel channel)
{
    int ret;
    struct sensor_value temp_value;
    ret = sensor_sample_fetch(dev);
    if (ret) {
        LOG_ERR("Error fetching sensor sample from %s!", dev->name);
        return RET_ERROR_INTERNAL;
    }
    ret = sensor_channel_get(dev, channel, &temp_value);
    if (ret) {
        LOG_ERR("Error getting ambient temperature from %s (%d)!", dev->name,
                ret);
        return RET_ERROR_INTERNAL;
    }
    LOG_DBG("%s: %uC", dev->name, temp_value.val1);
    *temp = temp_value.val1;

    return RET_SUCCESS;
}

static void
temperature_report_internal(struct sensor_and_channel *sensor_and_channel,
                            int32_t temperature_in_c)
{
    temperature_report(sensor_and_channel->temperature_source,
                       temperature_in_c);

    if (sensor_and_channel->cb) {
        sensor_and_channel->cb(sensor_and_channel, temperature_in_c);
    }
}

void
temperature_report(Temperature_TemperatureSource source,
                   int32_t temperature_in_c)
{
    if (send_temperature_messages) {
        Temperature temperature = {.source = source,
                                   .temperature_c = temperature_in_c};
        int err_code = publish_new(&temperature, sizeof(temperature),
                                   McuToJetson_temperature_tag,
                                   CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        ASSERT_SOFT(err_code);
    }
}

static void
sample_and_report_temperature(struct sensor_and_channel *sensor_and_channel)
{
    int ret;
    int32_t temp;

    ret = get_ambient_temperature(sensor_and_channel->sensor, &temp,
                                  sensor_and_channel->channel);
    if (ret == RET_SUCCESS) {
        temperature_report_internal(sensor_and_channel, temp);
    }
}

static void
temperature_thread()
{
    for (;;) {
        k_sleep(global_sample_period);

        for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
            if (device_is_ready(sensors_and_channels[i].sensor)) {
                sample_and_report_temperature(&sensors_and_channels[i]);
            }
        }
    }
}

static int
check_ready(void)
{
    int ret = RET_SUCCESS;

    for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
        if (!device_is_ready(sensors_and_channels[i].sensor)) {
            LOG_ERR("Could not initialize temperature sensor '%s'",
                    sensors_and_channels[i].sensor->name);
            ret = RET_ERROR_INVALID_STATE;
        } else {
            LOG_INF("Initialized %s", sensors_and_channels[i].sensor->name);
        }
    }
    return ret;
}

void
temperature_start_sending(void)
{
    LOG_INF("Starting to send temperatures to Jetson");
    send_temperature_messages = true;
}

void
temperature_init(void)
{
    check_ready();
    global_sample_period = K_SECONDS(1);

    if (thread_id == NULL) {
        thread_id = k_thread_create(&thread_data, stack_area,
                                    K_THREAD_STACK_SIZEOF(stack_area),
                                    temperature_thread, NULL, NULL, NULL,
                                    THREAD_PRIORITY_TEMPERATURE, 0, K_NO_WAIT);
    } else {
        LOG_ERR("Sampling already started");
    }
}

// ****************************
// * Overtemperature Handling *
// ****************************

// Theory of operation:
//
// Overtemperature conditions are optionally defined per temperature source and
// are checked at every temperature sampling. One provides a threshold in
// Celsius over which the overtemperature response is activated. Additionally,
// one provides a temperature drop which indicates how far a temperature
// source's temperature must drop from its overtemperature threshold before we
// consider the temperature nominal and the overtemperature condition resolved.
// The current overtemperature response is to command the fan(s) to run at 100%
// power. We activate and stay in the overtemperature response as long as at
// least one temperature source has reached its overtemperature condition.

static uint8_t num_sensors_in_overtemp_conditions = 0;
static uint8_t old_num_sensors_in_overtemp_conditions = 0;

bool
temperature_is_in_overtemp(void)
{
    return num_sensors_in_overtemp_conditions > 0;
}

static void
check_overtemp_conditions(void)
{
    static uint8_t fan_speed_before_overtemperature = 0;

    if (old_num_sensors_in_overtemp_conditions == 1 &&
        num_sensors_in_overtemp_conditions == 0) {
        LOG_INF("All overtemp conditions have abated -- restoring fan to old "
                "value of %u%%",
                fan_speed_before_overtemperature);
        fan_set_speed(fan_speed_before_overtemperature);
    } else if (old_num_sensors_in_overtemp_conditions == 0 &&
               num_sensors_in_overtemp_conditions > 0) {
        LOG_WRN("Overtemperature condition detected -- setting fan to 100%% "
                "until condition abates");
        fan_speed_before_overtemperature = fan_get_speed();
        fan_set_speed(100);
    }
}

static void
inc_overtemp_condition(void)
{
    old_num_sensors_in_overtemp_conditions = num_sensors_in_overtemp_conditions;
    num_sensors_in_overtemp_conditions++;
    check_overtemp_conditions();
}

static void
dec_overtemp_condition(void)
{
    old_num_sensors_in_overtemp_conditions = num_sensors_in_overtemp_conditions;
    num_sensors_in_overtemp_conditions--;
    check_overtemp_conditions();
}

static void
overtemp_callback(struct sensor_and_channel *sensor_and_channel,
                  uint32_t temperature)
{
    struct overtemp_info *overtemp_info =
        (struct overtemp_info *)sensor_and_channel->cb_data;

    if (overtemp_info == NULL) {
        LOG_ERR("Over-temperature callback called without data");
        return;
    }

    if (!overtemp_info->in_overtemp &&
        temperature > overtemp_info->overtemp_c) {
        LOG_WRN("Over-temperature alert -- %s temperature has "
                "exceeded %u°C",
                sensor_and_channel->sensor->name, overtemp_info->overtemp_c);
        overtemp_info->in_overtemp = true;
        inc_overtemp_condition();
    } else if (overtemp_info->in_overtemp &&
               temperature < (overtemp_info->overtemp_c -
                              overtemp_info->overtemp_drop_c)) {
        LOG_INF("Over-temperature alert -- %s temperature has decreased to "
                "safe value of %u°C",
                sensor_and_channel->sensor->name, temperature);
        overtemp_info->in_overtemp = false;
        dec_overtemp_condition();
    }
}
