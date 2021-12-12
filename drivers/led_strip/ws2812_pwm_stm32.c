/*
 * Copyright (c) 2021 Tools for Humanity
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT worldsemi_ws2812_pwm_stm32

#include <device.h>
#include <drivers/led_strip.h>
#include <init.h>
#include <soc.h>
#include <stm32_ll_dma.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <sys/util_macro.h>
#include <zephyr.h>

#include <drivers/clock_control/stm32_clock_control.h>
#include <pinmux/pinmux_stm32.h>

#define LOG_LEVEL_CONFIG CONFIG_LED_STRIP_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(worldsemi_ws2812_pwm_stm32);

#define WS2812_PERIOD_NS       1250
#define WS2812_PERIOD_1_BIT_NS 600
#define WS2812_PERIOD_0_BIT_NS 300
#define NUM_RESET_PIXELS       41

#if defined(CONFIG_SOC_SERIES_STM32F3X) ||                                     \
    defined(CONFIG_SOC_SERIES_STM32F7X) ||                                     \
    defined(CONFIG_SOC_SERIES_STM32G0X) ||                                     \
    defined(CONFIG_SOC_SERIES_STM32G4X) ||                                     \
    defined(CONFIG_SOC_SERIES_STM32H7X) ||                                     \
    defined(CONFIG_SOC_SERIES_STM32L4X) ||                                     \
    defined(CONFIG_SOC_SERIES_STM32MP1X) ||                                    \
    defined(CONFIG_SOC_SERIES_STM32WBX)
#define TIMER_HAS_6CH 1
#else
#define TIMER_HAS_6CH 0
#endif

/** Maximum number of timer channels. */
#if TIMER_HAS_6CH
#define TIMER_MAX_CH 6u
#else
#define TIMER_MAX_CH 4u
#endif

// Mapping from timer channel given in DTB to STM32 HAL LL values
static const uint32_t timer_ch2ll[TIMER_MAX_CH] = {
    LL_TIM_CHANNEL_CH1, LL_TIM_CHANNEL_CH2, LL_TIM_CHANNEL_CH3,
    LL_TIM_CHANNEL_CH4,
#if TIMER_HAS_6C
    LL_TIM_CHANNEL_CH5, LL_TIM_CHANNEL_CH6
#endif
};

// Mapping from timer channel given in DTB to STM32 HAL LL values
static const uint32_t timer_ch2ccr_offset[TIMER_MAX_CH] = {
    offsetof(TIM_TypeDef, CCR1),
    offsetof(TIM_TypeDef, CCR2),
    offsetof(TIM_TypeDef, CCR3),
    offsetof(TIM_TypeDef, CCR4),
#if TIMER_HAS_6C
    offsetof(TIM_TypeDef, CCR5),
    offsetof(TIM_TypeDef, CCR6)
#endif
};

// Mapping from timer number given in DTB to STM32 HAL LL values
static const uint32_t timer_number2dma_up_event[] = {
    [0] = LL_DMAMUX_REQ_TIM1_UP,   [1] = LL_DMAMUX_REQ_TIM2_UP,
    [2] = LL_DMAMUX_REQ_TIM3_UP,   [3] = LL_DMAMUX_REQ_TIM4_UP,
    [4] = LL_DMAMUX_REQ_TIM5_UP,   [7] = LL_DMAMUX_REQ_TIM8_UP,
    [14] = LL_DMAMUX_REQ_TIM15_UP, [15] = LL_DMAMUX_REQ_TIM16_UP,
    [16] = LL_DMAMUX_REQ_TIM17_UP, [19] = LL_DMAMUX_REQ_TIM20_UP};

#if defined(DMA1_Channel8)
#define DMA_MAX_CH 8
#elif defined(DMA1_Channel7)
#define DMA_MAX_CH 7
#else
#define DMA_MAX_CH 6
#endif

// Mapping from DMA channel given in DTB to STM32 HAL LL values
static const uint32_t dma_ch2ll[DMA_MAX_CH] = {
    LL_DMA_CHANNEL_1, LL_DMA_CHANNEL_2, LL_DMA_CHANNEL_3,
    LL_DMA_CHANNEL_4, LL_DMA_CHANNEL_5, LL_DMA_CHANNEL_6,
#ifdef DMA1_CHANNEL7
    LL_DMA_CHANNEL_7,
#endif
#ifdef DMA1_CHANNEL8
    LL_DMA_CHANNEL_8
#endif
};

// Mapping from DMA channel given in DTB to STM32 HAL LL values
static const uint32_t dma_ch2TC_clear_flag[DMA_MAX_CH] = {
    DMA_IFCR_CTCIF1, DMA_IFCR_CTCIF2, DMA_IFCR_CTCIF3,
    DMA_IFCR_CTCIF4, DMA_IFCR_CTCIF5, DMA_IFCR_CTCIF6,
#ifdef DMA1_CHANNEL7
    DMA_IFCR_CTCIF7,
#endif
#ifdef DMA1_CHANNEL8
    DMA_IFCR_CTCIF8
#endif
};

struct ws2812_pwm_stm32_data {
    // To hold clock frequency. We get this dynamically from the clock subsystem
    uint32_t tim_clk;
    // This points to an array with one byte for each bit
    uint8_t *pixel_bits;
    // Tells the timer and dma interrupt to stop after one update
    bool one_shot;
    // For the generic API update function to be able to wait for LEDs to update
    struct k_sem update_sem;
};

struct ws2812_pwm_stm32_config {
    // must substract 1 when looking up low level equivalent
    uint32_t timer_channel;
    // must substract 1 when looking up low level equivalent
    uint32_t timer_number;
    // must substract 1 when looking up low level equivalent
    uint16_t timer_up_irq_num;
    TIM_TypeDef *timer;
    // must substract 1 when looking up low level equivalent
    uint32_t dma_channel;
    DMA_TypeDef *dma;
    struct stm32_pclken pclken;
    const struct soc_gpio_pinctrl *pinctrl;
    // Used as a maximum check
    uint32_t num_leds;
};

static inline uint32_t
nsec_to_cycles(uint32_t ns, struct ws2812_pwm_stm32_data *data)
{
    uint32_t cycles_per_second = data->tim_clk;

    return (ns * (uint64_t)cycles_per_second) / NSEC_PER_SEC;
}

// Theory of operation:
//
// NOTE: We refer to each LED is a "pixel" sometimes.
//
// We are given pixel values in RGB form, with one byte for each color, so three
// bytes per pixel Each bit that is sent to the pixels is encoded as a
// particular duty cycle with an 800kHz frequency. This means that we need to
// continuously change the CCR (capture/compare register) of a timer channel
// after each PWM period. We do this by using DMA to update CCR every time the
// timer rolls over, which is an UPDATE event in timer terms. As a consequence,
// we need to expand every bit of the RGB values into a byte, since DMA works on
// bytes as the smallest unit. This means for X number of pixels, we need (8
// bytes for red) + (8 bytes for green) + (8 bytes for blue) = 24 So X * 24
//
// The first pixel waits for a reset signal, which is just a 0% duty cycle for
// at least 50us, or 40 cycles at 800KHz (1.25us period)
//
// After this, the first pixel receives 24 bits and updates its color
// accordingly. Then the pixel will amplify and retransmit every subsequent set
// of 24 bits until it sees a reset signal again. Each pixel down the line does
// the same, This is why pixels are chained together in series.
//
// The pixels expect bits in the following order:
//
// G7 G6 G5 G4 G3 G2 G1 G0 | R7 R6 R5 R4 R3 R2 R1 R0 | B7 B6 B5 B4 B3 B2 B1 B0
//
// I.e., we send green, red, and blue

static void
rgb_to_dma_pixels(const struct device *dev, struct led_rgb *pixels,
                  size_t num_pixels)
{
    struct ws2812_pwm_stm32_data *data = dev->data;

    const uint8_t zero_bit = nsec_to_cycles(WS2812_PERIOD_0_BIT_NS, data);
    const uint8_t one_bit = nsec_to_cycles(WS2812_PERIOD_1_BIT_NS, data);

    size_t i, j;
    for (i = 0; i < num_pixels; ++i) {
        for (j = 0; j < 8; ++j) {
            data->pixel_bits[NUM_RESET_PIXELS + i * 24 + j] =
                (pixels[i].g & (0x80 >> j)) ? one_bit : zero_bit;
        }
        for (j = 8; j < 16; ++j) {
            data->pixel_bits[NUM_RESET_PIXELS + i * 24 + j] =
                (pixels[i].r & (0x80 >> (j - 8))) ? one_bit : zero_bit;
        }
        for (j = 16; j < 24; ++j) {
            data->pixel_bits[NUM_RESET_PIXELS + i * 24 + j] =
                (pixels[i].b & (0x80 >> (j - 16))) ? one_bit : zero_bit;
        }
    }
}

static int
ws2812_pwm_stm32_update_rgb(const struct device *dev, struct led_rgb *pixels,
                            size_t num_pixels)
{
    (void)pixels;
    const struct ws2812_pwm_stm32_config *config = dev->config;
    struct ws2812_pwm_stm32_data *data = dev->data;
    const uint32_t dma_channel = dma_ch2ll[config->dma_channel - 1u];

    if (num_pixels > config->num_leds) {
        LOG_ERR("too many pixels given: %u, max: %u", num_pixels,
                config->num_leds);
        return -EINVAL;
    }

    // Datasheet says that to reconfigure a channel,
    // we must first disable the issuer of events and then the DMA channel
    // itself. So, first disable the timer and then the DMA channel

    LL_TIM_DisableCounter(config->timer);
    rgb_to_dma_pixels(dev, pixels, num_pixels);
    data->one_shot = true;
    LL_DMA_DisableChannel(config->dma, dma_channel);
    LL_DMA_SetDataLength(config->dma, dma_channel,
                         NUM_RESET_PIXELS + num_pixels * 24);
    // We use the DMA complete interrupt to turn on the timer interrupt so that
    // when the timer outputs its last PWM cycle, it can switch itself off.
    // If you try to switch off the timer directly in the DMA complete ISR,
    // then you will kill the timer before it can output its final cycle and
    // thus the timer will drop a bit.
    LL_DMA_EnableIT_TC(config->dma, dma_channel);
    LL_TIM_SetPrescaler(config->timer, 0);
    LL_TIM_SetAutoReload(config->timer,
                         nsec_to_cycles(WS2812_PERIOD_NS, data) - 1u);
    LL_TIM_CC_EnableChannel(config->timer,
                            timer_ch2ll[config->timer_channel - 1u]);
    LL_TIM_EnableDMAReq_UPDATE(config->timer);
    LL_TIM_DisableIT_UPDATE(config->timer);
    LL_DMA_EnableChannel(config->dma, dma_channel);

    // We need to trigger an event so that the timer's CCR register
    // (representing the PWM duty cycle) is loaded with the first DMA-provided
    // value before we start the timer. If we don't do this, then when the timer
    // starts, the first duty cycle it has is indeterminant
    LL_TIM_GenerateEvent_UPDATE(config->timer);
    LL_TIM_EnableCounter(config->timer);

    // Wait until the LEDs have finished updating
    // That way the caller of this function doesn't get missed updates by
    // prematurely overwriting the data from the previous update before it is
    // finished. with 60 pixels it can take 2ms to update all of them.
    // Calculation: (1.25us per bit @ 24 bits per pixel):
    // (1.25 * 24) * 60 + (50us reset pulse) = 1.850ms
    if (k_sem_take(&data->update_sem, K_FOREVER)) {
        LOG_ERR("semaphore was reset during the waiting period, but we never "
                "expect this to happen");
        return -EAGAIN;
    }
    return 0;
}

// This isn't implemented

static int
ws2812_pwm_stm32_update_channels(const struct device *dev, uint8_t *channels,
                                 size_t num_channels)
{
    (void)channels;
    (void)num_channels;
    return -ENOTSUP;
}

static const struct led_strip_driver_api ws2812_pwm_stm32_driver_api = {
    .update_rgb = ws2812_pwm_stm32_update_rgb,
    .update_channels = ws2812_pwm_stm32_update_channels};

/**
 * Obtain timer clock speed.
 *
 * @param pclken  Timer clock control subsystem.
 * @param tim_clk Where computed timer clock will be stored.
 *
 * @return 0 on success, error code otherwise.
 */
static int
get_tim_clk(const struct stm32_pclken *pclken, uint32_t *tim_clk)
{
    int r;
    const struct device *clk;
    uint32_t bus_clk, apb_psc;

    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

    r = clock_control_get_rate(clk, (clock_control_subsys_t *)pclken, &bus_clk);
    if (r < 0) {
        return r;
    }

#if defined(CONFIG_SOC_SERIES_STM32H7X)
    if (pclken->bus == STM32_CLOCK_BUS_APB1) {
        apb_psc = STM32_D2PPRE1;
    } else {
        apb_psc = STM32_D2PPRE2;
    }
#else
    if (pclken->bus == STM32_CLOCK_BUS_APB1) {
        apb_psc = STM32_APB1_PRESCALER;
    }
#if !defined(CONFIG_SOC_SERIES_STM32F0X) && !defined(CONFIG_SOC_SERIES_STM32G0X)
    else {
        apb_psc = STM32_APB2_PRESCALER;
    }
#endif
#endif
#if defined(RCC_DCKCFGR_TIMPRE) || defined(RCC_DCKCFGR1_TIMPRE) ||             \
    defined(RCC_CFGR_TIMPRE)
    /*
     * There are certain series (some F4, F7 and H7) that have the TIMPRE
     * bit to control the clock frequency of all the timers connected to
     * APB1 and APB2 domains.
     *
     * Up to a certain threshold value of APB{1,2} prescaler, timer clock
     * equals to HCLK. This threshold value depends on TIMPRE setting
     * (2 if TIMPRE=0, 4 if TIMPRE=1). Above threshold, timer clock is set
     * to a multiple of the APB domain clock PCLK{1,2} (2 if TIMPRE=0, 4 if
     * TIMPRE=1).
     */

    if (LL_RCC_GetTIMPrescaler() == LL_RCC_TIM_PRESCALER_TWICE) {
        /* TIMPRE = 0 */
        if (apb_psc <= 2u) {
            LL_RCC_ClocksTypeDef clocks;

            LL_RCC_GetSystemClocksFreq(&clocks);
            *tim_clk = clocks.HCLK_Frequency;
        } else {
            *tim_clk = bus_clk * 2u;
        }
    } else {
        /* TIMPRE = 1 */
        if (apb_psc <= 4u) {
            LL_RCC_ClocksTypeDef clocks;

            LL_RCC_GetSystemClocksFreq(&clocks);
            *tim_clk = clocks.HCLK_Frequency;
        } else {
            *tim_clk = bus_clk * 4u;
        }
    }
#else
    /*
     * If the APB prescaler equals 1, the timer clock frequencies
     * are set to the same frequency as that of the APB domain.
     * Otherwise, they are set to twice (×2) the frequency of the
     * APB domain.
     */
    if (apb_psc == 1u) {
        *tim_clk = bus_clk;
    } else {
        *tim_clk = bus_clk * 2u;
    }
#endif

    return 0;
}

static void
timer_wait_isr(const void *arg)
{
    const struct device *dev = arg;
    const struct ws2812_pwm_stm32_config *config = dev->config;
    struct ws2812_pwm_stm32_data *data = dev->data;

    LL_TIM_ClearFlag_UPDATE(config->timer);

    if (data->one_shot) {
        LL_TIM_DisableCounter(config->timer);
        k_sem_give(&data->update_sem);
        return;
    }
}

static void
dma_complete_isr(const void *arg)
{
    const struct device *dev = arg;
    const struct ws2812_pwm_stm32_config *config = dev->config;
    struct ws2812_pwm_stm32_data *data = dev->data;

    // clear transfer complete interrupt flag
    // This is the equivalent of calling LL_DMA_ClearFlag_TC1(config->dma),
    // but parameterized by channel
    WRITE_REG(config->dma->IFCR,
              dma_ch2TC_clear_flag[config->dma_channel - 1u]);

    if (data->one_shot) {
        LL_TIM_EnableIT_UPDATE(config->timer);
        return;
    }
}

static int
ws2812_pwm_stm32_init(const struct device *dev)
{
    struct ws2812_pwm_stm32_data *data = dev->data;
    const struct ws2812_pwm_stm32_config *config = dev->config;
    const struct device *clk;
    LL_TIM_InitTypeDef init;
    LL_TIM_OC_InitTypeDef oc_init;
    DMA_TypeDef *dma = config->dma;
    uint32_t timer_channel;
    uint32_t dma_channel;
    uint32_t dma_mux_periph_request;
    int r;

    LL_TIM_DisableCounter(config->timer);

    r = k_sem_init(&data->update_sem, 0, 1);
    if (r < 0) {
        LOG_ERR("Error initializing semaphore!");
        return r;
    }

    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

    // Enable the clock to the timer
    r = clock_control_on(clk, (clock_control_subsys_t *)&config->pclken);
    if (r < 0) {
        LOG_ERR("Could not initialize clock (%d)", r);
        return r;
    }

    r = get_tim_clk(&config->pclken, &data->tim_clk);
    if (r < 0) {
        LOG_ERR("Could not obtain timer clock (%d)", r);
        return r;
    }

    memset(data->pixel_bits, 0, NUM_RESET_PIXELS);

    // configure GPIO pin alternate function
    r = stm32_dt_pinctrl_configure(config->pinctrl, 1, (uint32_t)config->timer);
    if (r < 0) {
        LOG_ERR("PWM pinctrl setup failed (%d)", r);
        return r;
    }

    // Initialize the timer's time base unit struct to default values
    LL_TIM_StructInit(&init);

    init.Prescaler = 0u;
    init.CounterMode = LL_TIM_COUNTERMODE_UP;

    // The max frequency is 170MHz and we know this is representable with
    // 16 bits of cycles, so this will succeed.
    init.Autoreload = nsec_to_cycles(WS2812_PERIOD_NS, data) - 1u;
    init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

    // Initialize the time base unit
    if (LL_TIM_Init(config->timer, &init) != SUCCESS) {
        LOG_ERR("Could not initialize timer");
        return -EIO;
    }

#if !defined(CONFIG_SOC_SERIES_STM32L0X) && !defined(CONFIG_SOC_SERIES_STM32L1X)
    // enable outputs and counter
    if (IS_TIM_BREAK_INSTANCE(config->timer)) {
        LL_TIM_EnableAllOutputs(config->timer);
    }
#endif

    if (config->timer_channel < 1u || config->timer_channel > TIMER_MAX_CH) {
        LOG_ERR("Invalid timer channel (%d)", config->timer_channel);
        return -EINVAL;
    }

    timer_channel = timer_ch2ll[config->timer_channel - 1u];

    // Set output channel config struct to default values
    LL_TIM_OC_StructInit(&oc_init);

    oc_init.OCMode = LL_TIM_OCMODE_PWM1;
    oc_init.OCState = LL_TIM_OCSTATE_ENABLE;
    // 0% duty cycle until we hear otherwise
    oc_init.CompareValue = 0;
    oc_init.OCPolarity = LL_TIM_OCPOLARITY_HIGH;

    // Initialize timer output channel configuration
    if (LL_TIM_OC_Init(config->timer, timer_channel, &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }

    LL_TIM_EnableARRPreload(config->timer);
    LL_TIM_OC_EnablePreload(config->timer, timer_channel);
    LL_TIM_EnableDMAReq_UPDATE(config->timer);

    // Now setup DMA
    LL_DMA_InitTypeDef dma_init;
    LL_DMA_StructInit(&dma_init);

    if (config->timer_number > ARRAY_SIZE(timer_number2dma_up_event) + 1) {
        LOG_ERR("Invalid timer number (%d)", config->timer_number);
        return -EINVAL;
    }

    dma_mux_periph_request =
        timer_number2dma_up_event[config->timer_number - 1];

    // The timer_number2dma_up_event array has holes, which are filled with
    // zeros
    if (dma_mux_periph_request == 0) {
        LOG_ERR("Invalid timer number (%d)", config->timer_number);
        return -EINVAL;
    }

    dma_init.PeriphOrM2MSrcAddress =
        ((uint32_t)config->timer) +
        timer_ch2ccr_offset[config->timer_channel - 1u];
    dma_init.MemoryOrM2MDstAddress = (uint32_t)data->pixel_bits;
    dma_init.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_init.Mode = LL_DMA_MODE_CIRCULAR;
    dma_init.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_init.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    // We are writing to a CCR register, which is 16 bits without dithering.
    // With a period of 1.25us and duty cycles with a maximum of 600ns
    // and a timer prescaler of 0, we can represent the duty cycle in one byte,
    // do we use the DMA function that can transform bytes in a source in to 2
    // bytes at the destination, sticking one byte of zeros in front
    dma_init.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    dma_init.MemoryOrM2MDstDataSize = LL_DMA_PDATAALIGN_BYTE;

    dma_init.NbData = 0;
    dma_init.PeriphRequest = dma_mux_periph_request;
    dma_init.Priority = LL_DMA_PRIORITY_HIGH;

    if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(dma1)))) {
        LOG_ERR("NOT READY");
        return -EBUSY;
    }

    if (config->dma_channel < 1u || config->dma_channel > DMA_MAX_CH) {
        LOG_ERR("Invalid DMA channel (%d)", config->dma_channel);
        return -EINVAL;
    }

    dma_channel = dma_ch2ll[config->dma_channel - 1u];
    LL_DMA_Init(dma, dma_channel, &dma_init);

    irq_disable(DMA1_Channel1_IRQn);
    irq_connect_dynamic(DMA1_Channel1_IRQn, 1, &dma_complete_isr, dev, 0);
    irq_enable(DMA1_Channel1_IRQn);

    irq_connect_dynamic(config->timer_up_irq_num, 1, &timer_wait_isr, dev, 0);
    irq_enable(config->timer_up_irq_num);

    LL_DMA_EnableIT_TC(dma, dma_channel);
    LL_DMA_EnableChannel(dma, dma_channel);

    LL_TIM_EnableDMAReq_UPDATE(config->timer);

    return 0;
}

#define DT_INST_CLK(index)                                                     \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(DT_PARENT(DT_DRV_INST(index)), bus),             \
        .enr = DT_CLOCKS_CELL(DT_PARENT(DT_DRV_INST(index)), bits)             \
    }

#define STRIP_INIT(index)                                                      \
    static struct ws2812_pwm_stm32_data ws2812_pwm_stm32_data_##index =        \
        {/* using a compound literal */                                        \
         .pixel_bits = (uint8_t[NUM_RESET_PIXELS +                             \
                                24 * DT_INST_PROP(index, num_leds)]){}};       \
                                                                               \
    static const struct soc_gpio_pinctrl ws2812_pwm_stm32_pins_##index[] =     \
        ST_STM32_DT_INST_PINCTRL(index, 0);                                    \
    static const struct ws2812_pwm_stm32_config                                \
        ws2812_pwm_stm32_config_##index = {                                    \
            .timer_channel = DT_INST_PROP(index, timer_channel),               \
            .timer_number = DT_INST_PROP(index, timer_number),                 \
            .timer = (TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(                     \
                DT_DRV_INST(index))), /* if timer has "up" interrupt, use      \
                                         that, else use global interrupt */    \
            .timer_up_irq_num = COND_CODE_1(                                   \
                DT_IRQ_HAS_NAME(DT_PARENT(DT_DRV_INST(index)), up),            \
                (DT_IRQ_BY_NAME(DT_PARENT(DT_DRV_INST(index)), up, irq)),      \
                (DT_IRQ_BY_NAME(DT_PARENT(DT_DRV_INST(index)), global, irq))), \
            .dma_channel = DT_INST_PROP(index, dma_channel),                   \
            .dma = (DMA_TypeDef *)DT_REG_ADDR(DT_NODELABEL(dma1)),             \
            .pclken = DT_INST_CLK(index),                                      \
            .pinctrl = ws2812_pwm_stm32_pins_##index,                          \
            .num_leds = DT_INST_PROP(index, num_leds)};                        \
                                                                               \
    DEVICE_DT_INST_DEFINE(                                                     \
        index, &ws2812_pwm_stm32_init, NULL, &ws2812_pwm_stm32_data_##index,   \
        &ws2812_pwm_stm32_config_##index, POST_KERNEL,                         \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ws2812_pwm_stm32_driver_api);

DT_INST_FOREACH_STATUS_OKAY(STRIP_INIT)
