#include "temperature.h"
#include "orb_logs.h"
#include "orb_state.h"
#include "pubsub/pubsub.h"
#include "system/diag.h"
#include "temperature/fan/fan.h"
#include "voltage_measurement/voltage_measurement.h"
#include <app_assert.h>
#include <app_config.h>
#include <math.h>
#include <stdlib.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys_clock.h>

#if defined(CONFIG_MEMFAULT)
#include <memfault/core/reboot_tracking.h>
#endif

LOG_MODULE_REGISTER(temperature, CONFIG_TEMPERATURE_LOG_LEVEL);
ORB_STATE_REGISTER_MULTIPLE(tmp_front, tmp_main, tmp_lens);

// These values are informed by
// https://www.notion.so/PCBA-thermals-96849052d5c24a0bafaedb4363f460b5

/// Emergency temperatures (fan at max speed)
#define MAIN_BOARD_OVERTEMP_C  80
#define LIQUID_LENS_OVERTEMP_C 80

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define FRONT_UNIT_OVERTEMP_C 70
#define MCU_DIE_OVERTEMP_C    65
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define FRONT_UNIT_OVERTEMP_C 80
#define MCU_DIE_OVERTEMP_C    80
#endif

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

// use `die_temp` node to fetch calibration values, even though the driver
// is not used, as we use directly the adc driver.
static const uint16_t *cal1_addr =
    (uint16_t *)DT_PROP(DT_NODELABEL(die_temp), ts_cal1_addr);
static const int16_t cal1_temp = DT_PROP(DT_NODELABEL(die_temp), ts_cal1_temp);
static const uint16_t *cal2_addr =
    (uint16_t *)DT_PROP(DT_NODELABEL(die_temp), ts_cal2_addr);
static const int16_t cal2_temp = DT_PROP(DT_NODELABEL(die_temp), ts_cal2_temp);
static const uint16_t cal_vref_mv =
    DT_PROP(DT_NODELABEL(die_temp), ts_cal_vrefanalog);

struct sensor_and_channel; // forward declaration

typedef int (*temperature_callback)(
    struct sensor_and_channel *sensor_and_channel);
static int
overtemp_callback(struct sensor_and_channel *sensor_and_channel);

static struct k_mutex *temperature_i2c_mux_mutex;

struct overtemp_info {
    const int32_t overtemp_c;
    const int32_t overtemp_drop_c;
    bool in_overtemp;
    uint32_t critical_timer;
};

struct sensor_and_channel {
    const struct device *sensor;
    const enum sensor_channel channel;
    const orb_mcu_Temperature_TemperatureSource temperature_source;
    struct orb_state_dynamic_data *const state;
    temperature_callback cb;
    struct overtemp_info *cb_data;
    int32_t history[TEMPERATURE_AVERAGE_SAMPLE_COUNT];
    size_t wr_idx;
    int32_t average;
};

#define TEMPERATURE_SENTINEL_VALUE INT32_MIN

enum temperature_sensors {
    TEMPERATURE_SENSOR_MAIN_BOARD = 0,
    TEMPERATURE_SENSOR_LIQUID_LENS,
    TEMPERATURE_SENSOR_DIE,
#if defined(CONFIG_BOARD_PEARL_MAIN)
    TEMPERATURE_SENSOR_FRONT_UNIT,
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    TEMPERATURE_SENSOR_MAIN_BOARD_USB_HUB_BOT,
    TEMPERATURE_SENSOR_MAIN_BOARD_USB_HUB_TOP,
    TEMPERATURE_SENSOR_MAIN_BOARD_SECURITY_SUPPLY,
    TEMPERATURE_SENSOR_MAIN_BOARD_AUDIO_AMPLIFIER,
    TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAP_CHARGER,
    TEMPERATURE_SENSOR_POWER_BOARD_PVCC_SUPPLY,
    TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAPS_BOT,
    TEMPERATURE_SENSOR_POWER_BOARD_12V_CAPS_BOT,
    TEMPERATURE_SENSOR_FRONT_UNIT_850_LEFT_TOP,
    TEMPERATURE_SENSOR_FRONT_UNIT_850_LEFT_BOTTOM,
    TEMPERATURE_SENSOR_FRONT_UNIT_850_RIGHT_TOP,
    TEMPERATURE_SENSOR_FRONT_UNIT_940_LEFT_TOP,
    TEMPERATURE_SENSOR_FRONT_UNIT_940_LEFT_BOTTOM,
    TEMPERATURE_SENSOR_FRONT_UNIT_940_RIGHT_TOP,
    TEMPERATURE_SENSOR_FRONT_UNIT_940_RIGHT_BOTTOM,
    TEMPERATURE_SENSOR_FRONT_UNIT_850_CENTER_BOTTOM,
    TEMPERATURE_SENSOR_FRONT_UNIT_WHITE_RGB_LEFT_BOT,
    TEMPERATURE_SENSOR_FRONT_UNIT_WHITE_RGB_RIGHT_BOT,
    TEMPERATURE_SENSOR_FRONT_UNIT_940_SHROUD_RGB_TOP,
#endif
    TEMPERATURE_SENSOR_COUNT
};

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
const struct device fu_virtual = {.name = "fu_sensors_avg_virtual"};
struct overtemp_info fu_overtemp_data = {.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                         .overtemp_drop_c =
                                             OVERTEMP_TO_NOMINAL_DROP_C,
                                         .in_overtemp = false,
                                         .critical_timer = 0};
static struct sensor_and_channel fu_avg_sensors = {
    .sensor = &fu_virtual,
    .channel = SENSOR_CHAN_AMBIENT_TEMP,
    .temperature_source = orb_mcu_Temperature_TemperatureSource_FRONT_UNIT,
    .state = &ORB_STATE_ITEM_DYNAMIC_DATA(tmp_front),
    .cb = overtemp_callback,
    .cb_data = &fu_overtemp_data,
    .history = {0},
    .wr_idx = 0,
    .average = TEMPERATURE_SENTINEL_VALUE};

#endif

static struct sensor_and_channel sensors_and_channels[] = {
    [TEMPERATURE_SENSOR_MAIN_BOARD] =
#if defined(CONFIG_BOARD_PEARL_MAIN)
        {.sensor = DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor)),
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor_security_supply)),
#endif
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source = orb_mcu_Temperature_TemperatureSource_MAIN_BOARD,
         .state = &ORB_STATE_ITEM_DYNAMIC_DATA(tmp_main),
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
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_LIQUID_LENS,
         .state = &ORB_STATE_ITEM_DYNAMIC_DATA(tmp_lens),
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
    [TEMPERATURE_SENSOR_DIE] =
        {.sensor = &(struct device){.name = "die_temp"},
         .channel = SENSOR_CHAN_DIE_TEMP,
         .temperature_source = orb_mcu_Temperature_TemperatureSource_MAIN_MCU,
         .state = NULL,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = MCU_DIE_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
#if defined(CONFIG_BOARD_PEARL_MAIN)
    [TEMPERATURE_SENSOR_FRONT_UNIT] =
        {.sensor = DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source = orb_mcu_Temperature_TemperatureSource_FRONT_UNIT,
         .state = &ORB_STATE_ITEM_DYNAMIC_DATA(tmp_front),
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    [TEMPERATURE_SENSOR_MAIN_BOARD_USB_HUB_BOT] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor_usb_hub_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_MAIN_BOARD_USB_HUB_BOT,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_MAIN_BOARD_USB_HUB_TOP] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor_usb_hub_top)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_MAIN_BOARD_USB_HUB_TOP,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_MAIN_BOARD_SECURITY_SUPPLY] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor_security_supply)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_MAIN_BOARD_SECURITY_SUPPLY,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_MAIN_BOARD_AUDIO_AMPLIFIER] =
        {.sensor = DEVICE_DT_GET(DT_NODELABEL(main_board_tmp_sensor_audio_amp)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_MAIN_BOARD_AUDIO_AMPLIFIER,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAP_CHARGER] =
        {.sensor = DEVICE_DT_GET(
             DT_NODELABEL(power_board_tmp_sensor_super_cap_charger)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_POWER_BOARD_SUPER_CAP_CHARGER,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_POWER_BOARD_PVCC_SUPPLY] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(power_board_tmp_sensor_pvcc_supply)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_POWER_BOARD_PVCC_SUPPLY,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAPS_BOT] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(power_board_tmp_sensor_super_caps_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_POWER_BOARD_SUPER_CAPS_BOT,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_POWER_BOARD_12V_CAPS_BOT] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(power_board_tmp_sensor_12v_caps_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_POWER_BOARD_12V_CAPS_BOT,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_850_LEFT_TOP] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_850_left_top)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_850_LEFT_TOP,
         .state = NULL,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_850_LEFT_BOTTOM] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_850_left_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_850_LEFT_BOTTOM,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_850_RIGHT_TOP] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_850_right_top)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_850_RIGHT_TOP,
         .state = NULL,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_940_LEFT_TOP] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_940_left_top)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_940_LEFT_TOP,
         .state = NULL,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_940_LEFT_BOTTOM] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_940_left_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_940_LEFT_BOTTOM,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_940_RIGHT_TOP] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_940_right_top)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_940_RIGHT_TOP,
         .state = NULL,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_940_RIGHT_BOTTOM] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_940_right_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_940_RIGHT_BOTTOM,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_WHITE_RGB_RIGHT_BOT] =
        {.sensor = DEVICE_DT_GET(
             DT_NODELABEL(front_unit_tmp_sensor_white_rgb_right_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_WHITE_RGB_RIGHT_BOT,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_850_CENTER_BOTTOM] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_850_center_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_850_CENTER_BOTTOM,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_WHITE_RGB_LEFT_BOT] =
        {.sensor = DEVICE_DT_GET(
             DT_NODELABEL(front_unit_tmp_sensor_white_rgb_left_bot)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_WHITE_RGB_LEFT_BOT,
         .state = NULL,
         .cb = NULL,
         .cb_data = NULL,
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
    [TEMPERATURE_SENSOR_FRONT_UNIT_940_SHROUD_RGB_TOP] =
        {.sensor =
             DEVICE_DT_GET(DT_NODELABEL(front_unit_tmp_sensor_shroud_rgb_top)),
         .channel = SENSOR_CHAN_AMBIENT_TEMP,
         .temperature_source =
             orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_SHROUD_RGB_TOP,
         .state = NULL,
         .cb = overtemp_callback,
         .cb_data = &(struct overtemp_info){.overtemp_c = FRONT_UNIT_OVERTEMP_C,
                                            .overtemp_drop_c =
                                                OVERTEMP_TO_NOMINAL_DROP_C,
                                            .in_overtemp = false,
                                            .critical_timer = 0},
         .history = {0},
         .wr_idx = 0,
         .average = TEMPERATURE_SENTINEL_VALUE},
#endif
};

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
calculate_die_temperature(const uint16_t vref_mv, const uint16_t ts_data_raw)
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

ret_code_t
temperature_set_sampling_period_ms(uint32_t sample_period)
{
    if (sample_period < 100 || sample_period > 15000) {
        return RET_ERROR_INVALID_PARAM;
    }

    global_sample_period =
        K_MSEC(sample_period / TEMPERATURE_AVERAGE_SAMPLE_COUNT);
    k_wakeup(thread_id);

    return RET_SUCCESS;
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
            return RET_ERROR_NOT_INITIALIZED;
        }
        if (k_mutex_lock(temperature_i2c_mux_mutex, K_MSEC(200)) != 0) {
            LOG_ERR("Could not lock mutex.");
            return RET_ERROR_BUSY;
        }
        ret = sensor_sample_fetch(dev);
        k_mutex_unlock(temperature_i2c_mux_mutex);

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
        int ret = sensor_and_channel->cb(sensor_and_channel);
        ASSERT_SOFT(ret);
    }
}

void
temperature_report(orb_mcu_Temperature_TemperatureSource source,
                   int32_t temperature_in_c)
{
    orb_mcu_Temperature temperature = {.source = source,
                                       .temperature_c = temperature_in_c};
    publish_new(&temperature, sizeof(temperature),
                orb_mcu_main_McuToJetson_temperature_tag,
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

/**
 * @brief Sample the temperature sensor and report the average
 *
 * @param sensor_and_channel the sensor to sample along with its metadata
 * @return int64_t time in ticks it took to sample and report
 */
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
                k_msleep(1);
            }
        }
    }

    if (i == TEMPERATURE_SAMPLE_RETRY_COUNT) {
        // We failed after many attempts. Reset the history and try again later
        LOG_WRN(
            "Failed to sample '%s' [source %u], after %d retries. Last ret: %d",
            sensor_and_channel->sensor->name,
            sensor_and_channel->temperature_source,
            TEMPERATURE_SAMPLE_RETRY_COUNT, ret);
        init_sensor_and_channel(sensor_and_channel);
        if (sensor_and_channel->state != NULL) {
            orb_state_set(sensor_and_channel->state, RET_ERROR_TIMEOUT,
                          "failed to sample, last ret %d", ret);
        }
        return;
    }

    sensor_and_channel->wr_idx =
        (sensor_and_channel->wr_idx + 1) % TEMPERATURE_AVERAGE_SAMPLE_COUNT;

    if (sensor_and_channel->wr_idx == 0) {
        sensor_and_channel->average = average(sensor_and_channel->history);
        LOG_DBG("%s: %d: %iC", sensor_and_channel->sensor->name,
                sensor_and_channel->temperature_source,
                sensor_and_channel->average);
        temperature_report_internal(sensor_and_channel);
    }
}

_Noreturn static void
temperature_thread()
{
    int64_t elapsed = 0;
    for (;;) {
        k_timeout_t sleep_duration = {.ticks =
                                          global_sample_period.ticks - elapsed};
        k_sleep(sleep_duration);

        int64_t start = k_uptime_ticks();
        for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
            sample_and_report_temperature(&sensors_and_channels[i]);
        }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
        // on diamond, there are many temperature sensors on the front unit
        // so average all the temperature sensors before reporting
        // and checking for overtemperature conditions
        int32_t front_unit_avg = 0;
        int32_t front_unit_count = 0;
        for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
            if (sensors_and_channels[i].temperature_source >=
                    orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_850_730_LEFT_TOP &&
                sensors_and_channels[i].temperature_source <=
                    orb_mcu_Temperature_TemperatureSource_FRONT_UNIT_WHITE_RGB_LEFT_BOT &&
                sensors_and_channels[i].average != TEMPERATURE_SENTINEL_VALUE) {
                front_unit_avg += sensors_and_channels[i].average;
                front_unit_count++;
            }
        }
        if (front_unit_count > 0) {
            fu_avg_sensors.history[fu_avg_sensors.wr_idx] =
                front_unit_avg / front_unit_count;
            fu_avg_sensors.wr_idx =
                (fu_avg_sensors.wr_idx + 1) % TEMPERATURE_AVERAGE_SAMPLE_COUNT;
            if (fu_avg_sensors.wr_idx == 0) {
                fu_avg_sensors.average = average(fu_avg_sensors.history);
                LOG_DBG("%s: %d: %iC", fu_avg_sensors.sensor->name,
                        fu_avg_sensors.temperature_source,
                        fu_avg_sensors.average);
                temperature_report_internal(&fu_avg_sensors);
            }
        }
#endif

        elapsed = (k_uptime_ticks() - start);
    }
}

static int
check_ready(void)
{
    int ret = RET_SUCCESS;

    for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
        if (sensors_and_channels[i].channel != SENSOR_CHAN_DIE_TEMP) {
            if (!device_is_ready(sensors_and_channels[i].sensor)) {
                LOG_ERR("Could not initialize temperature sensor '%s: %d'",
                        sensors_and_channels[i].sensor->name,
                        sensors_and_channels[i].temperature_source);
                if (sensors_and_channels[i].state != NULL) {
                    orb_state_set(sensors_and_channels[i].state,
                                  RET_ERROR_NOT_INITIALIZED, "not ready");
                }
                ret = RET_ERROR_INVALID_STATE;
            } else {
                LOG_INF("Initialized %s: %d",
                        sensors_and_channels[i].sensor->name,
                        sensors_and_channels[i].temperature_source);
                if (sensors_and_channels[i].state != NULL) {
                    orb_state_set(sensors_and_channels[i].state, RET_SUCCESS,
                                  "initialized");
                }
            }
        } else {
            if (sensors_and_channels[i].state != NULL) {
                orb_state_set(sensors_and_channels[i].state, RET_SUCCESS, "");
            }
        }
    }
    return ret;
}

void
temperature_init(const orb_mcu_Hardware *hw_version,
                 struct k_mutex *i2c_mux_mutex)
{
    int ret;
    temperature_i2c_mux_mutex = i2c_mux_mutex;

#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (hw_version->version ==
        orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        sensors_and_channels[TEMPERATURE_SENSOR_LIQUID_LENS].sensor =
            DEVICE_DT_GET(DT_NODELABEL(liquid_lens_tmp_sensor_ev5));
    }
    ret = device_init(
        sensors_and_channels[TEMPERATURE_SENSOR_LIQUID_LENS].sensor);
    ASSERT_SOFT(ret);
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    // overwrite evt sensors from differently wired mux (2 nodes in device tree)
    if (hw_version->power_board <
        orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_5) {
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAP_CHARGER]
            .sensor = DEVICE_DT_GET(
            DT_NODELABEL(power_board_tmp_sensor_super_cap_charger_evt));
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_PVCC_SUPPLY]
            .sensor =
            DEVICE_DT_GET(DT_NODELABEL(power_board_tmp_sensor_pvcc_supply_evt));
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAPS_BOT]
            .sensor = DEVICE_DT_GET(
            DT_NODELABEL(power_board_tmp_sensor_super_caps_bot_evt));
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_12V_CAPS_BOT]
            .sensor = DEVICE_DT_GET(
            DT_NODELABEL(power_board_tmp_sensor_12v_caps_bot_evt));
    }
    ret = device_init(
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAP_CHARGER]
            .sensor);
    ASSERT_SOFT(ret);

    ret = device_init(
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_PVCC_SUPPLY]
            .sensor);
    ASSERT_SOFT(ret);

    ret = device_init(
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_SUPER_CAPS_BOT]
            .sensor);
    ASSERT_SOFT(ret);

    ret = device_init(
        sensors_and_channels[TEMPERATURE_SENSOR_POWER_BOARD_12V_CAPS_BOT]
            .sensor);
    ASSERT_SOFT(ret);

#endif

    check_ready();
    global_sample_period = K_MSEC(1000 / TEMPERATURE_AVERAGE_SAMPLE_COUNT);

    for (size_t i = 0; i < ARRAY_SIZE(sensors_and_channels); ++i) {
        init_sensor_and_channel(&sensors_and_channels[i]);
    }

    if (thread_id == NULL) {
        thread_id =
            k_thread_create(&temperature_thread_data, stack_area,
                            K_THREAD_STACK_SIZEOF(stack_area),
                            (k_thread_entry_t)temperature_thread, NULL, NULL,
                            NULL, THREAD_PRIORITY_TEMPERATURE, 0, K_NO_WAIT);
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
            ((double)fan_speed_before_overtemperature / UINT16_MAX) * 100);

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

static int
overtemp_callback(struct sensor_and_channel *sensor_and_channel)
{
    static uint64_t last_system_time = 0;

    struct overtemp_info *overtemp_info =
        (struct overtemp_info *)sensor_and_channel->cb_data;

    if (overtemp_info == NULL) {
        LOG_ERR("Over-temperature callback called without data");
        return RET_ERROR_INVALID_PARAM;
    }

    if (sensor_and_channel->state != NULL &&
        sensor_and_channel->average <= overtemp_info->overtemp_c) {
        orb_state_set(sensor_and_channel->state, RET_SUCCESS, "");
    }

    if (sensor_and_channel->average >
        overtemp_info->overtemp_c + OVERTEMP_TO_CRITICAL_RISE_C) {
        if (overtemp_info->critical_timer == 0) {
            last_system_time = k_uptime_get();
        } else {
            overtemp_info->critical_timer += k_uptime_delta(&last_system_time);
            last_system_time = k_uptime_get();
        }
        if (sensor_and_channel->state != NULL) {
            orb_state_set(sensor_and_channel->state, RET_ERROR_UNSAFE,
                          "over temperature");
        }
        overtemp_info->critical_timer +=
            (global_sample_period.ticks * TEMPERATURE_AVERAGE_SAMPLE_COUNT *
             1000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC);

        if (overtemp_info->critical_timer > CRITICAL_TO_SHUTDOWN_DELAY_MS) {
            // critical temperature
            orb_mcu_FatalError error = {
                .reason =
                    orb_mcu_FatalError_FatalReason_FATAL_CRITICAL_TEMPERATURE,
                .arg = sensor_and_channel->temperature_source,
            };

            // store event
            (void)publish_store(&error, sizeof(error),
                                orb_mcu_main_McuToJetson_fatal_error_tag,
                                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
#ifdef CONFIG_MEMFAULT
            MEMFAULT_REBOOT_MARK_RESET_IMMINENT(kMfltRebootReason_Temperature);
#endif

            // return early to avoid system reboot during ztest
            if (IS_ENABLED(CONFIG_ZTEST)) {
                return -1;
            }

            sys_reboot(0);
        }
    } else {
        overtemp_info->critical_timer = 0;
    }

    if (!overtemp_info->in_overtemp &&
        sensor_and_channel->average > overtemp_info->overtemp_c) {
        LOG_WRN("%s temperature exceeds %u°C", sensor_and_channel->sensor->name,
                overtemp_info->overtemp_c);
        if (sensor_and_channel->state != NULL) {
            orb_state_set(sensor_and_channel->state, RET_ERROR_UNSAFE,
                          "over threshold: %d>%dºC",
                          sensor_and_channel->average,
                          overtemp_info->overtemp_c);
        }
        overtemp_info->in_overtemp = true;
        inc_overtemp_condition();
    } else if (overtemp_info->in_overtemp &&
               sensor_and_channel->average < (overtemp_info->overtemp_c -
                                              overtemp_info->overtemp_drop_c)) {
        LOG_INF("Over-temperature alert -- %s temperature has decreased to "
                "safe value of %u°C",
                sensor_and_channel->sensor->name, sensor_and_channel->average);
        overtemp_info->in_overtemp = false;
        if (sensor_and_channel->state != NULL) {
            orb_state_set(sensor_and_channel->state, RET_SUCCESS, "nominal");
        }
        dec_overtemp_condition();
    }

    return RET_SUCCESS;
}

#if CONFIG_ZTEST

#include <zephyr/ztest.h>

ZTEST(hil, test_overtemp)
{
#define FAKE_SENSOR_OVERTEMP_C 50

    struct sensor_and_channel fake_sensor_and_channel = {
        .sensor = &(struct device){.name = "fake_sensor"},
        .channel = SENSOR_CHAN_DIE_TEMP,
        .temperature_source = orb_mcu_Temperature_TemperatureSource_MAIN_MCU,
        .state = NULL,
        .cb = overtemp_callback,
        .cb_data = NULL,
        .wr_idx = 0,
        .average = TEMPERATURE_SENTINEL_VALUE,
    };

    struct overtemp_info fake_overtemp_info = {
        .overtemp_c = FAKE_SENSOR_OVERTEMP_C,
        .overtemp_drop_c = OVERTEMP_TO_NOMINAL_DROP_C,
        .in_overtemp = false,
        .critical_timer = 1, // different from 0 to test reset
    };

    int ret = overtemp_callback(&fake_sensor_and_channel);
    // cb_data is NULL, so should return RET_ERROR_INVALID_PARAM
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "overtemp_callback should return RET_ERROR_INVALID_PARAM");

    fake_sensor_and_channel.cb_data = &fake_overtemp_info;

    // set average to nominal temperature
    fake_sensor_and_channel.average = FAKE_SENSOR_OVERTEMP_C - 1;
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(ret, RET_SUCCESS,
                  "overtemp_callback should return RET_SUCCESS");
    zassert_equal(fake_overtemp_info.critical_timer, 0,
                  "Critical timer should have been reset");

    // set average to over-temperature, not critical
    // this will put the fan at full speed
    fake_sensor_and_channel.average = FAKE_SENSOR_OVERTEMP_C + 1;
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(ret, RET_SUCCESS,
                  "overtemp_callback should return RET_SUCCESS");
    zassert_equal(fake_overtemp_info.critical_timer, 0,
                  "Critical timer should have been reset");
    zassert_equal(
        fake_overtemp_info.in_overtemp, true,
        "in_overtemp should be true because average is higher than overtemp_c");
    zassert_not_equal(
        num_sensors_in_overtemp_conditions, 0,
        "num_sensors_in_overtemp_conditions should be greater than 0");
    uint16_t fan_speed = fan_get_speed_setting();
    zassert_equal(fan_speed, UINT16_MAX,
                  "fan speed should be UINT16_MAX in overtemp");

    // reset in_overtemp to false by setting average to lower than overtemp_c -
    // overtemp_drop_c this will reset the fan to initial speed
    fake_sensor_and_channel.average =
        FAKE_SENSOR_OVERTEMP_C - OVERTEMP_TO_NOMINAL_DROP_C - 1;
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(ret, RET_SUCCESS,
                  "overtemp_callback should return RET_SUCCESS");
    zassert_equal(fake_overtemp_info.in_overtemp, false,
                  "in_overtemp should be false because average is lower than "
                  "overtemp_c - overtemp_drop_c");
    zassert_equal(fake_overtemp_info.critical_timer, 0,
                  "Critical timer should have been reset");

    // set average to over-temperature, critical
    fake_sensor_and_channel.average =
        FAKE_SENSOR_OVERTEMP_C + OVERTEMP_TO_NOMINAL_DROP_C + 1;
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(ret, RET_SUCCESS,
                  "overtemp_callback should return RET_SUCCESS");
    zassert_equal(
        fake_overtemp_info.in_overtemp, true,
        "in_overtemp should be true because average is higher than overtemp_c");
    zassert_not_equal(fake_overtemp_info.critical_timer, 0,
                      "Critical timer should have been reset");
    fan_speed = fan_get_speed_setting();
    zassert_equal(fan_speed, UINT16_MAX,
                  "fan speed should be UINT16_MAX in overtemp");

    // wait a bit for the critical temperature timer advance, without expiring
    k_msleep(CRITICAL_TO_SHUTDOWN_DELAY_MS / 2);
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(ret, RET_SUCCESS,
                  "overtemp_callback should return RET_SUCCESS because "
                  "critical temperature timer has not expired");

    // wait a bit to make sure the critical timer has expired
    k_msleep(CRITICAL_TO_SHUTDOWN_DELAY_MS / 2 + 1);
    // critial timer has expired, so should return -1 when in ztest
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(
        ret, (-1),
        "overtemp_callback should return -1 (<=> system reboot) when in ztest");

    // reset fan speed
    fake_sensor_and_channel.average =
        FAKE_SENSOR_OVERTEMP_C - OVERTEMP_TO_NOMINAL_DROP_C - 1;
    ret = overtemp_callback(&fake_sensor_and_channel);
    zassert_equal(ret, RET_SUCCESS,
                  "overtemp_callback should return RET_SUCCESS");
}
#endif
