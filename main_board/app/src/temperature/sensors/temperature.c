#include "temperature.h"
#include "logs_can.h"
#include "pubsub/pubsub.h"
#include "system/diag.h"
#include "temperature/fan/fan.h"
#include "voltage_measurement/voltage_measurement.h"
#include <app_assert.h>
#include <app_config.h>
#include <math.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys_clock.h>

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
// number of attempt to sample a valid temperature before giving up
#define TEMPERATURE_SAMPLE_RETRY_COUNT 5

BUILD_ASSERT((int)(MAIN_BOARD_OVERTEMP_C - OVERTEMP_TO_NOMINAL_DROP_C) > 0 &&
                 (int)(FRONT_UNIT_OVERTEMP_C - OVERTEMP_TO_NOMINAL_DROP_C) > 0,
             "Unsigned integer will underflow");

static const uint16_t *cal1_addr =
    (uint16_t *)DT_PROP(DT_INST(0, st_stm32_temp_cal), ts_cal1_addr);
static const int16_t cal1_temp =
    DT_PROP(DT_INST(0, st_stm32_temp_cal), ts_cal1_temp);
static const uint16_t *cal2_addr =
    (uint16_t *)DT_PROP(DT_INST(0, st_stm32_temp_cal), ts_cal2_addr);
static const int16_t cal2_temp =
    DT_PROP(DT_INST(0, st_stm32_temp_cal), ts_cal2_temp);
static const uint16_t cal_vref_mv =
    DT_PROP(DT_INST(0, st_stm32_temp_cal), ts_cal_vrefanalog);

struct sensor_and_channel; // forward declaration

typedef void (*temperature_callback)(
    struct sensor_and_channel *sensor_and_channel);
static void
overtemp_callback(struct sensor_and_channel *sensor_and_channel);

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
    const HardwareDiagnostic_Source hardware_diagnostic_source;
    temperature_callback cb;
    struct overtemp_info *cb_data;
    int32_t history[TEMPERATURE_AVERAGE_SAMPLE_COUNT];
    size_t wr_idx;
    int32_t average;
};

#define TEMPERATURE_SENTINEL_VALUE INT32_MIN

enum temperature_sensors {
    TEMPERATURE_SENSOR_FRONT_UNIT = 0,
    TEMPERATURE_SENSOR_MAIN_BOARD,
    TEMPERATURE_SENSOR_LIQUID_LENS,
    TEMPERATURE_SENSOR_DIE,
    TEMPERATURE_SENSOR_COUNT
};

static struct sensor_and_channel sensors_and_channels[] = {
    [TEMPERATURE_SENSOR_FRONT_UNIT] =
        {.sensor = DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source = Temperature_TemperatureSource_FRONT_UNIT,
         .hardware_diagnostic_source =
             HardwareDiagnostic_Source_TEMPERATURE_SENSORS_FRONT_UNIT,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},

    [TEMPERATURE_SENSOR_MAIN_BOARD] =
        {.sensor = DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source = Temperature_TemperatureSource_MAIN_BOARD,
         .hardware_diagnostic_source =
             HardwareDiagnostic_Source_TEMPERATURE_SENSORS_MAIN_BOARD,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = MAIN_BOARD_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},

    [TEMPERATURE_SENSOR_LIQUID_LENS] =
        {.sensor = DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source = Temperature_TemperatureSource_LIQUID_LENS,
         .hardware_diagnostic_source =
             HardwareDiagnostic_Source_TEMPERATURE_SENSORS_LIQUID_LENS,
         .cb = overtemp_callback,
         .cb_data =
             &(struct overtemp_info){.overtemp_c = LIQUID_LENS_OVERTEMP_C,
                                     .overtemp_drop_c =
                                         OVERTEMP_TO_NOMINAL_DROP_C,
                                     .in_overtemp = false,
                                     .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_DIE] = {
        .sensor = &(struct device){.name = "die_temp"},
        .channel = SENSOR_CHAN_DIE_TEMP,
        .temperature_source = Temperature_TemperatureSource_MAIN_MCU,
        .hardware_diagnostic_source = HardwareDiagnostic_Source_UNKNOWN,
        .cb = overtemp_callback,
        .cb_data = &(struct overtemp_info){.overtemp_c = MCU_DIE_OVERTEMP_C,
                                           .overtemp_drop_c =
                                               OVERTEMP_TO_NOMINAL_DROP_C,
                                           .in_overtemp = false,
                                           .critical_timer = 0},
        .history = {0},
        .wr_idx = 0,
        .average = TEMPERATURE_SENTINEL_VALUE}};

BUILD_ASSERT(TEMPERATURE_SENSOR_COUNT == ARRAY_SIZE(sensors_and_channels),
             "Count must match sensors_and_channels size");

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_TEMPERATURE);
static struct k_thread temperature_thread_data;
static k_tid_t thread_id = NULL;

static volatile k_timeout_t global_sample_period;

static void
init_sensor_and_channel(struct sensor_and_channel *x)
{
    x->wr_idx = 0;
    for (size_t i = 0; i < ARRAY_SIZE(x->history); ++i) {
        x->history[i] = TEMPERATURE_SENTINEL_VALUE;
    }
}

static int16_t
calculate_die_temperature(uint16_t vref_mv, uint16_t ts_data_raw)
{
    int32_t temperature_degrees = ((int32_t)cal2_temp - (int32_t)cal1_temp) *
                                  ts_data_raw * vref_mv / cal_vref_mv /
                                  (*cal2_addr - *cal1_addr);
    temperature_degrees -= ((int32_t)cal2_temp - (int32_t)cal1_temp) *
                           (*cal1_addr) / (*cal2_addr - *cal1_addr);
    temperature_degrees += 30;
    return (int16_t)temperature_degrees;
}

static ret_code_t
get_die_temperature_degree(struct sensor_value *value)
{
    uint16_t vref_mv = voltage_measurement_get_vref_mv();
    uint16_t ts_data_raw = 0;

    int ret = voltage_measurement_get_raw(CHANNEL_DIE_TEMP, &ts_data_raw);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    value->val1 = calculate_die_temperature(vref_mv, ts_data_raw);
    value->val2 = 0;

    return ret;
}

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

    if (channel == SENSOR_CHAN_DIE_TEMP) {
        // die temperature is not a sensor, but a voltage measurement
        // made by our own module
        ret = get_die_temperature_degree(&temp_value);
        if (ret) {
            return ret;
        }
    } else {
        if (device_is_ready(dev) == false) {
            return RET_ERROR_INTERNAL;
        }
        ret = sensor_sample_fetch(dev);
        if (ret) {
            LOG_ERR("Error fetching %s: %d", dev->name, ret);
            return RET_ERROR_INTERNAL;
        }
        ret = sensor_channel_get(dev, channel, &temp_value);
        if (ret) {
            LOG_ERR("Error getting %s: %d", dev->name, ret);
            return RET_ERROR_INTERNAL;
        }
    }

    float temp_float =
        (float)temp_value.val1 + (float)temp_value.val2 / 1000000.0f;
    *temp = (int32_t)roundl(temp_float);

    return RET_SUCCESS;
}

static void
temperature_report_internal(struct sensor_and_channel *sensor_and_channel)
{
    temperature_report(sensor_and_channel->temperature_source,
                       sensor_and_channel->average);

    if (sensor_and_channel->cb) {
        sensor_and_channel->cb(sensor_and_channel);
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

    return (int32_t)lroundl(avg);
}

static void
sample_and_report_temperature(struct sensor_and_channel *sensor_and_channel)
{
    int ret;
    size_t i;

    for (i = 0; i < TEMPERATURE_SAMPLE_RETRY_COUNT; ++i) {
        ret = get_ambient_temperature(
            sensor_and_channel->sensor,
            &sensor_and_channel->history[sensor_and_channel->wr_idx],
            sensor_and_channel->channel);

        if (ret == RET_SUCCESS) {
            // Sometimes the internal temperature sensor gives an erroneous
            // reading. Let's compare the current sample against the last known
            // average
            int32_t current_sample =
                sensor_and_channel->history[sensor_and_channel->wr_idx];

            if (sensor_and_channel->average == TEMPERATURE_SENTINEL_VALUE) {
                // This is the first sample. So instead of comparing against the
                // last value, just check if the reading generally seems in
                // range.
                if (current_sample > -25 && current_sample < 120) {
                    // Seems ok
                    break;
                }
            } else if (abs(current_sample - sensor_and_channel->average) < 8) {
                // Seems ok
                break;
            } else {
                LOG_DBG("'%s' outlier, avg: %" PRId32 ", current: %" PRId32
                        " (°C)",
                        sensor_and_channel->sensor->name,
                        sensor_and_channel->average, current_sample);
            }
        }
    }

    if (i == TEMPERATURE_SAMPLE_RETRY_COUNT) {
        // We failed after many attempts. Reset the history and try again later
        LOG_ERR("Failed to sample '%s', after %d retries!",
                sensor_and_channel->sensor->name,
                TEMPERATURE_SAMPLE_RETRY_COUNT);
        init_sensor_and_channel(sensor_and_channel);
        return;
    }

    sensor_and_channel->wr_idx =
        (sensor_and_channel->wr_idx + 1) % TEMPERATURE_AVERAGE_SAMPLE_COUNT;

    if (sensor_and_channel->wr_idx == 0) {
        sensor_and_channel->average = average(sensor_and_channel->history);
        LOG_DBG("%s: %iC", sensor_and_channel->sensor->name,
                sensor_and_channel->average);
        temperature_report_internal(sensor_and_channel);
    }
}

_Noreturn static void
temperature_thread()
{
    for (;;) {
        k_sleep(global_sample_period);

        for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
            sample_and_report_temperature(&sensors_and_channels[i]);
        }
    }
}

static int
check_ready(void)
{
    int ret = RET_SUCCESS;

    for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
        if (sensors_and_channels[i].channel != SENSOR_CHAN_DIE_TEMP) {
            if (!device_is_ready(sensors_and_channels[i].sensor)) {
                LOG_ERR("Could not initialize temperature sensor '%s'",
                        sensors_and_channels[i].sensor->name);
                diag_set_status(
                    sensors_and_channels[i].hardware_diagnostic_source,
                    HardwareDiagnostic_Status_STATUS_INITIALIZATION_ERROR);
                ret = RET_ERROR_INVALID_STATE;
            } else {
                LOG_INF("Initialized %s", sensors_and_channels[i].sensor->name);
                diag_set_status(
                    sensors_and_channels[i].hardware_diagnostic_source,
                    HardwareDiagnostic_Status_STATUS_OK);
            }
        } else {
            diag_set_status(sensors_and_channels[i].hardware_diagnostic_source,
                            HardwareDiagnostic_Status_STATUS_OK);
            ret = RET_ERROR_INVALID_STATE;
        }
    }
    return ret;
}

void
temperature_init(const Hardware *hw_version)
{
    if (hw_version->version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        sensors_and_channels[TEMPERATURE_SENSOR_LIQUID_LENS].sensor =
            DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor_ev5));
    } else {
        sensors_and_channels[TEMPERATURE_SENSOR_LIQUID_LENS].sensor =
            DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor));
    }

    check_ready();
    global_sample_period = K_MSEC(1000 / TEMPERATURE_AVERAGE_SAMPLE_COUNT);

    for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
        init_sensor_and_channel(&sensors_and_channels[i]);
    }

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
    static uint16_t fan_speed_before_overtemperature = 0;

    if (old_num_sensors_in_overtemp_conditions == 1 &&
        num_sensors_in_overtemp_conditions == 0) {
        // Warning so that it's logged over CAN
        LOG_WRN(
            "Over-temperature conditions have abated, restoring fan to %.2f%%",
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
overtemp_callback(struct sensor_and_channel *sensor_and_channel)
{
    struct overtemp_info *overtemp_info =
        (struct overtemp_info *)sensor_and_channel->cb_data;

    if (overtemp_info == NULL) {
        LOG_ERR("Over-temperature callback called without data");
        return;
    }

    if (sensor_and_channel->average >
        overtemp_info->overtemp_c + OVERTEMP_TO_CRITICAL_RISE_C) {
        overtemp_info->critical_timer +=
            (global_sample_period.ticks * TEMPERATURE_AVERAGE_SAMPLE_COUNT *
             1000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC);

        if (overtemp_info->critical_timer > CRITICAL_TO_SHUTDOWN_DELAY_MS) {
            // critical temperature
            FatalError error = {
                .reason = FatalError_FatalReason_FATAL_CRITICAL_TEMPERATURE,
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
        sensor_and_channel->average > overtemp_info->overtemp_c) {
        LOG_WRN("%s temperature exceeds %u°C", sensor_and_channel->sensor->name,
                overtemp_info->overtemp_c);
        overtemp_info->in_overtemp = true;
        inc_overtemp_condition();
    } else if (overtemp_info->in_overtemp &&
               sensor_and_channel->average < (overtemp_info->overtemp_c -
                                              overtemp_info->overtemp_drop_c)) {
        LOG_INF("Over-temperature alert -- %s temperature has decreased to "
                "safe value of %u°C",
                sensor_and_channel->sensor->name, sensor_and_channel->average);
        overtemp_info->in_overtemp = false;
        dec_overtemp_condition();
    }
}
