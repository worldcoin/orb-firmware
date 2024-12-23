#include "liquid_lens.h"
#include "orb_logs.h"
#include "system/version/version.h"
#include "voltage_measurement/voltage_measurement.h"
#include <app_assert.h>
#include <app_config.h>
#include <stdlib.h>
#include <stm32_ll_hrtim.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

LOG_MODULE_REGISTER(liquid_lens, CONFIG_LIQUID_LENS_LOG_LEVEL);

#define HR_TIMER (HRTIM_TypeDef *)HRTIM1_BASE

#define LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US 1000
#define LIQUID_LENS_CONTROLLER_KI                                              \
    ((float)LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US / 10000.0f)

#define LIQUID_LENS_TIM_PERIOD      0x3300
#define LIQUID_LENS_TIM_PERIOD_HALF (LIQUID_LENS_TIM_PERIOD / 2) // 0x1980
#define LIQUID_LENS_TIM_POS_BRIDGE  LL_HRTIM_TIMER_B
#define LIQUID_LENS_TIM_NEG_BRIDGE  LL_HRTIM_TIMER_A
#define LIQUID_LENS_TIM_HS1_OUTPUT  LL_HRTIM_OUTPUT_TB2
#define LIQUID_LENS_TIM_LS1_OUTPUT  LL_HRTIM_OUTPUT_TB1
#define LIQUID_LENS_TIM_HS2_OUTPUT  LL_HRTIM_OUTPUT_TA2
#define LIQUID_LENS_TIM_LS2_OUTPUT  LL_HRTIM_OUTPUT_TA1

#define LIQUID_LENS_MAX_CONTROL_OUTPUT 99 //%

#define DT_INST_CLK(inst)                                                      \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(inst, bus), .enr = DT_CLOCKS_CELL(inst, bits)    \
    }

static atomic_t target_current_ma;
static int8_t control_output_pwm = 0;

static const struct gpio_dt_spec liquid_lens_en =
    GPIO_DT_SPEC_GET(DT_PATH(liquid_lens), enable_gpios);
static float liquid_lens_current_amplifier_gain = 0.0f;
static float liquid_lens_shunt_resistance_ohms = 0.0f;

#if defined(CONFIG_BOARD_PEARL_MAIN)
BUILD_ASSERT(DT_PROP_LEN(DT_PATH(liquid_lens), amplifier_gains) == 2,
             "We support 2 different gains based on hardware");
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
BUILD_ASSERT(DT_PROP_LEN(DT_PATH(liquid_lens), amplifier_gains) == 1,
             "We support only one gain property on Diamond hardware");
#endif

static K_THREAD_STACK_DEFINE(liquid_lens_stack_area,
                             THREAD_STACK_SIZE_LIQUID_LENS);
static struct k_thread liquid_lens_thread_data;
static k_tid_t thread_id = NULL;

static struct stm32_pclken liquid_lens_hrtim_pclken =
    DT_INST_CLK(DT_NODELABEL(hrtim1));

PINCTRL_DT_DEFINE(DT_NODELABEL(liquid_lens));

typedef enum {
    ADC_CH_INA240_REF,
    ADC_CH_INA240_SIG,
    ADC_CH_VREFINT,
    ADC_CH_COUNT
} adc_channel_t;

#define DT_SPEC_AND_COMMA(node_id, prop, idx)                                  \
    ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(liquid_lens), io_channels, DT_SPEC_AND_COMMA)};
BUILD_ASSERT(ADC_CH_COUNT == ARRAY_SIZE(adc_channels),
             "Number of voltage measurement channels does not match");

static const struct device *const adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc3));

#define ADC_SAMPLING_PERIOD_US 1000
#define ADC_RESOLUTION_BITS    12
#define ADC_OVERSAMPLING       5 // oversampling factor 2⁵ = 32
#define ADC_GAIN               ADC_GAIN_1
#define ADC_MAX_VALUE          ((1 << ADC_RESOLUTION_BITS) - 1)

static uint16_t adc_samples_buffer[ADC_CH_COUNT];
static bool liquid_lens_enabled = false;

ret_code_t
liquid_set_target_current_ma(int32_t new_target_current_ma)
{

    int32_t clamped_target_current_ma =
        CLAMP(new_target_current_ma, LIQUID_LENS_MIN_CURRENT_MA,
              LIQUID_LENS_MAX_CURRENT_MA);

    if (clamped_target_current_ma != new_target_current_ma) {
        LOG_WRN("Clamp %" PRId32 "mA -> %" PRId32 "mA", new_target_current_ma,
                clamped_target_current_ma);
    }

    LOG_DBG("Setting target current to %" PRId32 " mA",
            clamped_target_current_ma);
    atomic_set(&target_current_ma, clamped_target_current_ma);

    return RET_SUCCESS;
}

static void
liquid_lens_set_pwm_percentage(int8_t percentage)
{
    LL_HRTIM_TIM_SetCompare2(
        HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE,
        (LIQUID_LENS_TIM_PERIOD_HALF +
         (LIQUID_LENS_TIM_PERIOD_HALF * (int32_t)percentage) / 100));
    LL_HRTIM_TIM_SetCompare2(
        HR_TIMER, LIQUID_LENS_TIM_NEG_BRIDGE,
        (LIQUID_LENS_TIM_PERIOD_HALF -
         (LIQUID_LENS_TIM_PERIOD_HALF * (int32_t)percentage) / 100));
}

// interrupt context!
static enum adc_action
adc_callback(const struct device *dev, const struct adc_sequence *sequence,
             uint16_t sampling_index)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(sequence);
    ARG_UNUSED(sampling_index);

    if (liquid_lens_is_enabled()) {
        const uint16_t stm32_vref_mv = voltage_measurement_get_vref_mv_from_raw(
            version_get_hardware_rev(), adc_samples_buffer[ADC_CH_VREFINT]);

        const int32_t current_amplifier_sig_mv =
            (int32_t)(((uint64_t)adc_samples_buffer[ADC_CH_INA240_SIG] *
                       (uint64_t)stm32_vref_mv) /
                      (uint64_t)(1 << 12));
        const int32_t current_amplifier_ref_mv =
            (int32_t)(((uint64_t)adc_samples_buffer[ADC_CH_INA240_REF] *
                       (uint64_t)stm32_vref_mv) /
                      (uint64_t)(1 << 12));

        const int32_t shunt_voltage_mv =
            current_amplifier_ref_mv - current_amplifier_sig_mv;

        const int32_t lens_current_ma =
            (int32_t)(((float)shunt_voltage_mv) /
                      liquid_lens_current_amplifier_gain /
                      liquid_lens_shunt_resistance_ohms);

        // PID control (currently just I.)
        // todo: The data type of control_output_pwm should be switched from
        // int8_t to float. Otherwise the I control part can only accumulate an
        // error if the error is quite large (9 mA right now). If the error is
        // lower it is rounded down to zero. For more details see
        // https://linear.app/worldcoin/issue/O-2064/liquid-lens-current-hysteresis
        const int32_t lens_current_error =
            atomic_get(&target_current_ma) - lens_current_ma;
        const int32_t ki = LIQUID_LENS_CONTROLLER_KI * 10000;
        const int32_t prev_control_output = control_output_pwm;
        control_output_pwm +=
            (int8_t)((int32_t)(lens_current_error * ki) / 10000);

        LOG_DBG(
            "current: %dmA; sig: %dmV; ref: %dmV; error: %dmA; output: %d%%",
            lens_current_ma, current_amplifier_sig_mv, current_amplifier_ref_mv,
            lens_current_error, control_output_pwm);

        control_output_pwm =
            CLAMP(control_output_pwm, -LIQUID_LENS_MAX_CONTROL_OUTPUT,
                  LIQUID_LENS_MAX_CONTROL_OUTPUT);
        if (prev_control_output != control_output_pwm) {
            liquid_lens_set_pwm_percentage(control_output_pwm);
        }
    }

    return ADC_ACTION_REPEAT;
}

_Noreturn static void
liquid_lens_thread()
{
    int err;

    struct adc_sequence_options sequence_options = {0};
    sequence_options.callback = adc_callback;
    sequence_options.interval_us = ADC_SAMPLING_PERIOD_US;
    sequence_options.user_data = NULL;

    struct adc_sequence sequence = {0};
    sequence.options = &sequence_options;
    sequence.channels = 0;
    sequence.buffer = (uint16_t *)adc_samples_buffer;
    sequence.buffer_size = sizeof(adc_samples_buffer);
    sequence.resolution = ADC_RESOLUTION_BITS;
    sequence.oversampling = ADC_OVERSAMPLING;
    sequence.calibrate = false;

    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        const struct adc_dt_spec *adc_channel = &adc_channels[i];
        if (adc_channel->dev == adc_dev) {
            sequence.channels |= BIT(adc_channel->channel_id);
        }
    }

    while (1) {
        // adc_read should block forever because the callback functions always
        // requests a repetition of the sample
        err = adc_read((const struct device *)adc_dev, &sequence);
        LOG_ERR("should not be reached, err = %d", err);

        // repeat adc_read after 1 second
        k_sleep(K_MSEC(1000));
    }
}

void
liquid_lens_enable(void)
{
    if (liquid_lens_is_enabled()) {
        return;
    }

    LOG_INF("Enabling liquid lens current");
    LL_HRTIM_EnableOutput(
        HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT | LIQUID_LENS_TIM_HS2_OUTPUT |
                      LIQUID_LENS_TIM_LS1_OUTPUT | LIQUID_LENS_TIM_HS1_OUTPUT);
    LL_HRTIM_TIM_CounterEnable(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE |
                                             LIQUID_LENS_TIM_NEG_BRIDGE);
    int ret = gpio_pin_set_dt(&liquid_lens_en, 1);
    ASSERT_SOFT(ret);

    liquid_lens_enabled = true;
}

void
liquid_lens_disable(void)
{
    if (!liquid_lens_is_enabled()) {
        return;
    }

    liquid_lens_enabled = false;

    LOG_INF("Disabling liquid lens current");
    int ret = gpio_pin_set_dt(&liquid_lens_en, 0);
    ASSERT_SOFT(ret);

    LL_HRTIM_TIM_CounterDisable(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE |
                                              LIQUID_LENS_TIM_NEG_BRIDGE);
    LL_HRTIM_DisableOutput(
        HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT | LIQUID_LENS_TIM_HS2_OUTPUT |
                      LIQUID_LENS_TIM_LS1_OUTPUT | LIQUID_LENS_TIM_HS1_OUTPUT);
}

bool
liquid_lens_is_enabled(void)
{
    return liquid_lens_enabled;
}

ret_code_t
liquid_lens_init(const orb_mcu_Hardware *hw_version)
{
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    UNUSED_PARAMETER(hw_version);
#endif

    const struct device *clk;
    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
    int err_code;

#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (hw_version->version ==
        orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        liquid_lens_current_amplifier_gain =
            DT_PROP_BY_IDX(DT_PATH(liquid_lens), amplifier_gains, 1);
        liquid_lens_shunt_resistance_ohms = DT_STRING_UNQUOTED_BY_IDX(
            DT_PATH(liquid_lens), shunt_resistor_ohms, 1);
    } else {
#endif
        liquid_lens_current_amplifier_gain =
            DT_PROP_BY_IDX(DT_PATH(liquid_lens), amplifier_gains, 0);
        liquid_lens_shunt_resistance_ohms = DT_STRING_UNQUOTED_BY_IDX(
            DT_PATH(liquid_lens), shunt_resistor_ohms, 0);
#if defined(CONFIG_BOARD_PEARL_MAIN)
    }
#endif

    err_code = clock_control_on(clk, &liquid_lens_hrtim_pclken);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    err_code = gpio_pin_configure_dt(&liquid_lens_en, GPIO_OUTPUT_INACTIVE);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    err_code = pinctrl_apply_state(
        PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(liquid_lens)),
        PINCTRL_STATE_DEFAULT);
    if (err_code < 0) {
        LOG_ERR("Liquid lens pinctrl setup failed");
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    LL_HRTIM_ConfigDLLCalibration(HR_TIMER,
                                  LL_HRTIM_DLLCALIBRATION_MODE_CONTINUOUS,
                                  LL_HRTIM_DLLCALIBRATION_RATE_3);
    LL_HRTIM_StartDLLCalibration(HR_TIMER);

    while (!LL_HRTIM_IsActiveFlag_DLLRDY(HR_TIMER))
        ;

    LL_HRTIM_TIM_SetPrescaler(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE,
                              LL_HRTIM_PRESCALERRATIO_MUL32);
    LL_HRTIM_TIM_SetCounterMode(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE,
                                LL_HRTIM_MODE_CONTINUOUS);
    LL_HRTIM_TIM_SetPeriod(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE, 0x3300);
    LL_HRTIM_TIM_SetPrescaler(HR_TIMER, LIQUID_LENS_TIM_NEG_BRIDGE,
                              LL_HRTIM_PRESCALERRATIO_MUL32);
    LL_HRTIM_TIM_SetCounterMode(HR_TIMER, LIQUID_LENS_TIM_NEG_BRIDGE,
                                LL_HRTIM_MODE_CONTINUOUS);
    LL_HRTIM_TIM_SetPeriod(HR_TIMER, LIQUID_LENS_TIM_NEG_BRIDGE, 0x3300);

    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT,
                             LL_HRTIM_OUT_POSITIVE_POLARITY);

    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LIQUID_LENS_TIM_HS2_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(HR_TIMER, LIQUID_LENS_TIM_HS2_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(HR_TIMER, LIQUID_LENS_TIM_HS2_OUTPUT,
                             LL_HRTIM_OUT_NEGATIVE_POLARITY);

    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LIQUID_LENS_TIM_LS1_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(HR_TIMER, LIQUID_LENS_TIM_LS1_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(HR_TIMER, LIQUID_LENS_TIM_LS1_OUTPUT,
                             LL_HRTIM_OUT_POSITIVE_POLARITY);

    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LIQUID_LENS_TIM_HS1_OUTPUT,
                                 LL_HRTIM_OUTPUTSET_TIMCMP2);
    LL_HRTIM_OUT_SetOutputResetSrc(HR_TIMER, LIQUID_LENS_TIM_HS1_OUTPUT,
                                   LL_HRTIM_OUTPUTRESET_TIMCMP1);
    LL_HRTIM_OUT_SetPolarity(HR_TIMER, LIQUID_LENS_TIM_HS1_OUTPUT,
                             LL_HRTIM_OUT_NEGATIVE_POLARITY);

    LL_HRTIM_TIM_SetCompare1(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE, 0);
    LL_HRTIM_TIM_SetCompare1(HR_TIMER, LIQUID_LENS_TIM_NEG_BRIDGE, 0);
    liquid_lens_set_pwm_percentage(0);

    LL_HRTIM_TIM_EnablePreload(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE |
                                             LIQUID_LENS_TIM_NEG_BRIDGE);

    /* Configure channels individually prior to sampling. */
    for (size_t i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
        if (!device_is_ready(adc_channels[i].dev)) {
            LOG_DBG("ADC controller device %s not ready\n",
                    adc_channels[i].dev->name);
            ASSERT_SOFT(RET_ERROR_INTERNAL);
            return RET_ERROR_INTERNAL;
        }

        int ret = adc_channel_setup_dt(&adc_channels[i]);
        if (ret < 0) {
            LOG_DBG("Could not setup channel #%d (%d)\n", i, ret);
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }
    }

    /* /!\ hardcoded */
    /* Do not remove existing paths so read value first */
    uint32_t path =
        LL_ADC_GetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC3));
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC3),
                                   path | LL_ADC_PATH_INTERNAL_VREFINT);

    thread_id =
        k_thread_create(&liquid_lens_thread_data, liquid_lens_stack_area,
                        K_THREAD_STACK_SIZEOF(liquid_lens_stack_area),
                        (k_thread_entry_t)liquid_lens_thread, NULL, NULL, NULL,
                        THREAD_PRIORITY_LIQUID_LENS, 0, K_NO_WAIT);
    k_thread_name_set(thread_id, "liquid_lens");

    return RET_SUCCESS;
}

#ifdef CONFIG_ZTEST

#include <zephyr/ztest.h>

// Test CRC speed over entire secondary slot (external flash on Diamond)
ZTEST(hil, test_liquid_lens)
{
    int32_t target;

    // ensure clamping works
    liquid_set_target_current_ma(500);
    target = atomic_get(&target_current_ma);
    zassert_equal(target, 400,
                  "liquid_lens: target_current_ma not clamped to 400");

    liquid_set_target_current_ma(-500);
    target = atomic_get(&target_current_ma);
    zassert_equal(target, -400,
                  "liquid_lens: target_current_ma not clamped to -400");

    // reset to 0
    liquid_set_target_current_ma(0);
    target = atomic_get(&target_current_ma);
    zassert_equal(target, 0, "liquid_lens: target_current_ma not set");

    liquid_lens_enable();
    k_msleep(10);
    int8_t prev_pwm = control_output_pwm;

    // check that PWM value changes with new target current
    liquid_set_target_current_ma(50);
    target = atomic_get(&target_current_ma);
    zassert_equal(target, 50, "liquid_lens: target_current_ma not set");

    k_msleep(10);
    zassert_not_equal(control_output_pwm, prev_pwm,
                      "liquid_lens: pwm didn't change even though "
                      "target_current_ma increased from 0 to 50");

    // check that PWM value is stable
    prev_pwm = control_output_pwm;

    k_msleep(10);
    zassert_equal(control_output_pwm, prev_pwm,
                  "liquid_lens: pwm didn't stabilize");

    liquid_lens_disable();
}

#endif
