#include "voltage_measurement.h"
#include "app_config.h"
#include "orb_logs.h"
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

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
K_THREAD_STACK_DEFINE(voltage_measurement_adc4_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC4);
static struct k_thread voltage_measurement_adc4_thread_data = {0};
#endif

K_THREAD_STACK_DEFINE(voltage_measurement_adc5_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC5);
static struct k_thread voltage_measurement_adc5_thread_data = {0};

K_THREAD_STACK_DEFINE(voltage_measurement_publish_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_PUBLISH);
static struct k_thread voltage_measurement_publish_thread_data = {0};

K_THREAD_STACK_DEFINE(voltage_measurement_self_test_thread_stack,
                      THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_SELFTEST);
static struct k_thread voltage_self_test_data = {0};

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

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
static const struct gpio_dt_spec super_cap_mux_gpios_evt[] = {
    DT_FOREACH_PROP_ELEM_SEP(DT_PATH(i2c_mux_gpio_power_board_evt), mux_gpios,
                             GPIO_DT_SPEC_GET_BY_IDX, (, ))};
static const struct gpio_dt_spec super_cap_enable_gpio_evt =
    GPIO_DT_SPEC_GET(DT_PATH(i2c_mux_gpio_power_board_evt), enable_gpios);

static const struct gpio_dt_spec super_cap_mux_gpios_dvt[] = {
    DT_FOREACH_PROP_ELEM_SEP(DT_PATH(super_caps_adc_mux_power_board), mux_gpios,
                             GPIO_DT_SPEC_GET_BY_IDX, (, ))};
static const struct gpio_dt_spec super_cap_enable_gpio_dvt =
    GPIO_DT_SPEC_GET(DT_PATH(super_caps_adc_mux_power_board), enable_gpios);

static const struct gpio_dt_spec *super_cap_mux_gpios = super_cap_mux_gpios_evt;
static const struct gpio_dt_spec *super_cap_enable_gpio_ptr =
    &super_cap_enable_gpio_evt;
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
static const char voltage_measurement_channel_names[][12] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(voltage_measurement), io_channel_names,
                         DT_PROP_AND_COMMA)};
BUILD_ASSERT(CHANNEL_COUNT == ARRAY_SIZE(adc_channels),
             "Number of voltage measurement channels does not match");
BUILD_ASSERT(CHANNEL_COUNT == ARRAY_SIZE(voltage_measurement_channel_names),
             "Number of voltage measurement channels does not match");

static volatile const struct device *const adc1_dev =
    DEVICE_DT_GET(DT_NODELABEL(adc1));
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
static volatile const struct device *const adc4_dev =
    DEVICE_DT_GET(DT_NODELABEL(adc4));
#endif
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
#define NUMBER_OF_CHANNELS_ADC_1 9
#define NUMBER_OF_CHANNELS_ADC_4 2
#define NUMBER_OF_CHANNELS_ADC_5 8
#else
#define NUMBER_OF_CHANNELS_ADC_1 6
#define NUMBER_OF_CHANNELS_ADC_4 0
#define NUMBER_OF_CHANNELS_ADC_5 5
#endif

#define NUMBER_OF_CHANNELS                                                     \
    (NUMBER_OF_CHANNELS_ADC_1 + NUMBER_OF_CHANNELS_ADC_4 +                     \
     NUMBER_OF_CHANNELS_ADC_5)

BUILD_ASSERT(CHANNEL_COUNT == NUMBER_OF_CHANNELS,
             "Number of voltage measurement channels does not match");

static volatile uint16_t adc1_samples_buffer[NUMBER_OF_CHANNELS_ADC_1] = {0};
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
static volatile uint16_t adc4_samples_buffer[NUMBER_OF_CHANNELS_ADC_4] = {0};
#endif
static volatile uint16_t adc5_samples_buffer[NUMBER_OF_CHANNELS_ADC_5] = {0};

typedef struct {
    union {
        struct {
            uint16_t raw_adc1[NUMBER_OF_CHANNELS_ADC_1];
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
            uint16_t raw_adc4[NUMBER_OF_CHANNELS_ADC_4];
#endif
            uint16_t raw_adc5[NUMBER_OF_CHANNELS_ADC_5];
        };

        uint16_t raw[NUMBER_OF_CHANNELS];
    };

    union {
        struct {
            uint16_t raw_min_adc1[NUMBER_OF_CHANNELS_ADC_1];
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
            uint16_t raw_min_adc4[NUMBER_OF_CHANNELS_ADC_4];
#endif
            uint16_t raw_min_adc5[NUMBER_OF_CHANNELS_ADC_5];
        };

        uint16_t raw_min[NUMBER_OF_CHANNELS];
    };

    union {
        struct {
            uint16_t raw_max_adc1[NUMBER_OF_CHANNELS_ADC_1];
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
            uint16_t raw_max_adc4[NUMBER_OF_CHANNELS_ADC_4];
#endif
            uint16_t raw_max_adc5[NUMBER_OF_CHANNELS_ADC_5];
        };

        uint16_t raw_max[NUMBER_OF_CHANNELS];
    };
} adc_samples_buffers_t;

static adc_samples_buffers_t adc_samples_buffers = {0};

static orb_mcu_Hardware_OrbVersion hardware_version =
    orb_mcu_Hardware_OrbVersion_HW_VERSION_UNKNOWN;

k_tid_t tid_publish = NULL;

static atomic_t voltages_publish_period_ms = ATOMIC_INIT(0);

static struct k_mutex *voltages_analog_mux_mutex;

struct self_test_range {
    int32_t min;
    int32_t max;
};

static const struct self_test_range voltage_measurement_tests[] = {
    [CHANNEL_VBAT_SW] = {.min = 12000, .max = 17000},
    [CHANNEL_PVCC] = {.min = 30590, .max = 32430},
    [CHANNEL_12V_CAPS] = {.min = 11700, .max = 12280},
    [CHANNEL_3V3_UC] = {.min = 3159, .max = 3389},
    [CHANNEL_1V8] = {.min = 1710, .max = 1890},
    [CHANNEL_3V3] = {.min = 3265, .max = 3456},
    [CHANNEL_5V] = {.min = 5061, .max = 5233},
    [CHANNEL_VREFINT] = {.min = 1182, .max = 1232},
    {0}};

#define VOLTAGES_SELF_TEST_PERIOD_MS         1000
#define VOLTAGES_SELF_TEST_SUSTAIN_PERIOD_MS 3000
#define VOLTAGES_SELF_TEST_LOOP_COUNT_PASS                                     \
    (VOLTAGES_SELF_TEST_SUSTAIN_PERIOD_MS / VOLTAGES_SELF_TEST_PERIOD_MS)

#if defined(CONFIG_BOARD_DIAMOND_MAIN)

#define NUMBER_OF_SUPER_CAPS    8
#define SUPER_CAP_MUX_POSITIONS 4
#define SUPER_CAP_MUX_LOW_IDX   0
#define SUPER_CAP_MUX_HIGH_IDX  1

BUILD_ASSERT(NUMBER_OF_SUPER_CAPS == 2 * SUPER_CAP_MUX_POSITIONS,
             "Number of super caps must be 2 times the number of multiplexer "
             "switch positions");

static const float super_cap_scaling_factors[NUMBER_OF_SUPER_CAPS] = {
    1.3333f, 2.818f, 5.343f, 5.343f, 6.757f, 11.1f, 11.1f, 11.1f};

static int32_t super_cap_voltages_mv[NUMBER_OF_SUPER_CAPS] = {0};
static int32_t super_cap_differential_voltages[NUMBER_OF_SUPER_CAPS] = {0};

#endif

uint16_t
voltage_measurement_get_vref_mv(void)
{
    uint16_t vrefint_raw;

    CRITICAL_SECTION_ENTER(k);

    vrefint_raw = adc_samples_buffers.raw[CHANNEL_VREFINT];

    CRITICAL_SECTION_EXIT(k);

    if (vrefint_raw == 0) {
        return 0;
    }

    return voltage_measurement_get_vref_mv_from_raw(hardware_version,
                                                    vrefint_raw);
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

    if (hardware_version == orb_mcu_Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
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
    if (hardware_version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
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

    if (hardware_version == orb_mcu_Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
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
    if (hardware_version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
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

    if (hardware_version == orb_mcu_Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
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

    return ADC_ACTION_REPEAT;
}

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
// interrupt context!
static enum adc_action
adc4_callback(const struct device *dev, const struct adc_sequence *sequence,
              uint16_t sampling_index)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(sequence);
    ARG_UNUSED(sampling_index);

    memcpy((void *)adc_samples_buffers.raw_adc4, (void *)adc4_samples_buffer,
           sizeof(adc_samples_buffers.raw_adc4));

    for (uint8_t i = 0; i < NUMBER_OF_CHANNELS_ADC_4; i++) {
        if (adc_samples_buffers.raw_adc4[i] <
            adc_samples_buffers.raw_min_adc4[i]) {
            adc_samples_buffers.raw_min_adc4[i] =
                adc_samples_buffers.raw_adc4[i];
        }

        if (adc_samples_buffers.raw_adc4[i] >
            adc_samples_buffers.raw_max_adc4[i]) {
            adc_samples_buffers.raw_max_adc4[i] =
                adc_samples_buffers.raw_adc4[i];
        }
    }

    return ADC_ACTION_REPEAT;
}
#endif

// interrupt context!
static enum adc_action
adc5_callback(const struct device *dev, const struct adc_sequence *sequence,
              uint16_t sampling_index)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(sequence);
    ARG_UNUSED(sampling_index);

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

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
_Noreturn static void
voltage_measurement_adc4_thread()
{
    int err;

    struct adc_sequence_options sequence_options = {0};
    sequence_options.callback = adc4_callback;
    sequence_options.interval_us = ADC_SAMPLING_PERIOD_US;
    sequence_options.user_data = NULL;

    struct adc_sequence sequence = {0};
    sequence.options = &sequence_options;
    sequence.channels = 0;
    sequence.buffer = (uint16_t *)adc4_samples_buffer;
    sequence.buffer_size = sizeof(adc4_samples_buffer);
    sequence.resolution = ADC_RESOLUTION_BITS;
    sequence.oversampling = ADC_OVERSAMPLING;
    sequence.calibrate = false;

    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        const struct adc_dt_spec *adc_channel = &adc_channels[i];
        if (adc_channel->dev == adc4_dev) {
            sequence.channels |= BIT(adc_channel->channel_id);
        }
    }

    while (1) {
        // adc_read should block forever because the callback functions always
        // requests a repetition of the sample
        err = adc_read((const struct device *)adc4_dev, &sequence);
        LOG_ERR("should not be reached, err = %d", err);

        // repeat adc_read after 1 second
        k_sleep(K_MSEC(1000));
    }
}
#endif

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
    LOG_DBG("reset statistics");

    CRITICAL_SECTION_ENTER(k);

    memset((void *)&adc_samples_buffers.raw_min, 0xFF,
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
    static orb_mcu_main_Voltage voltage_msg;

    bool at_least_one_publish_successful = false;

    // copy all adc_buffers before publishing the values because they might get
    // updated in the meantime and min/max values could be lost
    adc_samples_buffers_t publish_adc_buffers;
    CRITICAL_SECTION_ENTER(k);

    memcpy(&publish_adc_buffers, (adc_samples_buffers_t *)&adc_samples_buffers,
           sizeof(publish_adc_buffers));

    reset_statistics();

    CRITICAL_SECTION_EXIT(k);

    bool is_super_cap_channel = false;

    for (orb_mcu_main_Voltage_VoltageSource i =
             orb_mcu_main_Voltage_VoltageSource_MAIN_MCU_INTERNAL;
         i <= orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_7; ++i) {
        voltage_msg.source = i;

        voltage_measurement_channel_t channel = CHANNEL_3V3_UC;
        switch (i) {
        case orb_mcu_main_Voltage_VoltageSource_MAIN_MCU_INTERNAL:
            channel = CHANNEL_3V3_UC;
            break;
        case orb_mcu_main_Voltage_VoltageSource_SECURITY_MCU_INTERNAL:
            // not available on Main MCU
            continue;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_12V:
#ifdef CONFIG_BOARD_PEARL_MAIN
            channel = CHANNEL_12V;
#else
            // not available on Diamond, 12V_CAPS is used instead
            continue;
#endif
            break;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_5V:
            channel = CHANNEL_5V;
            break;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V8:
            if ((hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
                (hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
                (hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
                (hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
                channel = CHANNEL_3V3_SSD_3V8;
            } else {
                // not available
                continue;
            }
            break;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V3:
            channel = CHANNEL_3V3;
            break;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_1V8:
            channel = CHANNEL_1V8;
            break;
        case orb_mcu_main_Voltage_VoltageSource_VBAT:
            // not available on Main MCU
            continue;
        case orb_mcu_main_Voltage_VoltageSource_PVCC:
            channel = CHANNEL_PVCC;
            break;
        case orb_mcu_main_Voltage_VoltageSource_CAPS_12V:
            channel = CHANNEL_12V_CAPS;
            break;
        case orb_mcu_main_Voltage_VoltageSource_VBAT_SW:
            channel = CHANNEL_VBAT_SW;
            break;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V3_SSD:
            if ((hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
                (hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
                (hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
                (hardware_version ==
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
                // not available
                continue;
            } else {
                channel = CHANNEL_3V3_SSD_3V8;
            }
            break;
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V3_WIFI:
            channel = CHANNEL_3V3_WIFI;
            break;
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V3_LTE:
            channel = CHANNEL_3V3_LTE;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V6:
            channel = CHANNEL_3V6;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_1V2:
            channel = CHANNEL_1V2;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_2V8:
            channel = CHANNEL_2V8;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_1V8_SEC:
            channel = CHANNEL_1V8_SEC;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_4V7_SEC:
            channel = CHANNEL_4V7_SEC;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_17V_CAPS:
            voltage_msg.voltage_current_mv = super_cap_voltages_mv[7];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_0:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[0];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_1:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[1];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_2:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[2];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_3:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[3];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_4:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[4];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_5:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[5];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_6:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[6];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;

        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_7:
            voltage_msg.voltage_current_mv = super_cap_differential_voltages[7];
            voltage_msg.voltage_min_mv = voltage_msg.voltage_current_mv;
            voltage_msg.voltage_max_mv = voltage_msg.voltage_current_mv;
            is_super_cap_channel = true;
            break;
#elif defined(CONFIG_BOARD_PEARL_MAIN)
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V3_WIFI:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V3_LTE:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_3V6:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_1V2:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_2V8:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_1V8_SEC:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_4V7_SEC:
        case orb_mcu_main_Voltage_VoltageSource_SUPPLY_17V_CAPS:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_0:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_1:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_2:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_3:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_4:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_5:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_6:
        case orb_mcu_main_Voltage_VoltageSource_SUPER_CAP_7:
            continue;
#endif
        default:
            LOG_ERR("illegal state");
            continue;
        }

        ret_code_t ret;
        if (!is_super_cap_channel) {
            ret = voltage_measurement_get_stats(
                &publish_adc_buffers, channel, &voltage_msg.voltage_current_mv,
                &voltage_msg.voltage_min_mv, &voltage_msg.voltage_max_mv);
            ASSERT_SOFT(ret);
        } else {
            ret = RET_SUCCESS;
        }

        if (ret == RET_SUCCESS) {
            ret = publish_new(&voltage_msg, sizeof(voltage_msg),
                              orb_mcu_main_McuToJetson_voltage_tag,
                              CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            if (ret == RET_SUCCESS) {
                at_least_one_publish_successful = true;
            } else {
                LOG_DBG("Voltage publish error: %d", ret);
            }
        }

        if (!is_super_cap_channel) {
            LOG_DBG("channel %s published",
                    voltage_measurement_channel_names[channel]);
        } else {
            LOG_DBG("channel super cap published");
        }
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

#if defined(CONFIG_BOARD_DIAMOND_MAIN)

static ret_code_t
voltage_measurement_sample_switched_channels(void)
{

    if (k_mutex_lock(voltages_analog_mux_mutex, K_MSEC(200)) == 0) {
        // save gpio state and restore it later for not interfering with the i2c
        // mux driver
        int mux_store[2];
        mux_store[0] = gpio_pin_get_dt(&super_cap_mux_gpios[0]);
        mux_store[1] = gpio_pin_get_dt(&super_cap_mux_gpios[1]);

        gpio_pin_set_dt(super_cap_enable_gpio_ptr, 1);

        int32_t vref_mv = DT_PROP(DT_PATH(zephyr_user), vref_mv);

        for (uint8_t i = 0; i < SUPER_CAP_MUX_POSITIONS; i++) {
            // voltage channels are connected to multiplexer in reverse order
            uint8_t mux_position = SUPER_CAP_MUX_POSITIONS - 1 - i;
            gpio_pin_set_dt(&super_cap_mux_gpios[0],
                            ((mux_position & BIT(0)) != 0));
            gpio_pin_set_dt(&super_cap_mux_gpios[1],
                            ((mux_position & BIT(1)) != 0));

            // wait for 2.1 full sampling periods to be safe that
            // the signal voltage is applied for at least one whole sample time
            k_usleep((int32_t)(ADC_SAMPLING_PERIOD_US * 2.1f));

            int32_t super_cap_voltages_raw[2];

            CRITICAL_SECTION_ENTER(k);
            super_cap_voltages_raw[SUPER_CAP_MUX_LOW_IDX] =
                adc_samples_buffers.raw[CHANNEL_V_SCAP_LOW];
            super_cap_voltages_raw[SUPER_CAP_MUX_HIGH_IDX] =
                adc_samples_buffers.raw[CHANNEL_V_SCAP_HIGH];
            CRITICAL_SECTION_EXIT(k);

            adc_raw_to_millivolts(
                vref_mv, ADC_GAIN, ADC_RESOLUTION_BITS,
                &super_cap_voltages_raw[SUPER_CAP_MUX_LOW_IDX]);
            adc_raw_to_millivolts(
                vref_mv, ADC_GAIN, ADC_RESOLUTION_BITS,
                &super_cap_voltages_raw[SUPER_CAP_MUX_HIGH_IDX]);

            super_cap_voltages_mv[i] =
                (int32_t)((float)(super_cap_voltages_raw
                                      [SUPER_CAP_MUX_LOW_IDX]) *
                          super_cap_scaling_factors[i]);
            super_cap_voltages_mv[i + SUPER_CAP_MUX_POSITIONS] =
                (int32_t)((float)(super_cap_voltages_raw
                                      [SUPER_CAP_MUX_HIGH_IDX]) *
                          super_cap_scaling_factors[i +
                                                    SUPER_CAP_MUX_POSITIONS]);
        }

#ifdef CONFIG_VOLTAGE_MEASUREMENT_LOG_LEVEL_DBG
        for (uint8_t i = 0; i < NUMBER_OF_SUPER_CAPS; i++) {
            LOG_DBG("V_SCAP_%d = %d mV", i, super_cap_voltages_mv[i]);
        }
#endif

        gpio_pin_set_dt(super_cap_enable_gpio_ptr, 0);

        // restore mux gpio values for not interfering with the i2c mux driver
        gpio_pin_set_dt(&super_cap_mux_gpios[0], mux_store[0]);
        gpio_pin_set_dt(&super_cap_mux_gpios[1], mux_store[1]);

        k_mutex_unlock(voltages_analog_mux_mutex);
    } else {
        LOG_ERR("Could not lock mutex.");
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

int
check_caps_voltages(bool with_logs)
{
    size_t error_count = 0;
    ret_code_t ret = voltage_measurement_sample_switched_channels();
    if (ret == RET_SUCCESS) {
        super_cap_differential_voltages[0] = super_cap_voltages_mv[0];
        for (uint8_t i = 1; i < NUMBER_OF_SUPER_CAPS; i++) {
            super_cap_differential_voltages[i] =
                super_cap_voltages_mv[i] - super_cap_voltages_mv[i - 1];
        }

        char cap_buf_str[10] = {0};
        for (uint8_t i = 0; i < NUMBER_OF_SUPER_CAPS; i++) {
            sprintf(cap_buf_str, "cap #%d", i + 1);
            bool passed = app_assert_range(cap_buf_str,
                                           super_cap_differential_voltages[i],
                                           super_cap_differential_voltages[i],
                                           super_cap_differential_voltages[i],
                                           1600, 2400, with_logs, "mV");
            if (!passed) {
                error_count++;
            }
        }
    } else {
        memset((void *)&super_cap_differential_voltages, 0,
               sizeof(super_cap_differential_voltages));
    }

    return error_count;
}

#else
int
check_caps_voltages(bool with_logs)
{
    UNUSED_PARAMETER(with_logs);
    return 0;
}
#endif

_Noreturn static void
voltage_measurement_publish_thread()
{
    // clear statistics to remove min/max values that occurred during booting
    // the power supplies
    reset_statistics();

    while (1) {
        atomic_val_t sleep_period_ms = atomic_get(&voltages_publish_period_ms);
        if (sleep_period_ms == 0) {
            k_sleep(K_FOREVER);
        } else {
            k_msleep(sleep_period_ms);
        }

        (void)check_caps_voltages(false);
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

static void
voltage_measurement_self_test_thread()
{
    // the test must pass for a few seconds
    // to check if capacitors are not overcharging
    uint32_t passed_loop_count = 0;
    // test to last 10 seconds maximum
    uint32_t timeout = 10;

    // reset all and gather first data
    reset_statistics();
    k_msleep(1000);

    while (passed_loop_count < VOLTAGES_SELF_TEST_LOOP_COUNT_PASS &&
           timeout--) {
        uint32_t fail_count = 0;

        fail_count = check_caps_voltages(passed_loop_count == 0);

        for (size_t i = 0; i < CHANNEL_COUNT; i++) {
            int32_t voltage_mv;
            int32_t min_voltage_mv;
            int32_t max_voltage_mv;

            // skip pvcc test when super caps aren't enabled
            if (IS_ENABLED(CONFIG_NO_SUPER_CAPS) && i == CHANNEL_PVCC) {
                continue;
            }

            ret_code_t ret = voltage_measurement_get_stats(
                &adc_samples_buffers, i, &voltage_mv, &min_voltage_mv,
                &max_voltage_mv);

            if (ret != RET_SUCCESS) {
                LOG_ERR("voltage_measurement_get_stats returned %d", ret);
                continue;
            }

            const bool passed = app_assert_range(
                voltage_measurement_channel_names[i], voltage_mv,
                min_voltage_mv, max_voltage_mv,
                voltage_measurement_tests[i].min,
                voltage_measurement_tests[i].max, passed_loop_count == 0, "mV");
            if (!passed) {
                fail_count++;
            }
        }

        if (fail_count == 0) {
            if (passed_loop_count == 0) {
                LOG_INF(
                    "✅ All voltages in range, checking that it can last %u ms",
                    VOLTAGES_SELF_TEST_SUSTAIN_PERIOD_MS);
            }
            passed_loop_count++;
        } else {
            LOG_ERR("📈 Voltages not in range!");
            if (passed_loop_count >= 1) {
                passed_loop_count--;
            } else {
                passed_loop_count = 0;
            }
        }

        reset_statistics();

        k_sleep(K_MSEC(VOLTAGES_SELF_TEST_PERIOD_MS));
    }

    if (timeout == 0) {
        LOG_ERR("Voltage self-test timed out");
    } else if (passed_loop_count == VOLTAGES_SELF_TEST_LOOP_COUNT_PASS) {
        LOG_INF("✅ Voltages self-test passed");
    }
}

ret_code_t
voltage_measurement_selftest(void)
{
    static bool initialized_once = false;

    if (initialized_once == false ||
        k_thread_join(&voltage_self_test_data, K_NO_WAIT) == 0) {
        k_thread_create(
            &voltage_self_test_data, voltage_measurement_self_test_thread_stack,
            K_THREAD_STACK_SIZEOF(voltage_measurement_self_test_thread_stack),
            (k_thread_entry_t)voltage_measurement_self_test_thread, NULL, NULL,
            NULL, THREAD_PRIORITY_VOLTAGE_MEASUREMENT_SELFTEST, 0, K_NO_WAIT);
        k_thread_name_set(&voltage_self_test_data,
                          "voltage_measurement_self_test");
    } else {
        return RET_ERROR_INVALID_STATE;
    }

    return RET_SUCCESS;
}

ret_code_t
voltage_measurement_init(const orb_mcu_Hardware *hw_version,
                         struct k_mutex *analog_mux_mutex)
{
    hardware_version = hw_version->version;
    voltages_analog_mux_mutex = analog_mux_mutex;

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

    /* /!\ hardcoded */
    /* Do not remove existing paths so read value first */
    uint32_t path =
        LL_ADC_GetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1));
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
                                   path | LL_ADC_PATH_INTERNAL_TEMPSENSOR |
                                       LL_ADC_PATH_INTERNAL_VBAT);
    path = LL_ADC_GetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC5));
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC5),
                                   path | LL_ADC_PATH_INTERNAL_VREFINT);

    k_tid_t tid_adc1 = k_thread_create(
        &voltage_measurement_adc1_thread_data,
        voltage_measurement_adc1_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_adc1_thread_stack),
        (k_thread_entry_t)voltage_measurement_adc1_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC1, 0, K_NO_WAIT);
    k_thread_name_set(tid_adc1, "voltage_measurement_adc1");

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (hw_version->front_unit ==
        orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3D) {
        super_cap_mux_gpios = super_cap_mux_gpios_dvt;
        super_cap_enable_gpio_ptr = &super_cap_enable_gpio_dvt;
    }

    k_tid_t tid_adc4 = k_thread_create(
        &voltage_measurement_adc4_thread_data,
        voltage_measurement_adc4_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_adc4_thread_stack),
        (k_thread_entry_t)voltage_measurement_adc4_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC4, 0, K_NO_WAIT);
    k_thread_name_set(tid_adc4, "voltage_measurement_adc4");
#endif

    k_tid_t tid_adc5 = k_thread_create(
        &voltage_measurement_adc5_thread_data,
        voltage_measurement_adc5_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_adc5_thread_stack),
        (k_thread_entry_t)voltage_measurement_adc5_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC5, 0, K_NO_WAIT);
    k_thread_name_set(tid_adc5, "voltage_measurement_adc5");

    // sleep for 2 sampling period so that new samples are ready as soon as the
    // module is initialized
    k_sleep(K_USEC(2 * ADC_SAMPLING_PERIOD_US));

    // Start publish thread with a delay of 10 seconds because we don't want to
    // publish voltages before all power supplies are turned on. Otherwise, it
    // is hard to filter for outliers because you see outliers in the data at
    // each boot of an Orb.
#ifdef CONFIG_ZTEST
    // don't delay execution when ZTESTs are enabled otherwise no voltage
    // messages are published when the test starts.
    const k_timeout_t delay = K_SECONDS(0);
#else
    const k_timeout_t delay = K_SECONDS(10);
#endif
    tid_publish = k_thread_create(
        &voltage_measurement_publish_thread_data,
        voltage_measurement_publish_thread_stack,
        K_THREAD_STACK_SIZEOF(voltage_measurement_publish_thread_stack),
        (k_thread_entry_t)voltage_measurement_publish_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_VOLTAGE_MEASUREMENT_PUBLISH, 0, delay);
    k_thread_name_set(tid_publish, "voltage_measurement_publish");

    return RET_SUCCESS;
}
