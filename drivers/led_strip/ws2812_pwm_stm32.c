/*
 * Copyright (c) 2021 Tools for Humanity
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT worldsemi_ws2812_pwm_stm32

#include <soc.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/dma/dma_stm32.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util_macro.h>

#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(worldsemi_ws2812_pwm_stm32, CONFIG_LED_STRIP_LOG_LEVEL);

#define WS2812_PERIOD_NS       1250
#define WS2812_PERIOD_1_BIT_NS 600
#define WS2812_PERIOD_0_BIT_NS 300
#define NUM_RESET_PIXELS       65

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

// Mapping from timer channel given in DTB to CCR register offsets
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

struct ws2812_pwm_stm32_data {
    // To hold clock frequency. We get this dynamically from the clock subsystem
    uint32_t tim_clk;
    // This points to an array with one byte for each bit
    uint8_t *pixel_bits;
    // For the generic API update function to be able to wait for LEDs to update
    struct k_sem update_sem;
    // DMA transfer configuration (mutable: block_size changes per transfer)
    struct dma_config dma_cfg;
    struct dma_block_config dma_blk_cfg;
};

struct ws2812_pwm_stm32_config {
    // must subtract 1 when looking up low level equivalent
    uint32_t timer_channel;
    TIM_TypeDef *timer;
    // DMA controller device and channel from device tree
    const struct device *dma_dev;
    uint32_t dma_channel;
    uint32_t dma_slot;
    uint32_t dma_channel_config;
    struct stm32_pclken pclken;
    const struct pinctrl_dev_config *pcfg;
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
// bytes per pixel. Each bit that is sent to the pixels is encoded as a
// particular duty cycle with an 800kHz frequency. This means that we need to
// continuously change the CCR (capture/compare register) of a timer channel
// after each PWM period. We do this by using DMA to update CCR every time the
// timer rolls over, which is an UPDATE event in timer terms. As a consequence,
// we need to expand every bit of the RGB values into a byte, since DMA works on
// bytes as the smallest unit. This means for X number of pixels, we need (8
// bytes for red) + (8 bytes for green) + (8 bytes for blue) = 24. So X * 24
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
            data->pixel_bits[i * 24 + j] =
                (pixels[i].g & (0x80 >> j)) ? one_bit : zero_bit;
        }
        for (j = 8; j < 16; ++j) {
            data->pixel_bits[i * 24 + j] =
                (pixels[i].r & (0x80 >> (j - 8))) ? one_bit : zero_bit;
        }
        for (j = 16; j < 24; ++j) {
            data->pixel_bits[i * 24 + j] =
                (pixels[i].b & (0x80 >> (j - 16))) ? one_bit : zero_bit;
        }
    }
    memset(data->pixel_bits + num_pixels * 24, 0, NUM_RESET_PIXELS);
}

static void
dma_complete_callback(const struct device *dma_dev, void *user_data,
                      uint32_t channel, int status)
{
    const struct device *dev = user_data;
    struct ws2812_pwm_stm32_data *data = dev->data;

    ARG_UNUSED(dma_dev);
    ARG_UNUSED(channel);
    ARG_UNUSED(status);

    k_sem_give(&data->update_sem);
}

static int
ws2812_pwm_stm32_update_rgb(const struct device *dev, struct led_rgb *pixels,
                            size_t num_pixels)
{
    const struct ws2812_pwm_stm32_config *config = dev->config;
    struct ws2812_pwm_stm32_data *data = dev->data;
    int r;

    if (num_pixels > config->num_leds) {
        LOG_ERR("too many pixels given: %u, max: %u", num_pixels,
                config->num_leds);
        return -EINVAL;
    }

    // Disable the timer and DMA channel before reconfiguring
    LL_TIM_DisableCounter(config->timer);
    rgb_to_dma_pixels(dev, pixels, num_pixels);
    dma_stop(config->dma_dev, config->dma_channel);

    // Update block size for this transfer
    data->dma_blk_cfg.block_size = NUM_RESET_PIXELS + num_pixels * 24;

    r = dma_config(config->dma_dev, config->dma_channel, &data->dma_cfg);
    if (r < 0) {
        LOG_ERR("DMA config failed (%d)", r);
        return r;
    }

    LL_TIM_SetPrescaler(config->timer, 0);
    LL_TIM_SetAutoReload(config->timer,
                         nsec_to_cycles(WS2812_PERIOD_NS, data) - 1u);
    LL_TIM_CC_EnableChannel(config->timer,
                            timer_ch2ll[config->timer_channel - 1u]);

    LL_TIM_GenerateEvent_UPDATE(config->timer);

    LL_TIM_EnableDMAReq_UPDATE(config->timer);
    LL_TIM_DisableIT_UPDATE(config->timer);

    r = dma_start(config->dma_dev, config->dma_channel);
    if (r < 0) {
        LOG_ERR("DMA start failed (%d)", r);
        return r;
    }

    // We need to trigger an event so that the timer's CCR register
    // (representing the PWM duty cycle) is loaded with the first DMA-provided
    // value before we start the timer. If we don't do this, then when the timer
    // starts, the first duty cycle it has is indeterminant

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
    (void)dev;
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

static int
ws2812_pwm_stm32_init(const struct device *dev)
{
    struct ws2812_pwm_stm32_data *data = dev->data;
    const struct ws2812_pwm_stm32_config *config = dev->config;
    const struct device *clk;
    LL_TIM_InitTypeDef init;
    LL_TIM_OC_InitTypeDef oc_init;
    uint32_t timer_channel;
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

    // configure GPIO pin alternate function
    r = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
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

    // Setup DMA using Zephyr DMA API
    if (!device_is_ready(config->dma_dev)) {
        LOG_ERR("DMA device not ready");
        return -ENODEV;
    }

    uint32_t ch_cfg = config->dma_channel_config;

    // Configure DMA transfer
    memset(&data->dma_cfg, 0, sizeof(data->dma_cfg));
    data->dma_cfg.dma_slot = config->dma_slot;
    data->dma_cfg.channel_direction = STM32_DMA_CONFIG_DIRECTION(ch_cfg);
    data->dma_cfg.source_data_size = STM32_DMA_CONFIG_MEMORY_DATA_SIZE(ch_cfg);
    data->dma_cfg.dest_data_size =
        STM32_DMA_CONFIG_PERIPHERAL_DATA_SIZE(ch_cfg);
    data->dma_cfg.channel_priority = STM32_DMA_CONFIG_PRIORITY(ch_cfg);
    data->dma_cfg.block_count = 1;
    data->dma_cfg.head_block = &data->dma_blk_cfg;
    data->dma_cfg.dma_callback = dma_complete_callback;
    data->dma_cfg.user_data = (void *)dev;

    memset(&data->dma_blk_cfg, 0, sizeof(data->dma_blk_cfg));
    data->dma_blk_cfg.source_address = (uint32_t)data->pixel_bits;
    data->dma_blk_cfg.dest_address =
        ((uint32_t)config->timer) +
        timer_ch2ccr_offset[config->timer_channel - 1u];
    data->dma_blk_cfg.block_size = 0;
    data->dma_blk_cfg.source_addr_adj = STM32_DMA_CONFIG_MEMORY_ADDR_INC(ch_cfg)
                                            ? DMA_ADDR_ADJ_INCREMENT
                                            : DMA_ADDR_ADJ_NO_CHANGE;
    data->dma_blk_cfg.dest_addr_adj =
        STM32_DMA_CONFIG_PERIPHERAL_ADDR_INC(ch_cfg) ? DMA_ADDR_ADJ_INCREMENT
                                                     : DMA_ADDR_ADJ_NO_CHANGE;

    r = dma_config(config->dma_dev, config->dma_channel, &data->dma_cfg);
    if (r < 0) {
        LOG_ERR("Could not configure DMA (%d)", r);
        return r;
    }

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
    PINCTRL_DT_INST_DEFINE(index);                                             \
                                                                               \
    static const struct ws2812_pwm_stm32_config                                \
        ws2812_pwm_stm32_config_##index = {                                    \
            .timer_channel = DT_INST_PROP(index, timer_channel),               \
            .timer =                                                           \
                (TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_DRV_INST(index))),     \
            .dma_dev = DEVICE_DT_GET(STM32_DMA_CTLR(index, tx)),               \
            .dma_channel = DT_INST_DMAS_CELL_BY_NAME(index, tx, channel),      \
            .dma_slot = STM32_DMA_SLOT(index, tx, slot),                       \
            .dma_channel_config = STM32_DMA_CHANNEL_CONFIG(index, tx),         \
            .pclken = DT_INST_CLK(index),                                      \
            .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(index),                     \
            .num_leds = DT_INST_PROP(index, num_leds)};                        \
                                                                               \
    DEVICE_DT_INST_DEFINE(                                                     \
        index, &ws2812_pwm_stm32_init, NULL, &ws2812_pwm_stm32_data_##index,   \
        &ws2812_pwm_stm32_config_##index, POST_KERNEL,                         \
        CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ws2812_pwm_stm32_driver_api);

DT_INST_FOREACH_STATUS_OKAY(STRIP_INIT)
