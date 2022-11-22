#include "temperature.h"
#include "fan/fan.h"
#include "pubsub/pubsub.h"
#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/sensor.h>
#include <math.h>
#include <sys/reboot.h>
#include <sys_clock.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(temperature, CONFIG_TEMPERATURE_LOG_LEVEL);

// These values are informed by
// https://www.notion.so/PCBA-thermals-96849052d5c24a0bafaedb4363f460b5

/// Emergency temperatures (fan at max speed)
#define MAIN_BOARD_OVERTEMP_C  80
#define FRONT_UNIT_OVERTEMP_C  70
#define MCU_DIE_OVERTEMP_C     65
#define LIQUID_LENS_OVERTEMP_C 80

// drop in temperature needed to stop over-temp mode
#define OVERTEMP_TO_NOMINAL_DROP_C 5
// rise in temperature above over-temp/emergency which shuts down the device
#define OVERTEMP_TO_CRITICAL_RISE_C   5
#define CRITICAL_TO_SHUTDOWN_DELAY_MS 10000

// number of samples used in a temperature measurement
#define TEMPERATURE_AVERAGE_SAMPLE_COUNT 3

static_assert((int)(MAIN_BOARD_OVERTEMP_C - OVERTEMP_TO_NOMINAL_DROP_C) > 0 &&
                  (int)(FRONT_UNIT_OVERTEMP_C - OVERTEMP_TO_NOMINAL_DROP_C) > 0,
              "Unsigned integer will underflow");

struct sensor_and_channel; // forward declaration

typedef void (*temperature_callback)(
    struct sensor_and_channel *sensor_and_channel, int32_t temperature);
static void
overtemp_callback(struct sensor_and_channel *sensor_and_channel,
                  int32_t temperature);

struct overtemp_info {
    int32_t overtemp_c;
    int32_t overtemp_drop_c;
    bool in_overtemp;
    uint32_t critical_timer;
};

struct sensor_and_channel {
    const struct device *sensor;
    const enum sensor_channel channel;
    const Temperature_TemperatureSource temperature_source;
    temperature_callback cb;
    struct overtemp_info *cb_data;
    int32_t history[TEMPERATURE_AVERAGE_SAMPLE_COUNT];
    size_t wr_idx;
};

static struct sensor_and_channel sensors_and_channels[] = {
    {.sensor = DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_FRONT_UNIT,
     .cb = overtemp_callback,
     .cb_data =
         &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                 .overtemp_drop_c = OVERTEMP_TO_NOMINAL_DROP_C,
                                 .in_overtemp = false,
                                 .critical_timer = 0},
     .history = {0},
     .wr_idx = 0},

    {.sensor = DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_MAIN_BOARD,
     .cb = overtemp_callback,
     .cb_data =
         &(struct overtemp_info){.overtemp_c = MAIN_BOARD_OVERTEMP_C,
                                 .overtemp_drop_c = OVERTEMP_TO_NOMINAL_DROP_C,
                                 .in_overtemp = false,
                                 .critical_timer = 0},
     .history = {0},
     .wr_idx = 0},

    {.sensor = DEVICE_DT_GET(DT_PATH(stm_tmp)),
     .channel = SENSOR_CHAN_DIE_TEMP,
     .temperature_source = Temperature_TemperatureSource_MAIN_MCU,
     .cb = overtemp_callback,
     .cb_data =
         &(struct overtemp_info){.overtemp_c = MCU_DIE_OVERTEMP_C,
                                 .overtemp_drop_c = OVERTEMP_TO_NOMINAL_DROP_C,
                                 .in_overtemp = false,
                                 .critical_timer = 0},
     .history = {0},
     .wr_idx = 0},

    {.sensor = DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor)),
     .channel = SENSOR_CHAN_AMBIENT_TEMP,
     .temperature_source = Temperature_TemperatureSource_LIQUID_LENS,
     .cb = overtemp_callback,
     .cb_data =
         &(struct overtemp_info){.overtemp_c = LIQUID_LENS_OVERTEMP_C,
                                 .overtemp_drop_c = OVERTEMP_TO_NOMINAL_DROP_C,
                                 .in_overtemp = false,
                                 .critical_timer = 0},
     .history = {0},
     .wr_idx = 0},
};

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_TEMPERATURE);
static struct k_thread temperature_thread_data;
static k_tid_t thread_id = NULL;

static volatile k_timeout_t global_sample_period;

void
temperature_set_sampling_period_ms(uint32_t sample_period)
{
    global_sample_period =
        K_MSEC(sample_period / TEMPERATURE_AVERAGE_SAMPLE_COUNT);
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
    double temp_float =
        (double)temp_value.val1 + (double)temp_value.val2 / 1000000.0;
    *temp = (int32_t)roundl(temp_float);

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
    Temperature temperature = {.source = source,
                               .temperature_c = temperature_in_c};
    publish_new(&temperature, sizeof(temperature), McuToJetson_temperature_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static int32_t
average(const int32_t *array)
{
    double avg = 0.0;
    for (size_t i = 0; i < TEMPERATURE_AVERAGE_SAMPLE_COUNT; ++i) {
        avg += (double)array[i];
    }
    avg /= (double)TEMPERATURE_AVERAGE_SAMPLE_COUNT;

    return (int32_t)roundl(avg);
}

static void
sample_and_report_temperature(struct sensor_and_channel *sensor_and_channel)
{
    int ret;
    int32_t temp;

    ret = get_ambient_temperature(
        sensor_and_channel->sensor,
        &sensor_and_channel->history[sensor_and_channel->wr_idx],
        sensor_and_channel->channel);
    sensor_and_channel->wr_idx =
        (sensor_and_channel->wr_idx + 1) % TEMPERATURE_AVERAGE_SAMPLE_COUNT;

    if (ret == RET_SUCCESS && sensor_and_channel->wr_idx == 0) {
        temp = average(sensor_and_channel->history);
        LOG_DBG("%s: %iC", sensor_and_channel->sensor->name, temp);
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
temperature_init(void)
{
    check_ready();
    global_sample_period = K_MSEC(1000 / TEMPERATURE_AVERAGE_SAMPLE_COUNT);

    if (thread_id == NULL) {
        thread_id = k_thread_create(&temperature_thread_data, stack_area,
                                    K_THREAD_STACK_SIZEOF(stack_area),
                                    temperature_thread, NULL, NULL, NULL,
                                    THREAD_PRIORITY_TEMPERATURE, 0, K_NO_WAIT);
        k_thread_name_set(thread_id, "temperature");
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
// The current overtemperature response is to command the fan(s) to run at max
// speed. We activate and stay in the overtemperature response as long as at
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
    static uint32_t fan_speed_before_overtemperature = 0;

    if (old_num_sensors_in_overtemp_conditions == 1 &&
        num_sensors_in_overtemp_conditions == 0) {
        // Warning so that it's logged over CAN
        LOG_WRN("All over-temperature conditions have abated, restoring fan "
                "to old value of %.2f%%",
                ((float)fan_speed_before_overtemperature / UINT16_MAX) * 100);

        fan_set_speed_by_value(fan_speed_before_overtemperature);
    } else if (old_num_sensors_in_overtemp_conditions == 0 &&
               num_sensors_in_overtemp_conditions > 0) {
        LOG_WRN("Setting fan in emergency mode");
        fan_speed_before_overtemperature = fan_get_speed_setting();
        fan_set_max_speed();
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
                  int32_t temperature)
{
    struct overtemp_info *overtemp_info =
        (struct overtemp_info *)sensor_and_channel->cb_data;

    if (overtemp_info == NULL) {
        LOG_ERR("Over-temperature callback called without data");
        return;
    }

    if (temperature > overtemp_info->overtemp_c + OVERTEMP_TO_CRITICAL_RISE_C) {
        overtemp_info->critical_timer +=
            (global_sample_period.ticks * TEMPERATURE_AVERAGE_SAMPLE_COUNT *
             1000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC);

        if (overtemp_info->critical_timer > CRITICAL_TO_SHUTDOWN_DELAY_MS) {
            // critical temperature
            FatalError error = {
                .reason = 0,
                .arg = sensor_and_channel->temperature_source,
            };

            // store event
            (void)publish_store(&error, sizeof(error),
                                McuToJetson_fatal_error_tag,
                                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            sys_reboot(0);
        }
    } else {
        overtemp_info->critical_timer = 0;
    }

    if (!overtemp_info->in_overtemp &&
        temperature > overtemp_info->overtemp_c) {
        LOG_WRN("%s temperature exceeds %u°C", sensor_and_channel->sensor->name,
                overtemp_info->overtemp_c);
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
