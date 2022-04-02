#include <assert.h>
#include <device.h>
#include <stm32_ll_hrtim.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <zephyr.h>

#include <drivers/clock_control/stm32_clock_control.h>
#include <pinmux/pinmux_stm32.h>
#include <soc.h>

#include "ir_camera_system.h"
#include "ir_camera_timer_settings.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(ir_camera_system);

#define DT_INST_CLK(inst)                                                      \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(DT_PARENT(inst), bus),                           \
        .enr = DT_CLOCKS_CELL(DT_PARENT(inst), bits)                           \
    }

struct pin_control {
    const struct soc_gpio_pinctrl *pins;
    size_t len;
};

// I expect all camera triggers to be on the same timer, but with different
// channels

// START --- 2D ToF (time of flight) camera trigger
static const struct soc_gpio_pinctrl tof_2d_camera_trigger_pins[] =
    ST_STM32_DT_PINCTRL(tof_2d_camera_trigger, 0);
static_assert(ARRAY_SIZE(tof_2d_camera_trigger_pins) == 1,
              "For tof_2d_camera_trigger, we expect one entry in pinctrl-0");
static struct stm32_pclken tof_2d_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(tof_2d_camera_trigger));
#define TOF_2D_CAMERA_TRIGGER_TIMER                                            \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(tof_2d_camera_trigger))))
#define TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL                                    \
    (DT_PROP_BY_IDX(DT_NODELABEL(tof_2d_camera_trigger), channels, 0))

// START --- IR eye camera trigger
static const struct soc_gpio_pinctrl ir_eye_camera_trigger_pins[] =
    ST_STM32_DT_PINCTRL(ir_eye_camera_trigger, 0);
static_assert(ARRAY_SIZE(ir_eye_camera_trigger_pins) == 1,
              "For ir_eye_camera_trigger, we expect one entry in pinctrl-0");
static struct stm32_pclken ir_eye_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(ir_eye_camera_trigger));
#define IR_EYE_CAMERA_TRIGGER_TIMER                                            \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(ir_eye_camera_trigger))))
#define IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL                                    \
    (DT_PROP_BY_IDX(DT_NODELABEL(ir_eye_camera_trigger), channels, 0))
// END --- IR eye

// START --- IR face camera trigger
static const struct soc_gpio_pinctrl ir_face_camera_trigger_pins[] =
    ST_STM32_DT_PINCTRL(ir_face_camera_trigger, 0);
static_assert(ARRAY_SIZE(ir_face_camera_trigger_pins) == 1,
              "For ir_face_camera_trigger, we expect one entry in pinctrl-0");
static struct stm32_pclken ir_face_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(ir_face_camera_trigger));
#define IR_FACE_CAMERA_TRIGGER_TIMER                                           \
    ((TIM_TypeDef *)DT_REG_ADDR(                                               \
        DT_PARENT(DT_NODELABEL(ir_face_camera_trigger))))
#define IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL                                   \
    (DT_PROP_BY_IDX(DT_NODELABEL(ir_face_camera_trigger), channels, 0))
// END --- IR face

// AKA: TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER ==
// IR_FACE_CAMERA_TRIGGER_TIMER
static_assert(TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER &&
                  IR_EYE_CAMERA_TRIGGER_TIMER == IR_FACE_CAMERA_TRIGGER_TIMER,
              "We expect that all camera triggers are different channels on "
              "the same timer");

#define CAMERA_TRIGGER_TIMER IR_FACE_CAMERA_TRIGGER_TIMER
#define CAMERA_TRIGGER_TIMER_IRQn                                              \
    DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(ir_eye_camera_trigger)), up, irq)

// START --- 850nm LEDs
static const struct soc_gpio_pinctrl led_850nm_pins[] =
    ST_STM32_DT_PINCTRL(led_850nm, 0);
static_assert(ARRAY_SIZE(led_850nm_pins) == 2,
              "For LED 850nm DTS node, we expect two entries in pinctrl-0");
static struct stm32_pclken led_850nm_pclken =
    DT_INST_CLK(DT_NODELABEL(led_850nm));
#define LED_850NM_TIMER                                                        \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(led_850nm))))
#define LED_850NM_TIMER_LEFT_CHANNEL                                           \
    (DT_PROP_BY_IDX(DT_NODELABEL(led_850nm), channels, 0))
#define LED_850NM_TIMER_RIGHT_CHANNEL                                          \
    (DT_PROP_BY_IDX(DT_NODELABEL(led_850nm), channels, 1))
// END --- 850nm LEDs

// START --- 940nm LED
static const struct soc_gpio_pinctrl led_940nm_pins[] =
    ST_STM32_DT_PINCTRL(led_940nm, 0);
static_assert(ARRAY_SIZE(led_940nm_pins) == 2,
              "For LED 940nm DTS node, we expect two entries in pinctrl-0");
static struct stm32_pclken led_940nm_pclken =
    DT_INST_CLK(DT_NODELABEL(led_940nm));
#define LED_940NM_TIMER                                                        \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(led_940nm))))
#define LED_940NM_TIMER_LEFT_CHANNEL                                           \
    (DT_PROP_BY_IDX(DT_NODELABEL(led_940nm), channels, 0))
#define LED_940NM_TIMER_RIGHT_CHANNEL                                          \
    (DT_PROP_BY_IDX(DT_NODELABEL(led_940nm), channels, 1))
// END --- 940nm LED

// START --- 740nm LED
static const struct soc_gpio_pinctrl led_740nm_pins[] =
    ST_STM32_DT_PINCTRL(led_740nm, 0);
static_assert(ARRAY_SIZE(led_740nm_pins) == 1,
              "For LED 740nm DTS node, we expect one entry in pinctrl-0");
static struct stm32_pclken led_740nm_pclken =
    DT_INST_CLK(DT_NODELABEL(led_740nm));
#define LED_740NM_TIMER                                                        \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(led_740nm))))
#define LED_740NM_TIMER_CHANNEL                                                \
    (DT_PROP_BY_IDX(DT_NODELABEL(led_740nm), channels, 0))
// END --- 740nm LED

static_assert(LED_740NM_TIMER == LED_940NM_TIMER,
              "The 740nm timer and the 940nm timer must be the same");

#define LED_740NM_940NM_COMMON_TIMER LED_740NM_TIMER

// START -- combined: for easy initialization of the above
static struct stm32_pclken *all_pclken[] = {
    &led_850nm_pclken,
    &led_740nm_pclken,
    &led_940nm_pclken,
    &tof_2d_camera_trigger_pclken,
    &ir_eye_camera_trigger_pclken,
    &ir_face_camera_trigger_pclken,
};

static const struct pin_control pin_controls[] = {
    {.pins = led_850nm_pins, .len = ARRAY_SIZE(led_850nm_pins)},
    {.pins = led_740nm_pins, .len = ARRAY_SIZE(led_740nm_pins)},
    {.pins = led_940nm_pins, .len = ARRAY_SIZE(led_940nm_pins)},
    {.pins = tof_2d_camera_trigger_pins,
     .len = ARRAY_SIZE(tof_2d_camera_trigger_pins)},
    {.pins = ir_eye_camera_trigger_pins,
     .len = ARRAY_SIZE(ir_eye_camera_trigger_pins)},
    {.pins = ir_face_camera_trigger_pins,
     .len = ARRAY_SIZE(ir_face_camera_trigger_pins)},
};

static_assert(ARRAY_SIZE(pin_controls) == ARRAY_SIZE(all_pclken),
              "Each array must be the same length");
// END --- combined

static struct ir_camera_timer_settings global_timer_settings;

static bool enable_ir_eye_camera;
static bool enable_ir_face_camera;
static bool enable_2d_tof_camera;

static InfraredLEDs_Wavelength enabled_led_wavelength =
    InfraredLEDs_Wavelength_WAVELENGTH_NONE;

#define TIMER_MAX_CH 4

/** Channel to LL mapping. */
static const uint32_t ch2ll[TIMER_MAX_CH] = {
    LL_TIM_CHANNEL_CH1, LL_TIM_CHANNEL_CH2, LL_TIM_CHANNEL_CH3,
    LL_TIM_CHANNEL_CH4};

/** Channel to compare set function mapping. */
static void (*const set_timer_compare[TIMER_MAX_CH])(TIM_TypeDef *,
                                                     uint32_t) = {
    LL_TIM_OC_SetCompareCH1,
    LL_TIM_OC_SetCompareCH2,
    LL_TIM_OC_SetCompareCH3,
    LL_TIM_OC_SetCompareCH4,
};

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

    if (pclken->bus == STM32_CLOCK_BUS_APB1) {
        apb_psc = STM32_APB1_PRESCALER;
    } else {
        apb_psc = STM32_APB2_PRESCALER;
    }

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

    return 0;
}

static int
enable_clocks_and_configure_pins(void)
{
    int r;
    size_t i;
    uint32_t timer_clock_freq;
    const struct device *clk;

    r = 0;
    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

    for (i = 0; i < ARRAY_SIZE(all_pclken); ++i) {
        r = clock_control_on(clk, all_pclken[i]);
        if (r < 0) {
            LOG_ERR("Could not initialize clock (%d)", r);
            return r;
        }

        r = get_tim_clk(all_pclken[i], &timer_clock_freq);
        if (r < 0) {
            LOG_ERR("Could not obtain timer clock (%d)", r);
            return r;
        }
        if (timer_clock_freq != ASSUMED_TIMER_CLOCK_FREQ) {
            LOG_ERR(
                "To make Pete's life easier, this module is written with"
                "the assumption that all timers involved are running "
                "at " STRINGIFY(
                    ASSUMED_TIMER_CLOCK_ASSUMED_TIMER_CLOCK_FREQ) "Hz, but one "
                                                                  "of the "
                                                                  "clocks "
                                                                  "involved"
                                                                  "is running "
                                                                  "at %uHz!",
                timer_clock_freq);
            return -EINVAL;
        }

        r = stm32_dt_pinctrl_configure(pin_controls[i].pins,
                                       pin_controls[i].len, 0);
        if (r < 0) {
            LOG_ERR("pinctrl"
                    "setup failed (%d)",
                    r);
            return r;
        }
    }

    return r;
}

static void
zero_led_ccrs(void)
{
    set_timer_compare[LED_850NM_TIMER_LEFT_CHANNEL - 1](LED_850NM_TIMER, 0);
    set_timer_compare[LED_850NM_TIMER_RIGHT_CHANNEL - 1](LED_850NM_TIMER, 0);
    set_timer_compare[LED_940NM_TIMER_LEFT_CHANNEL - 1](LED_940NM_TIMER, 0);
    set_timer_compare[LED_940NM_TIMER_RIGHT_CHANNEL - 1](LED_940NM_TIMER, 0);
    set_timer_compare[LED_740NM_TIMER_CHANNEL - 1](LED_740NM_TIMER, 0);
}

static int
setup_camera_triggers(void)
{
    LL_TIM_InitTypeDef init;
    LL_TIM_OC_InitTypeDef oc_init;

    LL_TIM_StructInit(&init);

    init.Prescaler = 0;
    init.CounterMode = LL_TIM_COUNTERMODE_UP;
    init.Autoreload = 0;
    init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

    if (LL_TIM_Init(CAMERA_TRIGGER_TIMER, &init) != SUCCESS) {
        LOG_ERR("Could not initialize camera trigger timer");
        return -EIO;
    }

    // enable outputs and counter
    if (IS_TIM_BREAK_INSTANCE(CAMERA_TRIGGER_TIMER)) {
        LL_TIM_EnableAllOutputs(CAMERA_TRIGGER_TIMER);
    }

    LL_TIM_OC_StructInit(&oc_init);
    oc_init.OCMode = LL_TIM_OCMODE_PWM1;
    oc_init.OCState = LL_TIM_OCSTATE_ENABLE;
    oc_init.CompareValue = 0;
    oc_init.OCPolarity = LL_TIM_OCPOLARITY_HIGH;

    if (LL_TIM_OC_Init(CAMERA_TRIGGER_TIMER,
                       ch2ll[TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }
    if (LL_TIM_OC_Init(CAMERA_TRIGGER_TIMER,
                       ch2ll[IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }
    if (LL_TIM_OC_Init(CAMERA_TRIGGER_TIMER,
                       ch2ll[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }

    LL_TIM_EnableARRPreload(CAMERA_TRIGGER_TIMER);

    LL_TIM_OC_EnablePreload(CAMERA_TRIGGER_TIMER,
                            ch2ll[TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL - 1]);
    LL_TIM_OC_EnablePreload(CAMERA_TRIGGER_TIMER,
                            ch2ll[IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL - 1]);
    LL_TIM_OC_EnablePreload(CAMERA_TRIGGER_TIMER,
                            ch2ll[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1]);

    LL_TIM_SetTriggerOutput(CAMERA_TRIGGER_TIMER, LL_TIM_TRGO_UPDATE);

    LL_TIM_EnableCounter(CAMERA_TRIGGER_TIMER);

    return 0;
}

static void
set_ccr_ir_leds(void)
{
    zero_led_ccrs();

    switch (enabled_led_wavelength) {
    case InfraredLEDs_Wavelength_WAVELENGTH_850NM:
        set_timer_compare[LED_850NM_TIMER_LEFT_CHANNEL - 1](
            LED_850NM_TIMER, global_timer_settings.ccr);
        set_timer_compare[LED_850NM_TIMER_RIGHT_CHANNEL - 1](
            LED_850NM_TIMER, global_timer_settings.ccr);
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT:
        set_timer_compare[LED_850NM_TIMER_LEFT_CHANNEL - 1](
            LED_850NM_TIMER, global_timer_settings.ccr);
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT:
        set_timer_compare[LED_850NM_TIMER_RIGHT_CHANNEL - 1](
            LED_850NM_TIMER, global_timer_settings.ccr);
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_940NM:
        set_timer_compare[LED_940NM_TIMER_LEFT_CHANNEL - 1](
            LED_940NM_TIMER, global_timer_settings.ccr);
        set_timer_compare[LED_940NM_TIMER_RIGHT_CHANNEL - 1](
            LED_940NM_TIMER, global_timer_settings.ccr);
    case InfraredLEDs_Wavelength_WAVELENGTH_940NM_LEFT:
        set_timer_compare[LED_940NM_TIMER_LEFT_CHANNEL - 1](
            LED_940NM_TIMER, global_timer_settings.ccr);
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_940NM_RIGHT:
        set_timer_compare[LED_940NM_TIMER_RIGHT_CHANNEL - 1](
            LED_940NM_TIMER, global_timer_settings.ccr);
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_740NM:
        set_timer_compare[LED_740NM_TIMER_CHANNEL - 1](
            LED_740NM_TIMER, global_timer_settings.ccr_740nm);
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_NONE:
        break;
    }
}

static inline void
set_trigger_cc(bool enabled, int channel)
{
    if (enabled) {
        set_timer_compare[channel - 1](CAMERA_TRIGGER_TIMER,
                                       global_timer_settings.ccr);
    } else {
        set_timer_compare[channel - 1](CAMERA_TRIGGER_TIMER, 0);
    }
}

static void
apply_new_timer_settings()
{
    static struct ir_camera_timer_settings old_timer_settings = {0};

    unsigned key = irq_lock();

    LL_TIM_SetPrescaler(CAMERA_TRIGGER_TIMER, global_timer_settings.psc);
    LL_TIM_SetAutoReload(CAMERA_TRIGGER_TIMER, global_timer_settings.arr);

    LL_TIM_SetPrescaler(LED_850NM_TIMER, global_timer_settings.psc);
    LL_TIM_SetAutoReload(LED_850NM_TIMER, global_timer_settings.arr);

    LL_TIM_SetPrescaler(LED_740NM_940NM_COMMON_TIMER,
                        global_timer_settings.psc);
    if (enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr / 2);
    } else {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr);
    }

    set_trigger_cc(enable_ir_eye_camera, IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL);
    set_trigger_cc(enable_ir_face_camera, IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL);
    set_trigger_cc(enable_2d_tof_camera, TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL);

    set_ccr_ir_leds();

    irq_unlock(key);

    // Auto-reload preload is enabled. This means that the auto-reload preload
    // register is deposited into the auto-reload register only on a timer
    // update, which will never occur if the auto-reload value was previously
    // zero. So in that case, we manually issue an update event.
    if (old_timer_settings.arr == 0) {
        LL_TIM_GenerateEvent_UPDATE(CAMERA_TRIGGER_TIMER);
    }

    old_timer_settings = global_timer_settings;
}

static int
setup_850nm_led_timer(void)
{
    LL_TIM_InitTypeDef init;
    LL_TIM_OC_InitTypeDef oc_init;

    LL_TIM_StructInit(&init);

    init.Prescaler = 0;
    init.CounterMode = LL_TIM_COUNTERMODE_UP;
    init.Autoreload = 0;
    init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

    if (LL_TIM_Init(LED_850NM_TIMER, &init) != SUCCESS) {
        LOG_ERR("Could not initialize the LED_850NM_TIMER");
        return -EIO;
    }

    if (IS_TIM_BREAK_INSTANCE(LED_850NM_TIMER)) {
        LL_TIM_EnableAllOutputs(LED_850NM_TIMER);
    }

    LL_TIM_OC_StructInit(&oc_init);
    oc_init.OCMode = LL_TIM_OCMODE_PWM1;
    oc_init.OCState = LL_TIM_OCSTATE_ENABLE;
    oc_init.CompareValue = 0;
    oc_init.OCPolarity = LL_TIM_OCPOLARITY_HIGH;

    if (LL_TIM_OC_Init(LED_850NM_TIMER, ch2ll[LED_850NM_TIMER_LEFT_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR(
            "Could not initialize the LED_850NM_TIMER's left channel output");
        return -EIO;
    }

    if (LL_TIM_OC_Init(LED_850NM_TIMER,
                       ch2ll[LED_850NM_TIMER_RIGHT_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR(
            "Could not initialize the LED_850NM_TIMER's right channel output");
        return -EIO;
    }

    LL_TIM_SetOnePulseMode(LED_850NM_TIMER, LL_TIM_ONEPULSEMODE_SINGLE);

    LL_TIM_SetUpdateSource(LED_850NM_TIMER, LL_TIM_UPDATESOURCE_COUNTER);

    LL_TIM_SetSlaveMode(LED_850NM_TIMER,
                        LL_TIM_SLAVEMODE_COMBINED_RESETTRIGGER);

    static_assert(CAMERA_TRIGGER_TIMER == TIM8,
                  "The slave mode trigger input source needs to be changed "
                  "here if CAMERA_TRIGGER_TIMER is not longer timer 8");
    LL_TIM_SetTriggerInput(LED_850NM_TIMER, LL_TIM_TS_ITR5); // timer 8

    LL_TIM_EnableARRPreload(LED_850NM_TIMER);

    LL_TIM_OC_EnablePreload(LED_850NM_TIMER,
                            ch2ll[LED_850NM_TIMER_LEFT_CHANNEL - 1]);
    LL_TIM_OC_EnablePreload(LED_850NM_TIMER,
                            ch2ll[LED_850NM_TIMER_RIGHT_CHANNEL - 1]);

    return 0;
}

static int
setup_740nm_940nm_led_timer(void)
{
    LL_TIM_InitTypeDef init;
    LL_TIM_OC_InitTypeDef oc_init;

    LL_TIM_StructInit(&init);

    init.Prescaler = 0;
    init.CounterMode = LL_TIM_COUNTERMODE_UP;
    init.Autoreload = 0;
    init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

    if (LL_TIM_Init(LED_740NM_940NM_COMMON_TIMER, &init) != SUCCESS) {
        LOG_ERR("Could not initialize the 740nm/940nm timer");
        return -EIO;
    }

    if (IS_TIM_BREAK_INSTANCE(LED_740NM_940NM_COMMON_TIMER)) {
        LL_TIM_EnableAllOutputs(LED_740NM_940NM_COMMON_TIMER);
    }

    LL_TIM_OC_StructInit(&oc_init);
    oc_init.OCMode = LL_TIM_OCMODE_PWM1;
    oc_init.OCState = LL_TIM_OCSTATE_ENABLE;
    oc_init.CompareValue = 0;
    oc_init.OCPolarity = LL_TIM_OCPOLARITY_HIGH;

    if (LL_TIM_OC_Init(LED_740NM_940NM_COMMON_TIMER,
                       ch2ll[LED_940NM_TIMER_LEFT_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize the 940nm timer's left channel output");
        return -EIO;
    }

    if (LL_TIM_OC_Init(LED_740NM_940NM_COMMON_TIMER,
                       ch2ll[LED_940NM_TIMER_RIGHT_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize the 940nm LED_740NM_940NM_COMMON_TIMER's "
                "right "
                "channel output");
        return -EIO;
    }

    if (LL_TIM_OC_Init(LED_740NM_940NM_COMMON_TIMER,
                       ch2ll[LED_740NM_TIMER_CHANNEL - 1],
                       &oc_init) != SUCCESS) {
        LOG_ERR("Could not initialize the 740nm LED_740NM_940NM_COMMON_TIMER's "
                "channel output");
        return -EIO;
    }

    LL_TIM_SetOnePulseMode(LED_740NM_940NM_COMMON_TIMER,
                           LL_TIM_ONEPULSEMODE_REPETITIVE);

    LL_TIM_SetUpdateSource(LED_740NM_940NM_COMMON_TIMER,
                           LL_TIM_UPDATESOURCE_COUNTER);

    LL_TIM_SetSlaveMode(LED_740NM_940NM_COMMON_TIMER,
                        LL_TIM_SLAVEMODE_COMBINED_RESETTRIGGER);

    static_assert(CAMERA_TRIGGER_TIMER == TIM8,
                  "The slave mode trigger input source needs to be changed "
                  "here if CAMERA_TRIGGER_TIMER is not longer timer 8");
    LL_TIM_SetTriggerInput(LED_740NM_940NM_COMMON_TIMER,
                           LL_TIM_TS_ITR5); // timer 8

    LL_TIM_EnableARRPreload(LED_740NM_940NM_COMMON_TIMER);

    LL_TIM_OC_EnablePreload(LED_740NM_940NM_COMMON_TIMER,
                            ch2ll[LED_940NM_TIMER_LEFT_CHANNEL - 1]);
    LL_TIM_OC_EnablePreload(LED_740NM_940NM_COMMON_TIMER,
                            ch2ll[LED_940NM_TIMER_RIGHT_CHANNEL - 1]);
    LL_TIM_OC_EnablePreload(LED_740NM_940NM_COMMON_TIMER,
                            ch2ll[LED_740NM_TIMER_CHANNEL - 1]);

    return 0;
}

void
ir_camera_system_enable_ir_eye_camera(void)
{
    enable_ir_eye_camera = true;
    set_timer_compare[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER, global_timer_settings.ccr);
}

void
ir_camera_system_disable_ir_eye_camera(void)
{
    enable_ir_eye_camera = false;
    set_timer_compare[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER, 0);
}

bool
ir_camera_system_ir_eye_camera_is_enabled(void)
{
    return enable_ir_eye_camera;
}

void
ir_camera_system_enable_ir_face_camera(void)
{
    enable_ir_face_camera = true;
    set_timer_compare[IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER, global_timer_settings.ccr);
}

void
ir_camera_system_disable_ir_face_camera(void)
{
    enable_ir_face_camera = false;
    set_timer_compare[IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER, 0);
}

bool
ir_camera_system_ir_face_camera_is_enabled(void)
{
    return enable_ir_face_camera;
}

void
ir_camera_system_enable_2d_tof_camera(void)
{
    enable_2d_tof_camera = true;
    set_timer_compare[TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER, global_timer_settings.ccr);
}

void
ir_camera_system_disable_2d_tof_camera(void)
{
    enable_2d_tof_camera = false;
    set_timer_compare[TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER, 0);
}

bool
ir_camera_system_2d_tof_camera_is_enabled(void)
{
    return enable_2d_tof_camera;
}

ret_code_t
ir_camera_system_init(void)
{
    int r = 0;

    r = enable_clocks_and_configure_pins();
    if (r < 0) {
        LOG_ERR("Error enabling clocks and configuring pins");
        return RET_ERROR_INTERNAL;
    }

    r = setup_740nm_940nm_led_timer();
    if (r < 0) {
        LOG_ERR("Error setting up 740nm/940nm IR LEDs");
        return RET_ERROR_INTERNAL;
    }

    r = setup_850nm_led_timer();
    if (r < 0) {
        LOG_ERR("Error setting up 850nm IR LEDs");
        return RET_ERROR_INTERNAL;
    }

    r = setup_camera_triggers();
    if (r < 0) {
        LOG_ERR("Error setting up IR camera triggers");
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

ret_code_t
ir_camera_system_set_fps(uint16_t fps)
{
    ret_code_t ret;

    if (fps > IR_CAMERA_SYSTEM_MAX_FPS) {
        ret = RET_ERROR_INVALID_PARAM;
    } else {
        ret = timer_settings_from_fps(fps, &global_timer_settings,
                                      &global_timer_settings);
        if (ret != RET_SUCCESS) {
            LOG_ERR("Error setting new FPS");
        } else {
            apply_new_timer_settings();
        }
        timer_settings_print(&global_timer_settings);
    }

    return ret;
}

ret_code_t
ir_camera_system_set_on_time_us(uint16_t on_time_us)
{
    ret_code_t ret;

    if (on_time_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        ret = RET_ERROR_INVALID_PARAM;
    } else {
        ret = timer_settings_from_on_time_us(on_time_us, &global_timer_settings,
                                             &global_timer_settings);
        if (ret != RET_SUCCESS) {
            LOG_ERR("Error setting new on-time");
        } else {
            apply_new_timer_settings();
        }
        timer_settings_print(&global_timer_settings);
    }

    return ret;
}

ret_code_t
ir_camera_system_set_on_time_740nm_us(uint16_t on_time_us)
{
    ret_code_t ret = timer_740nm_ccr_from_on_time_us(
        on_time_us, &global_timer_settings, &global_timer_settings);

    if (ret == RET_SUCCESS) {
        apply_new_timer_settings();
    }

    return ret;
}

void
ir_camera_system_enable_leds(InfraredLEDs_Wavelength wavelength)
{
    unsigned key = irq_lock();

    enabled_led_wavelength = wavelength;

    if (enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr / 2);
    } else {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr);
    }

    set_ccr_ir_leds();

    irq_unlock(key);
}

InfraredLEDs_Wavelength
ir_camera_system_get_enabled_leds(void)
{
    return enabled_led_wavelength;
}
