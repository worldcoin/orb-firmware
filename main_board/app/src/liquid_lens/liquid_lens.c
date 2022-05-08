#include "liquid_lens.h"
#include <app_assert.h>
#include <app_config.h>
#include <device.h>
#include <drivers/clock_control/stm32_clock_control.h>
#include <drivers/gpio.h>
#include <drivers/pinctrl.h>
#include <logging/log.h>
#include <stdlib.h>
#include <stm32_ll_adc.h>
#include <stm32_ll_dma.h>
#include <stm32_ll_hrtim.h>
#include <stm32_ll_rcc.h>
#include <sys_clock.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(liquid_lens);

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

#define LIQUID_LENS_SHUNT_RESISTANCE_OHM 0.15f

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

#define LIQUID_LENS_SHUNT_RESISTANCE   0.15f // Ohm
#define LIQUID_LENS_MAX_CONTROL_OUTPUT 99    //%

#define DT_INST_CLK(inst)                                                      \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(inst, bus), .enr = DT_CLOCKS_CELL(inst, bits)    \
    }

static volatile uint16_t samples[LIQUID_LENS_ADC_NUM_CONVERSION_SAMPLES];
static volatile int32_t target_current = 0;
static volatile int8_t prev_pwm = 0;

static K_THREAD_STACK_DEFINE(liquid_lens_stack_area,
                             THREAD_STACK_SIZE_LIQUID_LENS);
static struct k_thread thread_data;
static k_tid_t thread_id;

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

void
liquid_set_target_current_ma(int32_t new_target_current)
{
    target_current = new_target_current;
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
            continue;
        }

        if (LL_ADC_REG_IsConversionOngoing(ADC)) {
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
        uint32_t vref_voltage = __LL_ADC_CALC_VREFANALOG_VOLTAGE(
            averages[2], LIQUID_LENS_ADC_RESOLUTION);
        int32_t sig_voltage = ((uint64_t)averages[0] * (uint64_t)vref_voltage) /
                              (uint64_t)(1 << 12);
        int32_t ref_voltage = ((uint64_t)averages[1] * (uint64_t)vref_voltage) /
                              (uint64_t)(1 << 12);
        int32_t shunt_voltage = ref_voltage - sig_voltage;
        int32_t lens_current =
            ((float)shunt_voltage) / 20.0f / LIQUID_LENS_SHUNT_RESISTANCE;
        /* LOG_INF("lens_current: %d", lens_current); */

        // PID control (currently just I.)
        int32_t lens_current_error = target_current - lens_current;
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
    gpio_pin_set(liquid_lens_en, LIQUID_LENS_EN_PIN, 1);
}

void
liquid_lens_disable(void)
{
    if (!liquid_lens_is_enabled()) {
        return;
    }

    LOG_INF("Disabling liquid lens current");
    gpio_pin_set(liquid_lens_en, LIQUID_LENS_EN_PIN, 0);
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
liquid_lens_init(void)
{
    const struct device *clk;
    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);
    int err_code;

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

    err_code = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(adc3)),
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

    thread_id = k_thread_create(&thread_data, liquid_lens_stack_area,
                                K_THREAD_STACK_SIZEOF(liquid_lens_stack_area),
                                liquid_lens_thread, NULL, NULL, NULL,
                                THREAD_PRIORITY_LIQUID_LENS, 0, K_NO_WAIT);

    return RET_SUCCESS;
}
