#include "liquid_lens.h"
#include "logs_can.h"
#include <app_assert.h>
#include <app_config.h>
#include <stdlib.h>
#include <stm32_ll_adc.h>
#include <stm32_ll_dma.h>
#include <stm32_ll_hrtim.h>
#include <stm32_ll_rcc.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
LOG_MODULE_REGISTER(liquid_lens, CONFIG_LIQUID_LENS_LOG_LEVEL);

#define HR_TIMER         (HRTIM_TypeDef *)HRTIM1_BASE
#define ADC              (ADC_TypeDef *)ADC3_BASE
#define ADC_COMMON       (ADC_Common_TypeDef *)ADC345_COMMON_BASE
#define DMA              (DMA_TypeDef *)DMA2_BASE
#define DMA_CHANNEL      LL_DMA_CHANNEL_1
#define DMA_CHANNEL_IRQn DMA2_Channel1_IRQn
#define DMAMUX_REQ_ADC   LL_DMAMUX_REQ_ADC3

#define LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US 1000
#define LIQUID_LENS_CONTROLLER_KI                                              \
    ((float)LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US / 10000.0f)

#define LIQUID_LENS_EN_NODE  DT_PATH(liquid_lens_en)
#define LIQUID_LENS_EN_CTLR  DT_GPIO_CTLR(LIQUID_LENS_EN_NODE, gpios)
#define LIQUID_LENS_EN_PIN   DT_GPIO_PIN(LIQUID_LENS_EN_NODE, gpios)
#define LIQUID_LENS_EN_FLAGS DT_GPIO_FLAGS(LIQUID_LENS_EN_NODE, gpios)

#define LIQUID_LENS_TIM_PERIOD      0x3300
#define LIQUID_LENS_TIM_PERIOD_HALF (LIQUID_LENS_TIM_PERIOD / 2) // 0x1980
#define LIQUID_LENS_TIM_POS_BRIDGE  LL_HRTIM_TIMER_B
#define LIQUID_LENS_TIM_NEG_BRIDGE  LL_HRTIM_TIMER_A
#define LIQUID_LENS_TIM_HS1_OUTPUT  LL_HRTIM_OUTPUT_TB2
#define LIQUID_LENS_TIM_LS1_OUTPUT  LL_HRTIM_OUTPUT_TB1
#define LIQUID_LENS_TIM_HS2_OUTPUT  LL_HRTIM_OUTPUT_TA2
#define LIQUID_LENS_TIM_LS2_OUTPUT  LL_HRTIM_OUTPUT_TA1

#define LIQUID_LENS_DMA_NODE DT_NODELABEL(dma2)

#define LIQUID_LENS_ADC_NUM_CHANNELS            3
#define LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL 4
#define LIQUID_LENS_ADC_NUM_CONVERSION_SAMPLES                                 \
    (LIQUID_LENS_ADC_NUM_CHANNELS * LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL)
#define LIQUID_LENS_ADC_CHANNEL_INA240_REF LL_ADC_CHANNEL_10
#define LIQUID_LENS_ADC_CHANNEL_INA240_SIG LL_ADC_CHANNEL_11
#define LIQUID_LENS_ADC_SAMPLING_TIME      LL_ADC_SAMPLINGTIME_47CYCLES_5
#define LIQUID_LENS_ADC_CLOCK_PRESCALER    LL_ADC_CLOCK_SYNC_PCLK_DIV4
#define LIQUID_LENS_ADC_RESOLUTION         LL_ADC_RESOLUTION_12B

#define LIQUID_LENS_MAX_CONTROL_OUTPUT 99 //%

#define DT_INST_CLK(inst)                                                      \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(inst, bus), .enr = DT_CLOCKS_CELL(inst, bits)    \
    }

static volatile uint16_t samples[LIQUID_LENS_ADC_NUM_CONVERSION_SAMPLES];
static atomic_t target_current_ma;
static volatile int8_t prev_pwm = 0;
static volatile float liquid_lens_current_amplifier_gain = DT_STRING_UNQUOTED(
    DT_PATH(zephyr_user), liquid_lens_current_amplifier_gain);
static volatile bool use_stm32_vrefint = true;

static K_THREAD_STACK_DEFINE(liquid_lens_stack_area,
                             THREAD_STACK_SIZE_LIQUID_LENS);
static struct k_thread liquid_lens_thread_data;
static k_tid_t thread_id = NULL;

static const struct device *dev_dma = DEVICE_DT_GET(LIQUID_LENS_DMA_NODE);
static const struct device *liquid_lens_en = DEVICE_DT_GET(LIQUID_LENS_EN_CTLR);

static struct stm32_pclken liquid_lens_hrtim_pclken =
    DT_INST_CLK(DT_NODELABEL(hrtim1));
static struct stm32_pclken liquid_lens_adc_pclken =
    DT_INST_CLK(DT_NODELABEL(adc3));
static struct stm32_pclken liquid_lens_dma_pclken =
    DT_INST_CLK(DT_NODELABEL(dma2));
static struct stm32_pclken liquid_lens_dmamux_pclken =
    DT_INST_CLK(DT_NODELABEL(dmamux1));

PINCTRL_DT_DEFINE(DT_NODELABEL(liquid_lens));
PINCTRL_DT_DEFINE(DT_NODELABEL(adc3));

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

static void
liquid_lens_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    for (;;) {
        k_sleep(K_USEC(LIQUID_LENS_DEFAULT_SAMPLING_PERIOD_US));

        if (!LL_ADC_IsEnabled(ADC)) {
            // thread will be waked up when enabling liquid lens
            k_sleep(K_FOREVER);
        }

        if (LL_ADC_REG_IsConversionOngoing(ADC)) {
            LL_ADC_REG_StopConversion(ADC);
            LOG_WRN("liquid lens ADC overrun");
            continue;
        }

        LL_DMA_ConfigAddresses(
            DMA, DMA_CHANNEL, (uint32_t)ADC + offsetof(ADC_TypeDef, DR),
            (uint32_t)samples, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
        LL_DMA_SetDataLength(DMA, DMA_CHANNEL,
                             LIQUID_LENS_ADC_NUM_CONVERSION_SAMPLES);
        LL_DMA_EnableChannel(DMA, DMA_CHANNEL);
        LL_ADC_REG_StartConversion(ADC);
    }
}

static int
compare_sample(const void *a, const void *b)
{
    return (*(uint16_t *)a - *(uint16_t *)b);
}

static void
dma_isr(const void *arg)
{
    uint32_t averages[LIQUID_LENS_ADC_NUM_CHANNELS];
    uint16_t samples_per_average[LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL];

    ARG_UNUSED(arg);
    if (LL_DMA_IsActiveFlag_TC1(DMA)) {
        LL_DMA_ClearFlag_TC1(DMA);
        LL_DMA_DisableChannel(DMA, DMA_CHANNEL);

        // Calculate averages of the samples.
        for (size_t i = 0; i < LIQUID_LENS_ADC_NUM_CHANNELS; i++) {
            for (size_t j = 0; j < LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL;
                 j++) {
                samples_per_average[j] =
                    samples[j * LIQUID_LENS_ADC_NUM_CHANNELS + i];
            }
            qsort(samples_per_average, LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL,
                  sizeof(uint16_t), compare_sample);
            uint32_t sum = 0;
            uint32_t first_quartile =
                (1 * LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL) / 4;
            uint32_t third_quartile =
                (3 * LIQUID_LENS_ADC_NUM_SAMPLES_PER_CHANNEL) / 4;
            for (uint32_t j = first_quartile; j < third_quartile; j++) {
                sum += samples_per_average[j];
            }
            averages[i] = sum / (third_quartile - first_quartile);
        }

        // Calculate the lens current.
        uint32_t stm32_vref_mv;

        if (use_stm32_vrefint) {
            // calculate the voltage at VREF+ pin from measurement of internal
            // reference voltage VREFINT
            stm32_vref_mv = __LL_ADC_CALC_VREFANALOG_VOLTAGE(
                averages[2], LIQUID_LENS_ADC_RESOLUTION);
        } else {
            // use fixed value for VREF+ from device tree
            stm32_vref_mv = DT_PROP(DT_PATH(zephyr_user), ev5_vref_mv);
        }

        int32_t current_amplifier_sig_mv =
            ((uint64_t)averages[0] * (uint64_t)stm32_vref_mv) /
            (uint64_t)(1 << 12);
        int32_t current_amplifier_ref_mv =
            ((uint64_t)averages[1] * (uint64_t)stm32_vref_mv) /
            (uint64_t)(1 << 12);
        int32_t shunt_voltage_mv =
            current_amplifier_ref_mv - current_amplifier_sig_mv;
        int32_t lens_current_ma =
            ((float)shunt_voltage_mv) / liquid_lens_current_amplifier_gain /
            DT_STRING_UNQUOTED(DT_PATH(zephyr_user),
                               liquid_lens_shunt_resistance_ohm);

        LOG_DBG("lens_current_ma: %d; sig_mV: %d; ref_mV: %d", lens_current_ma,
                current_amplifier_sig_mv, current_amplifier_ref_mv);

        // PID control (currently just I.)
        // todo: The data type of prev_pwm should be switched from int8_t to
        // float. Otherwise the I control part can only accumulate an error if
        // the error is quite large (9 mA right now). If the error is lower it
        // is rounded down to zero.
        // For more details see
        // https://linear.app/worldcoin/issue/O-2064/liquid-lens-current-hysteresis
        int32_t lens_current_error =
            atomic_get(&target_current_ma) - lens_current_ma;
        int32_t ki = LIQUID_LENS_CONTROLLER_KI * 10000;
        int32_t prev_control_output = prev_pwm;
        prev_pwm += (int32_t)(lens_current_error * ki) / 10000;
        if (prev_pwm < -LIQUID_LENS_MAX_CONTROL_OUTPUT) {
            prev_pwm = -LIQUID_LENS_MAX_CONTROL_OUTPUT;
        } else if (prev_pwm > LIQUID_LENS_MAX_CONTROL_OUTPUT) {
            prev_pwm = LIQUID_LENS_MAX_CONTROL_OUTPUT;
        }
        if (prev_control_output != prev_pwm) {
            liquid_lens_set_pwm_percentage(prev_pwm);
        }
    }
}

void
liquid_lens_enable(void)
{
    if (liquid_lens_is_enabled()) {
        return;
    }

    LOG_INF("Enabling liquid lens current");
    LL_ADC_ClearFlag_ADRDY(ADC);
    LL_ADC_Enable(ADC);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC)) {
    }
    LL_HRTIM_EnableOutput(
        HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT | LIQUID_LENS_TIM_HS2_OUTPUT |
                      LIQUID_LENS_TIM_LS1_OUTPUT | LIQUID_LENS_TIM_HS1_OUTPUT);
    LL_HRTIM_TIM_CounterEnable(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE |
                                             LIQUID_LENS_TIM_NEG_BRIDGE);
    int ret = gpio_pin_set(liquid_lens_en, LIQUID_LENS_EN_PIN, 1);
    ASSERT_SOFT(ret);

    if (thread_id != NULL) {
        k_wakeup(thread_id);
    } else {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
    }
}

void
liquid_lens_disable(void)
{
    if (!liquid_lens_is_enabled()) {
        return;
    }

    LOG_INF("Disabling liquid lens current");
    int ret = gpio_pin_set(liquid_lens_en, LIQUID_LENS_EN_PIN, 0);
    ASSERT_SOFT(ret);

    LL_HRTIM_TIM_CounterDisable(HR_TIMER, LIQUID_LENS_TIM_POS_BRIDGE |
                                              LIQUID_LENS_TIM_NEG_BRIDGE);
    LL_HRTIM_DisableOutput(
        HR_TIMER, LIQUID_LENS_TIM_LS2_OUTPUT | LIQUID_LENS_TIM_HS2_OUTPUT |
                      LIQUID_LENS_TIM_LS1_OUTPUT | LIQUID_LENS_TIM_HS1_OUTPUT);
    LL_ADC_Disable(ADC);
}

bool
liquid_lens_is_enabled(void)
{
    return LL_ADC_IsEnabled(ADC);
}

ret_code_t
liquid_lens_init(const Hardware *hw_version)
{
    const struct device *clk;
    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
    int err_code;

    if (hw_version->version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        liquid_lens_current_amplifier_gain = DT_STRING_UNQUOTED(
            DT_PATH(zephyr_user), liquid_lens_current_amplifier_gain_ev5);
        use_stm32_vrefint = false;
    } else {
        liquid_lens_current_amplifier_gain = DT_STRING_UNQUOTED(
            DT_PATH(zephyr_user), liquid_lens_current_amplifier_gain);
        use_stm32_vrefint = true;
    }

    err_code = clock_control_on(clk, &liquid_lens_hrtim_pclken);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    err_code = clock_control_on(clk, &liquid_lens_adc_pclken);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    err_code = clock_control_on(clk, &liquid_lens_dma_pclken);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    err_code = clock_control_on(clk, &liquid_lens_dmamux_pclken);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_NOT_INITIALIZED;
    }

    err_code = gpio_pin_configure(liquid_lens_en, LIQUID_LENS_EN_PIN,
                                  LIQUID_LENS_EN_FLAGS | GPIO_OUTPUT);
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

    err_code = pinctrl_apply_state(
        PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(adc3)), PINCTRL_STATE_DEFAULT);
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

    if (LL_ADC_IsDeepPowerDownEnabled(ADC)) {
        LL_ADC_DisableDeepPowerDown(ADC);
    }
    if (!LL_ADC_IsInternalRegulatorEnabled(ADC)) {
        LL_ADC_EnableInternalRegulator(ADC);
        k_busy_wait(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
        if (!LL_ADC_IsInternalRegulatorEnabled(ADC)) {
            LOG_ERR("liquid lens ADC internal voltage regulator failure");
            return RET_ERROR_BUSY;
        }
    }

    LL_ADC_CommonInitTypeDef adc_common_init;
    LL_ADC_CommonStructInit(&adc_common_init);
    adc_common_init.CommonClock = LIQUID_LENS_ADC_CLOCK_PRESCALER;
    if (LL_ADC_CommonInit(ADC_COMMON, &adc_common_init)) {
        LOG_ERR("liquid lens ADC Common initialization failed");
        return RET_ERROR_NOT_INITIALIZED;
    }

    LL_ADC_InitTypeDef adc_init;
    LL_ADC_StructInit(&adc_init);
    adc_init.Resolution = LIQUID_LENS_ADC_RESOLUTION;
    adc_init.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
    adc_init.LowPowerMode = LL_ADC_LP_MODE_NONE;
    if (LL_ADC_Init(ADC, &adc_init)) {
        LOG_ERR("liquid lens ADC initialization failed");
        return RET_ERROR_NOT_INITIALIZED;
    }

    LL_ADC_REG_InitTypeDef adc_reg_init;
    LL_ADC_REG_StructInit(&adc_reg_init);
    adc_reg_init.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
    adc_reg_init.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_3RANKS;
    adc_reg_init.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
    adc_reg_init.ContinuousMode = LL_ADC_REG_CONV_CONTINUOUS;
    adc_reg_init.DMATransfer = LL_ADC_REG_DMA_TRANSFER_LIMITED;
    adc_reg_init.Overrun = LL_ADC_REG_OVR_DATA_OVERWRITTEN;
    if (LL_ADC_REG_Init(ADC, &adc_reg_init)) {
        LOG_ERR("liquid lens ADC Regular initialization failed");
        return RET_ERROR_NOT_INITIALIZED;
    }

    LL_ADC_SetCommonPathInternalCh(ADC_COMMON, LL_ADC_PATH_INTERNAL_VREFINT);
    LL_ADC_SetSamplingTimeCommonConfig(ADC, LL_ADC_SAMPLINGTIME_COMMON_DEFAULT);
    LL_ADC_REG_SetSequencerRanks(ADC, LL_ADC_REG_RANK_1,
                                 LIQUID_LENS_ADC_CHANNEL_INA240_SIG);
    LL_ADC_SetChannelSamplingTime(ADC, LIQUID_LENS_ADC_CHANNEL_INA240_SIG,
                                  LIQUID_LENS_ADC_SAMPLING_TIME);
    LL_ADC_REG_SetSequencerRanks(ADC, LL_ADC_REG_RANK_2,
                                 LIQUID_LENS_ADC_CHANNEL_INA240_REF);
    LL_ADC_SetChannelSamplingTime(ADC, LIQUID_LENS_ADC_CHANNEL_INA240_REF,
                                  LIQUID_LENS_ADC_SAMPLING_TIME);
    LL_ADC_REG_SetSequencerRanks(ADC, LL_ADC_REG_RANK_3,
                                 LL_ADC_CHANNEL_VREFINT);
    LL_ADC_SetChannelSamplingTime(ADC, LL_ADC_CHANNEL_VREFINT,
                                  LIQUID_LENS_ADC_SAMPLING_TIME);

    LL_ADC_StartCalibration(ADC, LL_ADC_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(ADC)) {
    }

    if (!device_is_ready(dev_dma)) {
        LOG_ERR("liquid lens DMA device not ready");
        return RET_ERROR_BUSY;
    }

    LL_DMA_InitTypeDef dma_init;
    LL_DMA_StructInit(&dma_init);
    dma_init.Mode = LL_DMA_MODE_NORMAL;
    dma_init.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_init.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_init.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    dma_init.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
    dma_init.PeriphRequest = DMAMUX_REQ_ADC;
    dma_init.Priority = LL_DMA_PRIORITY_HIGH;
    if (LL_DMA_Init(DMA, DMA_CHANNEL, &dma_init)) {
        LOG_ERR("liquid lens DMA initialization failed");
        return RET_ERROR_NOT_INITIALIZED;
    }

    LL_DMA_EnableIT_TC(DMA, DMA_CHANNEL);

    irq_disable(DMA_CHANNEL_IRQn);
    irq_connect_dynamic(DMA_CHANNEL_IRQn, 1, &dma_isr, NULL, 0);
    irq_enable(DMA_CHANNEL_IRQn);

    thread_id = k_thread_create(
        &liquid_lens_thread_data, liquid_lens_stack_area,
        K_THREAD_STACK_SIZEOF(liquid_lens_stack_area), liquid_lens_thread, NULL,
        NULL, NULL, THREAD_PRIORITY_LIQUID_LENS, 0, K_NO_WAIT);
    k_thread_name_set(thread_id, "liquid_lens");

    return RET_SUCCESS;
}
