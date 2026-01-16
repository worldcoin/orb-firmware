/*
 * Copyright (c) 2023 Tools for Humanity GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT tfh_liquid_lens

#include "liquid_lens.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <stm32_ll_hrtim.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

LOG_MODULE_REGISTER(liquid_lens, CONFIG_LIQUID_LENS_LOG_LEVEL);

/* HRTIM pointer for interrupt context - set during init */
static HRTIM_TypeDef *hr_timer;

#define LIQUID_LENS_TIM_PERIOD      0x3300
#define LIQUID_LENS_TIM_PERIOD_HALF (LIQUID_LENS_TIM_PERIOD / 2)
#define LIQUID_LENS_TIM_POS_BRIDGE  LL_HRTIM_TIMER_B
#define LIQUID_LENS_TIM_NEG_BRIDGE  LL_HRTIM_TIMER_A
#define LIQUID_LENS_TIM_HS1_OUTPUT  LL_HRTIM_OUTPUT_TB2
#define LIQUID_LENS_TIM_LS1_OUTPUT  LL_HRTIM_OUTPUT_TB1
#define LIQUID_LENS_TIM_HS2_OUTPUT  LL_HRTIM_OUTPUT_TA2
#define LIQUID_LENS_TIM_LS2_OUTPUT  LL_HRTIM_OUTPUT_TA1

/* Control loop constants */
#define LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US 1000
#define LIQUID_LENS_CONTROLLER_KI                                              \
    (500.0f * (float)LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US / 1000000.0f)
#define LIQUID_LENS_CONTROLLER_FEED_FORWARD      1.0f
#define LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE 999

/* ADC configuration */
#define ADC_SAMPLING_PERIOD_US 1000
#define ADC_RESOLUTION_BITS    12
#define ADC_OVERSAMPLING       5
#define ADC_MAX_VALUE          ((1 << ADC_RESOLUTION_BITS) - 1)

/* ADC channel indices */
typedef enum {
    ADC_CH_INA240_REF,
    ADC_CH_INA240_SIG,
    ADC_CH_VREFINT,
    ADC_CH_COUNT
} adc_channel_t;

/* Device tree property helpers for string-to-float conversion */
#define DT_INST_SHUNT_RESISTANCE(inst, idx)                                    \
    DT_STRING_UNQUOTED_BY_IDX(DT_DRV_INST(inst), shunt_resistor_ohms, idx)

/* Configuration structure */
struct liquid_lens_config {
    const struct pinctrl_dev_config *pcfg;
    struct gpio_dt_spec enable_gpio;
    const struct adc_dt_spec *adc_channels;
    size_t num_adc_channels;
    const struct device *adc_dev;
    HRTIM_TypeDef *hrtim;
    struct stm32_pclken hrtim_pclken;
    uint32_t amplifier_gain_default;
    float shunt_resistance_default;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    uint32_t amplifier_gain_ev5;
    float shunt_resistance_ev5;
#endif
};

/* Runtime data structure */
struct liquid_lens_data {
    atomic_t target_current_ma;
    float pwm_output_integral_per_mille;
    int16_t last_pwm_output_per_mille;
    bool enabled;
    float current_amplifier_gain;
    float shunt_resistance_ohms;
    uint16_t adc_samples_buffer[ADC_CH_COUNT];
    struct k_thread thread_data;
    k_tid_t thread_id;
};

/* Forward declarations */
static void
liquid_lens_thread(void *arg1, void *arg2, void *arg3);
static int
liquid_lens_self_test(const struct device *dev);

/**
 * @brief Set the PWM duty cycle
 * Can be used in interrupt context
 * @param per_mille PWM duty cycle in per_mille, clamped to [-999,999]
 */
static void
liquid_lens_set_pwm(int16_t per_mille)
{
    per_mille = CLAMP(per_mille, -LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE,
                      LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE);

    LL_HRTIM_TIM_SetCompare2(
        hr_timer, LIQUID_LENS_TIM_POS_BRIDGE,
        (LIQUID_LENS_TIM_PERIOD_HALF +
         (LIQUID_LENS_TIM_PERIOD_HALF * (int32_t)per_mille) / 1000));
    LL_HRTIM_TIM_SetCompare2(
        hr_timer, LIQUID_LENS_TIM_NEG_BRIDGE,
        (LIQUID_LENS_TIM_PERIOD_HALF -
         (LIQUID_LENS_TIM_PERIOD_HALF * (int32_t)per_mille) / 1000));
}

/**
 * @brief Get the STM32 VREF from raw ADC value
 * This is a simplified version - in practice you'd use the voltage_measurement
 * module
 */
static uint16_t
get_vref_mv_from_raw(uint16_t vrefint_raw)
{
    /* VREFINT typical value is 1.212V, factory calibrated value at 3.0V or 3.3V
     * For simplicity, assume 3.3V reference and 1.212V VREFINT
     * Real implementation should use factory calibration values
     */
    if (vrefint_raw == 0) {
        return 3300; /* Fallback */
    }

    /* VREFINT_CAL is typically measured at 3.0V or 3.3V (depends on STM32
     * variant) Using 1.212V typical VREFINT value */
    const uint32_t vrefint_mv = 1212;
    return (uint32_t)(vrefint_mv * ADC_MAX_VALUE) / vrefint_raw;
}

/* ADC callback - runs in interrupt context */
static enum adc_action
adc_callback(const struct device *adc_dev, const struct adc_sequence *sequence,
             uint16_t sampling_index)
{
    ARG_UNUSED(adc_dev);
    ARG_UNUSED(sequence);
    ARG_UNUSED(sampling_index);

    /* Get the liquid lens device from the user_data */
    const struct device *dev =
        (const struct device *)sequence->options->user_data;
    struct liquid_lens_data *data = dev->data;

    if (!data->enabled) {
        return ADC_ACTION_REPEAT;
    }

    const uint16_t stm32_vref_mv =
        get_vref_mv_from_raw(data->adc_samples_buffer[ADC_CH_VREFINT]);

    const int32_t current_amplifier_sig_mv =
        (int32_t)(((uint64_t)data->adc_samples_buffer[ADC_CH_INA240_SIG] *
                   (uint64_t)stm32_vref_mv) /
                  (uint64_t)ADC_MAX_VALUE);
    const int32_t current_amplifier_ref_mv =
        (int32_t)(((uint64_t)data->adc_samples_buffer[ADC_CH_INA240_REF] *
                   (uint64_t)stm32_vref_mv) /
                  (uint64_t)ADC_MAX_VALUE);

    const int32_t shunt_voltage_mv =
        current_amplifier_ref_mv - current_amplifier_sig_mv;

    const int32_t lens_current_ma =
        (int32_t)(((float)shunt_voltage_mv) / data->current_amplifier_gain /
                  data->shunt_resistance_ohms);

    const int32_t target_ma = atomic_get(&data->target_current_ma);

    LOG_DBG("lens_current_ma: %d; sig_mV: %d; ref_mV: %d", lens_current_ma,
            current_amplifier_sig_mv, current_amplifier_ref_mv);

    /* PI control with feed forward */
    const int32_t lens_current_error_ma = target_ma - lens_current_ma;

    data->pwm_output_integral_per_mille +=
        (float)lens_current_error_ma * LIQUID_LENS_CONTROLLER_KI;

    /* Limit integral value to prevent controller windup */
    if (data->pwm_output_integral_per_mille <
        -LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE) {
        data->pwm_output_integral_per_mille =
            -LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE;
    } else if (data->pwm_output_integral_per_mille >
               LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE) {
        data->pwm_output_integral_per_mille =
            LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE;
    }

    const float pwm_feed_forward_per_mille =
        LIQUID_LENS_CONTROLLER_FEED_FORWARD * (float)target_ma;

    const float pwm_output_float =
        pwm_feed_forward_per_mille + data->pwm_output_integral_per_mille;
    const int16_t pwm_output_per_mille = (int16_t)CLAMP(
        lroundf(pwm_output_float), -LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE,
        LIQUID_LENS_MAX_CONTROL_OUTPUT_PER_MILLE);

    data->last_pwm_output_per_mille = pwm_output_per_mille;
    liquid_lens_set_pwm(pwm_output_per_mille);

    return ADC_ACTION_REPEAT;
}

/* API implementation: set target current */
static int
liquid_lens_api_set_target_current(const struct device *dev, int32_t current_ma)
{
    struct liquid_lens_data *data = dev->data;

    const int32_t clamped_current_ma = CLAMP(
        current_ma, LIQUID_LENS_MIN_CURRENT_MA, LIQUID_LENS_MAX_CURRENT_MA);

    if (clamped_current_ma != current_ma) {
        LOG_WRN("Clamp %" PRId32 "mA -> %" PRId32 "mA", current_ma,
                clamped_current_ma);
    }

    LOG_DBG("Setting target current to %" PRId32 " mA", clamped_current_ma);
    atomic_set(&data->target_current_ma, clamped_current_ma);

    return 0;
}

/* API implementation: enable */
static int
liquid_lens_api_enable(const struct device *dev)
{
    const struct liquid_lens_config *config = dev->config;
    struct liquid_lens_data *data = dev->data;

    if (data->enabled) {
        return 0;
    }

    /* Reset integral to avoid windup from previous enable/disable cycles */
    data->pwm_output_integral_per_mille = 0.0f;

    LOG_INF("Enabling liquid lens current");

    LL_HRTIM_EnableOutput(
        hr_timer, LIQUID_LENS_TIM_LS2_OUTPUT | LIQUID_LENS_TIM_HS2_OUTPUT |
                      LIQUID_LENS_TIM_LS1_OUTPUT | LIQUID_LENS_TIM_HS1_OUTPUT);
    LL_HRTIM_TIM_CounterEnable(hr_timer, LIQUID_LENS_TIM_POS_BRIDGE |
                                             LIQUID_LENS_TIM_NEG_BRIDGE);

    int ret = gpio_pin_set_dt(&config->enable_gpio, 1);
    if (ret != 0) {
        LOG_ERR("Failed to enable liquid lens GPIO: %d", ret);
        return ret;
    }

    data->enabled = true;

    return 0;
}

/* API implementation: disable */
static int
liquid_lens_api_disable(const struct device *dev)
{
    const struct liquid_lens_config *config = dev->config;
    struct liquid_lens_data *data = dev->data;

    if (!data->enabled) {
        return 0;
    }

    /* Perform self-test before disabling */
    int test_result = liquid_lens_self_test(dev);
    if (test_result != 0) {
        LOG_WRN("Liquid lens self-test failed: %d", test_result);
    }

    LOG_INF("Disabling liquid lens current");

    int ret = gpio_pin_set_dt(&config->enable_gpio, 0);
    if (ret != 0) {
        LOG_ERR("Failed to disable liquid lens GPIO: %d", ret);
        return ret;
    }

    LL_HRTIM_TIM_CounterDisable(hr_timer, LIQUID_LENS_TIM_POS_BRIDGE |
                                              LIQUID_LENS_TIM_NEG_BRIDGE);
    LL_HRTIM_DisableOutput(
        hr_timer, LIQUID_LENS_TIM_LS2_OUTPUT | LIQUID_LENS_TIM_HS2_OUTPUT |
                      LIQUID_LENS_TIM_LS1_OUTPUT | LIQUID_LENS_TIM_HS1_OUTPUT);

    data->enabled = false;

    return 0;
}

/* API implementation: is_enabled */
static bool
liquid_lens_api_is_enabled(const struct device *dev)
{
    struct liquid_lens_data *data = dev->data;

    return data->enabled;
}

/* API implementation: configure_current_sense */
static int
liquid_lens_api_configure_current_sense(const struct device *dev,
                                        uint32_t amplifier_gain,
                                        float shunt_resistance_ohms)
{
    struct liquid_lens_data *data = dev->data;

    if (amplifier_gain == 0 || shunt_resistance_ohms <= 0.0f) {
        return -EINVAL;
    }

    data->current_amplifier_gain = (float)amplifier_gain;
    data->shunt_resistance_ohms = shunt_resistance_ohms;

    LOG_INF("Configured current sense: gain=%u, shunt=%.3f ohms",
            amplifier_gain, (double)shunt_resistance_ohms);

    return 0;
}

/* Self-test implementation */
static int
liquid_lens_self_test(const struct device *dev)
{
    struct liquid_lens_data *data = dev->data;
    int ret = -EIO;

    /* Reset to 0 */
    liquid_lens_api_set_target_current(dev, 0);
    k_msleep(10);

    int16_t prev_pwm = data->last_pwm_output_per_mille;

    /* Check that PWM output changes with new target current */
    liquid_lens_api_set_target_current(dev, 50);
    k_msleep(10);

    if (data->last_pwm_output_per_mille != prev_pwm) {
        /* Check that PWM output is stable */
        prev_pwm = data->last_pwm_output_per_mille;
        k_msleep(10);

        if (abs(data->last_pwm_output_per_mille - prev_pwm) <= 1) {
            ret = 0;
        }
    }

    return ret;
}

/* ADC sampling thread */
static void
liquid_lens_thread(void *arg1, void *arg2, void *arg3)
{
    const struct device *dev = (const struct device *)arg1;
    const struct liquid_lens_config *config = dev->config;
    struct liquid_lens_data *data = dev->data;
    int err;

    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct adc_sequence_options sequence_options = {0};
    sequence_options.callback = adc_callback;
    sequence_options.interval_us = ADC_SAMPLING_PERIOD_US;
    sequence_options.user_data = (void *)dev;

    struct adc_sequence sequence = {0};
    sequence.options = &sequence_options;
    sequence.channels = 0;
    sequence.buffer = (uint16_t *)data->adc_samples_buffer;
    sequence.buffer_size = sizeof(data->adc_samples_buffer);
    sequence.resolution = ADC_RESOLUTION_BITS;
    sequence.oversampling = ADC_OVERSAMPLING;
    sequence.calibrate = false;

    for (size_t i = 0; i < config->num_adc_channels; i++) {
        const struct adc_dt_spec *adc_channel = &config->adc_channels[i];
        if (adc_channel->dev == config->adc_dev) {
            sequence.channels |= BIT(adc_channel->channel_id);
        }
    }

    while (1) {
        /* adc_read should block forever because the callback function always
         * requests a repetition of the sample */
        err = adc_read(config->adc_dev, &sequence);
        LOG_ERR("ADC read returned unexpectedly, err = %d", err);

        /* Repeat adc_read after 1 second */
        k_sleep(K_MSEC(1000));
    }
}

/* HRTIM initialization */
static int
liquid_lens_init_hrtim(const struct device *dev)
{
    const struct liquid_lens_config *config = dev->config;
    HRTIM_TypeDef *hrtim = config->hrtim;
    int err;

    /* Set global hr_timer pointer for use in interrupt context */
    hr_timer = hrtim;

    /* Enable HRTIM clock */
    const struct device *clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

    err = clock_control_on(clk, (void *)&config->hrtim_pclken);
    if (err) {
        LOG_ERR("Failed to enable HRTIM clock: %d", err);
        return err;
    }

    /* Apply pin control */
    err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
    if (err < 0) {
        LOG_ERR("Liquid lens pinctrl setup failed: %d", err);
        return err;
    }

    /* Configure DLL calibration */
    LL_HRTIM_ConfigDLLCalibration(hrtim,
                                  LL_HRTIM_DLLCALIBRATION_MODE_CONTINUOUS,
                                  LL_HRTIM_DLLCALIBRATION_RATE_3);
    LL_HRTIM_StartDLLCalibration(hrtim);

    while (!LL_HRTIM_IsActiveFlag_DLLRDY(hrtim)) {
        /* Wait for DLL calibration */
    }

    /* Configure timer prescaler and mode */
    LL_HRTIM_TIM_SetPrescaler(hrtim, LIQUID_LENS_TIM_POS_BRIDGE,
                              LL_HRTIM_PRESCALERRATIO_MUL32);
    LL_HRTIM_TIM_SetCounterMode(hrtim, LIQUID_LENS_TIM_POS_BRIDGE,
                                LL_HRTIM_MODE_CONTINUOUS);
    LL_HRTIM_TIM_SetPeriod(hrtim, LIQUID_LENS_TIM_POS_BRIDGE,
                           LIQUID_LENS_TIM_PERIOD);

    LL_HRTIM_TIM_SetPrescaler(hrtim, LIQUID_LENS_TIM_NEG_BRIDGE,
                              LL_HRTIM_PRESCALERRATIO_MUL32);
    LL_HRTIM_TIM_SetCounterMode(hrtim, LIQUID_LENS_TIM_NEG_BRIDGE,
                                LL_HRTIM_MODE_CONTINUOUS);
    LL_HRTIM_TIM_SetPeriod(hrtim, LIQUID_LENS_TIM_NEG_BRIDGE,
                           LIQUID_LENS_TIM_PERIOD);

    /* Configure output sources and polarities for H-bridge */
    LL_HRTIM_OUT_SetOutputSetSrc(hrtim, LIQUID_LENS_TIM_LS2_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(hrtim, LIQUID_LENS_TIM_LS2_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(hrtim, LIQUID_LENS_TIM_LS2_OUTPUT,
                             LL_HRTIM_OUT_POSITIVE_POLARITY);

    LL_HRTIM_OUT_SetOutputSetSrc(hrtim, LIQUID_LENS_TIM_HS2_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(hrtim, LIQUID_LENS_TIM_HS2_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(hrtim, LIQUID_LENS_TIM_HS2_OUTPUT,
                             LL_HRTIM_OUT_NEGATIVE_POLARITY);

    LL_HRTIM_OUT_SetOutputSetSrc(hrtim, LIQUID_LENS_TIM_LS1_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(hrtim, LIQUID_LENS_TIM_LS1_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(hrtim, LIQUID_LENS_TIM_LS1_OUTPUT,
                             LL_HRTIM_OUT_POSITIVE_POLARITY);

    LL_HRTIM_OUT_SetOutputSetSrc(hrtim, LIQUID_LENS_TIM_HS1_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(hrtim, LIQUID_LENS_TIM_HS1_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(hrtim, LIQUID_LENS_TIM_HS1_OUTPUT,
                             LL_HRTIM_OUT_NEGATIVE_POLARITY);

    /* Initialize compare values */
    LL_HRTIM_TIM_SetCompare1(hrtim, LIQUID_LENS_TIM_POS_BRIDGE, 0);
    LL_HRTIM_TIM_SetCompare1(hrtim, LIQUID_LENS_TIM_NEG_BRIDGE, 0);
    liquid_lens_set_pwm(0);

    /* Enable preload */
    LL_HRTIM_TIM_EnablePreload(hrtim, LIQUID_LENS_TIM_POS_BRIDGE |
                                          LIQUID_LENS_TIM_NEG_BRIDGE);

    /* Configure update trigger for synchronous updates */
    LL_HRTIM_TIM_SetUpdateTrig(hrtim, LIQUID_LENS_TIM_POS_BRIDGE,
                               LL_HRTIM_UPDATETRIG_RESET);
    LL_HRTIM_TIM_SetUpdateTrig(hrtim, LIQUID_LENS_TIM_NEG_BRIDGE,
                               LL_HRTIM_UPDATETRIG_RESET);

    return 0;
}

/* ADC initialization */
static int
liquid_lens_init_adc(const struct device *dev)
{
    const struct liquid_lens_config *config = dev->config;

    /* Configure ADC channels */
    for (size_t i = 0; i < config->num_adc_channels; i++) {
        if (!device_is_ready(config->adc_channels[i].dev)) {
            LOG_ERR("ADC controller device %s not ready",
                    config->adc_channels[i].dev->name);
            return -ENODEV;
        }

        int ret = adc_channel_setup_dt(&config->adc_channels[i]);
        if (ret < 0) {
            LOG_ERR("Could not setup ADC channel #%zu (%d)", i, ret);
            return ret;
        }
    }

    /* Enable VREFINT path - hardcoded for ADC3 */
    uint32_t path =
        LL_ADC_GetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC3));
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC3),
                                   path | LL_ADC_PATH_INTERNAL_VREFINT);

    return 0;
}

/* Driver API */
static const struct liquid_lens_driver_api liquid_lens_api = {
    .set_target_current = liquid_lens_api_set_target_current,
    .enable = liquid_lens_api_enable,
    .disable = liquid_lens_api_disable,
    .is_enabled = liquid_lens_api_is_enabled,
    .configure_current_sense = liquid_lens_api_configure_current_sense,
};

/* Device initialization */
static int
liquid_lens_init(const struct device *dev)
{
    const struct liquid_lens_config *config = dev->config;
    struct liquid_lens_data *data = dev->data;
    int err;

    /* Set default gains/resistances (can be overridden by application
     * for different hardware versions) */
    data->current_amplifier_gain = config->amplifier_gain_default;
    data->shunt_resistance_ohms = config->shunt_resistance_default;

    /* Initialize enable GPIO */
    err = gpio_pin_configure_dt(&config->enable_gpio, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("Failed to configure enable GPIO: %d", err);
        return err;
    }

    /* Initialize HRTIM */
    err = liquid_lens_init_hrtim(dev);
    if (err) {
        return err;
    }

    /* Initialize ADC */
    err = liquid_lens_init_adc(dev);
    if (err) {
        return err;
    }

    /* Create ADC sampling thread */
    static K_THREAD_STACK_DEFINE(liquid_lens_stack,
                                 CONFIG_LIQUID_LENS_THREAD_STACK_SIZE);

    data->thread_id =
        k_thread_create(&data->thread_data, liquid_lens_stack,
                        K_THREAD_STACK_SIZEOF(liquid_lens_stack),
                        liquid_lens_thread, (void *)dev, NULL, NULL,
                        CONFIG_LIQUID_LENS_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(data->thread_id, "liquid_lens");

    /* Perform initial self-test */
    liquid_lens_api_enable(dev);
    err = liquid_lens_self_test(dev);
    if (err != 0) {
        LOG_WRN("Initial self-test failed: %d", err);
    }
    liquid_lens_api_disable(dev);

    LOG_INF("Liquid lens driver initialized");

    return 0;
}

/* Helper macro for ADC channel specs */
#define ADC_DT_SPEC_GET_BY_IDX_INST(inst, idx)                                 \
    ADC_DT_SPEC_GET_BY_IDX(DT_DRV_INST(inst), idx)

/* Instance-specific data and config */
#define LIQUID_LENS_DEFINE(inst)                                               \
                                                                               \
    PINCTRL_DT_INST_DEFINE(inst);                                              \
                                                                               \
    static const struct adc_dt_spec liquid_lens_adc_channels_##inst[] = {      \
        ADC_DT_SPEC_GET_BY_IDX_INST(inst, 0),                                  \
        ADC_DT_SPEC_GET_BY_IDX_INST(inst, 1),                                  \
        ADC_DT_SPEC_GET_BY_IDX_INST(inst, 2),                                  \
    };                                                                         \
                                                                               \
    static const struct liquid_lens_config liquid_lens_config_##inst = {       \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                          \
        .enable_gpio = GPIO_DT_SPEC_INST_GET(inst, enable_gpios),              \
        .adc_channels = liquid_lens_adc_channels_##inst,                       \
        .num_adc_channels = ARRAY_SIZE(liquid_lens_adc_channels_##inst),       \
        .adc_dev = DEVICE_DT_GET(                                              \
            DT_PHANDLE_BY_IDX(DT_DRV_INST(inst), io_channels, 0)),             \
        .hrtim = (HRTIM_TypeDef *)DT_REG_ADDR(DT_INST_PHANDLE(inst, hrtim)),   \
        .hrtim_pclken =                                                        \
            {                                                                  \
                .bus = DT_CLOCKS_CELL(DT_INST_PHANDLE(inst, hrtim), bus),      \
                .enr = DT_CLOCKS_CELL(DT_INST_PHANDLE(inst, hrtim), bits),     \
            },                                                                 \
        .amplifier_gain_default =                                              \
            DT_INST_PROP_BY_IDX(inst, amplifier_gains, 0),                     \
        .shunt_resistance_default = DT_INST_SHUNT_RESISTANCE(inst, 0),         \
        IF_ENABLED(                                                            \
            CONFIG_BOARD_PEARL_MAIN,                                           \
            (.amplifier_gain_ev5 =                                             \
                 DT_INST_PROP_BY_IDX(inst, amplifier_gains, 1),                \
             .shunt_resistance_ev5 = DT_INST_SHUNT_RESISTANCE(inst, 1), ))};   \
                                                                               \
    static struct liquid_lens_data liquid_lens_data_##inst = {                 \
        .target_current_ma = ATOMIC_INIT(0),                                   \
        .pwm_output_integral_per_mille = 0.0f,                                 \
        .last_pwm_output_per_mille = 0,                                        \
        .enabled = false,                                                      \
    };                                                                         \
                                                                               \
    DEVICE_DT_INST_DEFINE(inst, liquid_lens_init, NULL,                        \
                          &liquid_lens_data_##inst,                            \
                          &liquid_lens_config_##inst, POST_KERNEL,             \
                          CONFIG_LIQUID_LENS_INIT_PRIORITY, &liquid_lens_api);

DT_INST_FOREACH_STATUS_OKAY(LIQUID_LENS_DEFINE)
