#include "voltage_measurement.h"
#include "app_config.h"
#include "logs_can.h"
#include "pubsub/pubsub.h"
#include "utils.h"
#include <app_assert.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(voltage_measurement, CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL);

K_THREAD_STACK_DEFINE(voltage_measurement_adc1_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC1);
static struct k_thread voltage_measurement_adc1_thread_data = {0};

K_THREAD_STACK_DEFINE(voltage_measurement_adc5_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC5);
static struct k_thread voltage_measurement_adc5_thread_data = {0};

K_THREAD_STACK_DEFINE(voltage_measurement_publish_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_PUBLISH);
static struct k_thread voltage_measurement_publish_thread_data = {0};

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
K_THREAD_STACK_DEFINE(voltage_measurement_debug_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_DEBUG);
static struct k_thread voltage_measurement_debug_thread_data = {0};
#endif

#if !DT_NODE_EXISTS(DT_PATH(voltage_measurement)) ||                           \
    !DT_NODE_HAS_PROP(DT_PATH(voltage_measurement), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define DT_SPEC_AND_COMMA(node_id, prop, idx)                                  \
    ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

#define DT_PROP_AND_COMMA(node_id, prop, idx)                                  \
    DT_PROP_BY_IDX(node_id, prop, idx),

#define DT_STRING_UNQUOTED_AND_COMMA(node_id, prop, idx)                       \
    DT_STRING_UNQUOTED_BY_IDX(node_id, prop, idx),

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
static const struct gpio_dt_spec debug_led_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), debug_led_gpios);
#endif

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {DT_FOREACH_PROP_ELEM(
    DT_PATH(voltage_measurement), io_channels, DT_SPEC_AND_COMMA)};
static const float voltage_divider_scalings[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(voltage_measurement), voltage_divider_scalings,
                         DT_STRING_UNQUOTED_AND_COMMA)};
#if defined(CONFIG_BOARD_PEARL_MAIN)
static const float voltage_divider_scalings_ev5[] = {DT_FOREACH_PROP_ELEM(
    DT_PATH(voltage_measurement_ev5), voltage_divider_scalings,
    DT_STRING_UNQUOTED_AND_COMMA)};
#endif
static const char voltage_measurement_channel_names[][11] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(voltage_measurement), io_channel_names,
                         DT_PROP_AND_COMMA)};
BUILD_ASSERT(CHANNEL_COUNT == ARRAY_SIZE(adc_channels),
             "Number of voltage measurement channels does not match");
BUILD_ASSERT(CHANNEL_COUNT == ARRAY_SIZE(voltage_measurement_channel_names),
             "Number of voltage measurement channels does not match");

static volatile const struct device *const adc1_dev =
    DEVICE_DT_GET(DT_NODELABEL(adc1));
static volatile const struct device *const adc5_dev =
    DEVICE_DT_GET(DT_NODELABEL(adc5));

#define ADC_SAMPLING_PERIOD_US 1000
#define ADC_RESOLUTION_BITS    12
#define ADC_OVERSAMPLING       5 // oversampling factor 2⁵ = 32
#define ADC_GAIN               ADC_GAIN_1
#define ADC_MAX_VALUE          ((1 << ADC_RESOLUTION_BITS) - 1)

// The voltage transmit period will be capped to this value if a larger
// value is requested by the Jetson.
#define MAX_VOLTAGE_TRANSMIT_PERIOD_MS 60000

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#define NUMBER_OF_CHANNELS_ADC_1 7
#else
#define NUMBER_OF_CHANNELS_ADC_1 6
#endif
#define NUMBER_OF_CHANNELS_ADC_5 5
#define NUMBER_OF_CHANNELS       (NUMBER_OF_CHANNELS_ADC_1 + NUMBER_OF_CHANNELS_ADC_5)
BUILD_ASSERT(CHANNEL_COUNT == NUMBER_OF_CHANNELS,
             "Number of voltage measurement channels does not match");

static volatile uint16_t adc1_samples_buffer[NUMBER_OF_CHANNELS_ADC_1] = {0};
static volatile uint16_t adc5_samples_buffer[NUMBER_OF_CHANNELS_ADC_5] = {0};

typedef struct {
    union {
        struct {
            uint16_t raw_adc1[NUMBER_OF_CHANNELS_ADC_1];
            uint16_t raw_adc5[NUMBER_OF_CHANNELS_ADC_5];
        };
        uint16_t raw[NUMBER_OF_CHANNELS];
    };

    union {
        struct {
            uint16_t raw_min_adc1[NUMBER_OF_CHANNELS_ADC_1];
            uint16_t raw_min_adc5[NUMBER_OF_CHANNELS_ADC_5];
        };
        uint16_t raw_min[NUMBER_OF_CHANNELS];
    };

    union {
        struct {
            uint16_t raw_max_adc1[NUMBER_OF_CHANNELS_ADC_1];
            uint16_t raw_max_adc5[NUMBER_OF_CHANNELS_ADC_5];
        };
        uint16_t raw_max[NUMBER_OF_CHANNELS];
    };
} adc_samples_buffers_t;

static adc_samples_buffers_t adc_samples_buffers = {0};

static Hardware_OrbVersion hardware_version =
    Hardware_OrbVersion_HW_VERSION_UNKNOWN;

k_tid_t tid_publish = NULL;

static atomic_t voltages_publish_period_ms = ATOMIC_INIT(0);

uint16_t
voltage_measurement_get_vref_mv(void)
{
    uint16_t vrefint_raw;

    CRITICAL_SECTION_ENTER(k);

    vrefint_raw = adc_samples_buffers.raw[CHANNEL_VREFINT];

    CRITICAL_SECTION_EXIT(k);

    if (vrefint_raw == 0) {
        return 0;
    } else {
        return voltage_measurement_get_vref_mv_from_raw(hardware_version,
                                                        vrefint_raw);
    }
}

static ret_code_t
voltage_measurement_get_stats(adc_samples_buffers_t *samples_buffers,
                              voltage_measurement_channel_t channel,
                              int32_t *voltage_mv, int32_t *min_voltage_mv,
                              int32_t *max_voltage_mv)
{
    if (channel >= ARRAY_SIZE(samples_buffers->raw)) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (hardware_version == Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    int32_t raw_value;
    int32_t raw_value_min;
    int32_t raw_value_max;
    uint16_t vrefint_raw;

    CRITICAL_SECTION_ENTER(k);

    raw_value = samples_buffers->raw[channel];
    raw_value_min = samples_buffers->raw_min[channel];
    raw_value_max = samples_buffers->raw_max[channel];
    vrefint_raw = samples_buffers->raw[CHANNEL_VREFINT];

    CRITICAL_SECTION_EXIT(k);

    int32_t vref_mv =
        voltage_measurement_get_vref_mv_from_raw(hardware_version, vrefint_raw);

    adc_raw_to_millivolts(vref_mv, ADC_GAIN, ADC_RESOLUTION_BITS, &raw_value);
    adc_raw_to_millivolts(vref_mv, ADC_GAIN, ADC_RESOLUTION_BITS,
                          &raw_value_min);
    adc_raw_to_millivolts(vref_mv, ADC_GAIN, ADC_RESOLUTION_BITS,
                          &raw_value_max);

    float voltage_divider_scaling;

#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (hardware_version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        voltage_divider_scaling = voltage_divider_scalings_ev5[channel];
    } else {
#endif
        voltage_divider_scaling = voltage_divider_scalings[channel];
#if defined(CONFIG_BOARD_PEARL_MAIN)
    }
#endif

    if (voltage_mv != NULL) {
        *voltage_mv = (int32_t)((float)raw_value * voltage_divider_scaling);
    }
    if (min_voltage_mv != NULL) {
        *min_voltage_mv =
            (int32_t)((float)raw_value_min * voltage_divider_scaling);
    }
    if (max_voltage_mv != NULL) {
        *max_voltage_mv =
            (int32_t)((float)raw_value_max * voltage_divider_scaling);
    }

    return RET_SUCCESS;
}

ret_code_t
voltage_measurement_get(const voltage_measurement_channel_t channel,
                        int32_t *voltage_mv)
{
    if (channel >= ARRAY_SIZE(adc_samples_buffers.raw) || voltage_mv == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (hardware_version == Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    int32_t raw_value;
    uint16_t vrefint_raw;

    CRITICAL_SECTION_ENTER(k);

    raw_value = adc_samples_buffers.raw[channel];
    vrefint_raw = adc_samples_buffers.raw[CHANNEL_VREFINT];

    CRITICAL_SECTION_EXIT(k);

    int32_t vref_mv =
        voltage_measurement_get_vref_mv_from_raw(hardware_version, vrefint_raw);

    adc_raw_to_millivolts(vref_mv, ADC_GAIN, ADC_RESOLUTION_BITS, &raw_value);

    float voltage_divider_scaling;

#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (hardware_version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        voltage_divider_scaling = voltage_divider_scalings_ev5[channel];
    } else {
#endif
        voltage_divider_scaling = voltage_divider_scalings[channel];
#if defined(CONFIG_BOARD_PEARL_MAIN)
    }
#endif

    *voltage_mv = (int32_t)((float)raw_value * voltage_divider_scaling);
    return RET_SUCCESS;
}

ret_code_t
voltage_measurement_get_raw(voltage_measurement_channel_t channel,
                            uint16_t *adc_raw_value)
{
    if (channel >= ARRAY_SIZE(adc_samples_buffers.raw)) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (hardware_version == Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (adc_raw_value == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    CRITICAL_SECTION_ENTER(k);

    *adc_raw_value = adc_samples_buffers.raw[channel];

    CRITICAL_SECTION_EXIT(k);

    return RET_SUCCESS;
}

// interrupt context!
static enum adc_action
adc1_callback(const struct device *dev, const struct adc_sequence *sequence,
              uint16_t sampling_index)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(sequence);
    ARG_UNUSED(sampling_index);

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
    gpio_pin_set_dt(&debug_led_gpio_spec, 1);
#endif
    memcpy((void *)adc_samples_buffers.raw_adc1, (void *)adc1_samples_buffer,
           sizeof(adc_samples_buffers.raw_adc1));

    for (uint8_t i = 0; i < NUMBER_OF_CHANNELS_ADC_1; i++) {
        if (adc_samples_buffers.raw_adc1[i] <
            adc_samples_buffers.raw_min_adc1[i]) {
            adc_samples_buffers.raw_min_adc1[i] =
                adc_samples_buffers.raw_adc1[i];
        }

        if (adc_samples_buffers.raw_adc1[i] >
            adc_samples_buffers.raw_max_adc1[i]) {
            adc_samples_buffers.raw_max_adc1[i] =
                adc_samples_buffers.raw_adc1[i];
        }
    }
#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
    gpio_pin_set_dt(&debug_led_gpio_spec, 0);
#endif
    return ADC_ACTION_REPEAT;
}

// interrupt context!
static enum adc_action
adc5_callback(const struct device *dev, const struct adc_sequence *sequence,
              uint16_t sampling_index)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(sequence);
    ARG_UNUSED(sampling_index);

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
    gpio_pin_set_dt(&debug_led_gpio_spec, 1);
#endif
    memcpy((void *)&adc_samples_buffers.raw_adc5, (void *)adc5_samples_buffer,
           sizeof(adc_samples_buffers.raw_adc5));

    for (size_t i = 0; i < NUMBER_OF_CHANNELS_ADC_5; i++) {
        if (adc_samples_buffers.raw_adc5[i] <
            adc_samples_buffers.raw_min_adc5[i]) {
            adc_samples_buffers.raw_min_adc5[i] =
                adc_samples_buffers.raw_adc5[i];
        }

        if (adc_samples_buffers.raw_adc5[i] >
            adc_samples_buffers.raw_max_adc5[i]) {
            adc_samples_buffers.raw_max_adc5[i] =
                adc_samples_buffers.raw_adc5[i];
        }
    }
#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
    gpio_pin_set_dt(&debug_led_gpio_spec, 0);
#endif
    return ADC_ACTION_REPEAT;
}

_Noreturn static void
voltage_measurement_adc1_thread()
{
    int err;

    struct adc_sequence_options sequence_options = {0};
    sequence_options.callback = adc1_callback;
    sequence_options.interval_us = ADC_SAMPLING_PERIOD_US;
    sequence_options.user_data = NULL;

    struct adc_sequence sequence = {0};
    sequence.options = &sequence_options;
    sequence.channels = 0;
    sequence.buffer = (uint16_t *)adc1_samples_buffer;
    sequence.buffer_size = sizeof(adc1_samples_buffer);
    sequence.resolution = ADC_RESOLUTION_BITS;
    sequence.oversampling = ADC_OVERSAMPLING;
    sequence.calibrate = false;

    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        const struct adc_dt_spec *adc_channel = &adc_channels[i];
        if (adc_channel->dev == adc1_dev) {
            sequence.channels |= BIT(adc_channel->channel_id);
        }
    }

    while (1) {
        // adc_read should block forever because the callback functions always
        // requests a repetition of the sample
        err = adc_read((const struct device *)adc1_dev, &sequence);
        LOG_ERR("should not be reached, err = %d", err);

        // repeat adc_read after 1 second
        k_sleep(K_MSEC(1000));
    }
}

_Noreturn static void
voltage_measurement_adc5_thread()
{
    int err;

    struct adc_sequence_options sequence_options = {0};
    sequence_options.callback = adc5_callback;
    sequence_options.interval_us = ADC_SAMPLING_PERIOD_US;
    sequence_options.user_data = NULL;

    struct adc_sequence sequence = {0};
    sequence.options = &sequence_options;
    sequence.channels = 0;
    sequence.buffer = (uint16_t *)adc5_samples_buffer;
    sequence.buffer_size = sizeof(adc5_samples_buffer);
    sequence.resolution = ADC_RESOLUTION_BITS;
    sequence.oversampling = ADC_OVERSAMPLING;
    sequence.calibrate = false;

    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        const struct adc_dt_spec *adc_channel = &adc_channels[i];
        if (adc_channel->dev == adc5_dev) {
            sequence.channels |= BIT(adc_channel->channel_id);
        }
    }

    while (1) {
        // adc_read should block forever because the callback functions always
        // requests a repetition of the sample
        err = adc_read((const struct device *)adc5_dev, &sequence);
        LOG_ERR("should not be reached, err = %d", err);

        k_sleep(K_MSEC(1000));
    }
}

static void
reset_statistics(void)
{
    CRITICAL_SECTION_ENTER(k);

    memset((void *)&adc_samples_buffers.raw_min, ADC_MAX_VALUE,
           sizeof(adc_samples_buffers.raw_min));

    memset((void *)&adc_samples_buffers.raw_max, 0,
           sizeof(adc_samples_buffers.raw_max));

    CRITICAL_SECTION_EXIT(k);
}

static void
update_min_max_from_adc_samples_buffer(
    adc_samples_buffers_t *buffer_to_update,
    const adc_samples_buffers_t *input_buffer)
{
    CRITICAL_SECTION_ENTER(k);

    for (size_t i = 0; i < ARRAY_SIZE(buffer_to_update->raw_min); i++) {
        if (input_buffer->raw_min[i] < buffer_to_update->raw_min[i]) {
            buffer_to_update->raw_min[i] = input_buffer->raw_min[i];
        }

        if (input_buffer->raw_max[i] > buffer_to_update->raw_max[i]) {
            buffer_to_update->raw_max[i] = input_buffer->raw_max[i];
        }
    }

    CRITICAL_SECTION_EXIT(k);
}

static void
publish_all_voltages(void)
{
    static Voltage voltage_msg;

    bool at_least_one_publish_successful = false;

    // copy all adc_buffers before publishing the values because they might get
    // updated in the meantime and min/max values could be lost
    adc_samples_buffers_t publish_adc_buffers;
    CRITICAL_SECTION_ENTER(k);

    memcpy(&publish_adc_buffers, (adc_samples_buffers_t *)&adc_samples_buffers,
           sizeof(publish_adc_buffers));

    reset_statistics();

    CRITICAL_SECTION_EXIT(k);

    for (Voltage_VoltageSource i = Voltage_VoltageSource_MAIN_MCU_INTERNAL;
         i <= Voltage_VoltageSource_SUPPLY_3V3_SSD; ++i) {
        voltage_msg.source = i;

        voltage_measurement_channel_t channel = CHANNEL_3V3_UC;
        switch (i) {
        case Voltage_VoltageSource_MAIN_MCU_INTERNAL:
            channel = CHANNEL_3V3_UC;
            break;
        case Voltage_VoltageSource_SECURITY_MCU_INTERNAL:
            // not available on Main MCU
            continue;
        case Voltage_VoltageSource_SUPPLY_12V:
            channel = CHANNEL_12V;
            break;
        case Voltage_VoltageSource_SUPPLY_5V:
            channel = CHANNEL_5V;
            break;
        case Voltage_VoltageSource_SUPPLY_3V8:
            if ((hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
                (hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
                (hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
                (hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
                channel = CHANNEL_3V3_SSD_3V8;
            } else {
                // not available
                continue;
            }
            break;
        case Voltage_VoltageSource_SUPPLY_3V3:
            channel = CHANNEL_3V3;
            break;
        case Voltage_VoltageSource_SUPPLY_1V8:
            channel = CHANNEL_1V8;
            break;
        case Voltage_VoltageSource_VBAT:
            // not available on Main MCU
            continue;
        case Voltage_VoltageSource_PVCC:
            channel = CHANNEL_PVCC;
            break;
        case Voltage_VoltageSource_CAPS_12V:
            channel = CHANNEL_12V_CAPS;
            break;
        case Voltage_VoltageSource_VBAT_SW:
            channel = CHANNEL_VBAT_SW;
            break;
        case Voltage_VoltageSource_SUPPLY_3V3_SSD:
            if ((hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
                (hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
                (hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
                (hardware_version ==
                 Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
                // not available
                continue;
            } else {
                channel = CHANNEL_3V3_SSD_3V8;
            }
            break;
        }

        ret_code_t ret = voltage_measurement_get_stats(
            &publish_adc_buffers, channel, &voltage_msg.voltage_current_mv,
            &voltage_msg.voltage_min_mv, &voltage_msg.voltage_max_mv);
        ASSERT_SOFT(ret);

        if (ret == RET_SUCCESS) {
            ret = publish_new(&voltage_msg, sizeof(voltage_msg),
                              McuToJetson_voltage_tag,
                              CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            if (ret == RET_SUCCESS) {
                at_least_one_publish_successful = true;
            } else {
                LOG_DBG("Voltage publish error: %d", ret);
            }
        }

        LOG_DBG("channel %s published",
                voltage_measurement_channel_names[channel]);
    }

    // If publishing of all voltages was unsuccessful the min/max values, which
    // were reset before, are updated by the values which should have been
    // transmitted. Otherwise, min/max values would get lost.
    if (!at_least_one_publish_successful) {
        update_min_max_from_adc_samples_buffer(
            (adc_samples_buffers_t *)&adc_samples_buffers,
            &publish_adc_buffers);
    }
}

_Noreturn static void
voltage_measurement_publish_thread()
{
    while (1) {
        atomic_val_t sleep_period_ms = atomic_get(&voltages_publish_period_ms);
        if (sleep_period_ms == 0) {
            k_sleep(K_FOREVER);
        } else {
            k_msleep(sleep_period_ms);
        }

        publish_all_voltages();
    }
}

void
voltage_measurement_set_publish_period(uint32_t publish_period_ms)
{
    uint16_t capped_publish_period_ms =
        CLAMP(publish_period_ms, 0, MAX_VOLTAGE_TRANSMIT_PERIOD_MS);

    LOG_DBG("setting publish period to %d ms", capped_publish_period_ms);

    atomic_set(&voltages_publish_period_ms, capped_publish_period_ms);
    k_wakeup(tid_publish);
}

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
static void
voltage_measurement_debug_thread()
{
    while (1) {
        LOG_DBG("analog voltages:");

        for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
            int32_t voltage_mv;
            int32_t min_voltage_mv;
            int32_t max_voltage_mv;
            ret_code_t ret = voltage_measurement_get_stats(
                &adc_samples_buffers, i, &voltage_mv, &min_voltage_mv,
                &max_voltage_mv);

            LOG_DBG("%s = %d mV; min = %d mV; max = %d mV",
                    voltage_measurement_channel_names[i], voltage_mv,
                    min_voltage_mv, max_voltage_mv);

            if (ret != RET_SUCCESS) {
                LOG_ERR("error = %d", ret);
            }
        }

        static uint32_t counter = 0;
        counter++;
        if (counter == 20) {
            reset_statistics();
            LOG_WRN("clearing states");
        }

        k_sleep(K_MSEC(1000));
    }
}
#endif

ret_code_t
voltage_measurement_init(const Hardware *hw_version)
{
    hardware_version = hw_version->version;

    reset_statistics();

    // provide power to operational amplifiers to enable power supply
    // measurement circuitry
    struct gpio_dt_spec supply_meas_enable_spec = GPIO_DT_SPEC_GET(
        DT_PATH(voltage_measurement), supply_voltages_meas_enable_gpios);
    int ret = gpio_pin_configure_dt(&supply_meas_enable_spec, GPIO_OUTPUT);
    ASSERT_SOFT(ret);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    ret = gpio_pin_set_dt(&supply_meas_enable_spec, 1);
    ASSERT_SOFT(ret);
    if (ret != RET_SUCCESS) {
        return ret;
    }

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
    // initialize LED for measuring timings with a logic analyzer
    ret = gpio_pin_configure_dt(&debug_led_gpio_spec, GPIO_OUTPUT);
    ASSERT_SOFT(ret);
    ret = gpio_pin_set_dt(&debug_led_gpio_spec, 0);
    ASSERT_SOFT(ret);
#endif

    /* Configure channels individually prior to sampling. */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!device_is_ready(adc_channels[i].dev)) {
            LOG_ERR("ADC controller device %s not ready",
                    adc_channels[i].dev->name);
            ASSERT_SOFT(RET_ERROR_INTERNAL);
            return RET_ERROR_INTERNAL;
        }

        ret = adc_channel_setup_dt(&adc_channels[i]);
        if (ret < 0) {
            LOG_ERR("Could not setup channel #%d (%d)", i, ret);
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }
    }

    k_tid_t tid_adc1 = k_thread_create(
        &voltage_measurement_adc1_thread_data,
        voltage_measurement_adc1_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_adc1_thread_stack),
        voltage_measurement_adc1_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC1, 0, K_NO_WAIT);
    k_thread_name_set(tid_adc1, "voltage_measurement_adc1");

    k_tid_t tid_adc5 = k_thread_create(
        &voltage_measurement_adc5_thread_data,
        voltage_measurement_adc5_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_adc5_thread_stack),
        voltage_measurement_adc5_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC5, 0, K_NO_WAIT);
    k_thread_name_set(tid_adc5, "voltage_measurement_adc5");

    // sleep for 2 sampling period so that new samples are ready as soon as the
    // module is initialized
    k_sleep(K_USEC(2 * ADC_SAMPLING_PERIOD_US));

    tid_publish = k_thread_create(
        &voltage_measurement_publish_thread_data,
        voltage_measurement_publish_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_publish_thread_stack),
        voltage_measurement_publish_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_PUBLISH, 0, K_NO_WAIT);
    k_thread_name_set(tid_publish, "voltage_measurement_publish");

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
    k_tid_t tid_debug = k_thread_create(
        &voltage_measurement_debug_thread_data,
        voltage_measurement_debug_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_debug_thread_stack),
        voltage_measurement_debug_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_DEBUG, 0, K_NO_WAIT);
    k_thread_name_set(tid_debug, "voltage_measurement_debug");
#endif

    return RET_SUCCESS;
}
