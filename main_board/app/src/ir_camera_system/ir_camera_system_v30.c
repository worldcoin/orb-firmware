#include "ir_camera_system.h"
#include "ir_camera_timer_settings.h"
#include <app_assert.h>
#include <assert.h>
#include <device.h>
#include <drivers/clock_control/stm32_clock_control.h>
#include <logging/log.h>
#include <soc.h>
#include <stm32_ll_hrtim.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <zephyr.h>
#include <drivers/pinctrl.h>
LOG_MODULE_REGISTER(ir_camera_system);

#define DT_INST_CLK(inst)                                                      \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(DT_PARENT(inst), bus),                           \
        .enr = DT_CLOCKS_CELL(DT_PARENT(inst), bits)                           \
    }

// I expect all camera triggers to be on the same timer, but with different
// channels

// START --- 2D ToF (time of flight) camera trigger
#define TOF_NODE DT_NODELABEL(tof_2d_camera_trigger)
PINCTRL_DT_DEFINE(TOF_NODE);
static_assert(DT_PROP_LEN(TOF_NODE, pinctrl_0) == 1,
              "For tof_2d_camera_trigger, we expect the pinctrl-0 property to "
              "contain one entry in the device tree node");
static struct stm32_pclken tof_2d_camera_trigger_pclken = DT_INST_CLK(TOF_NODE);
#define TOF_2D_CAMERA_TRIGGER_TIMER                                            \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(TOF_NODE)))
// END --- 2D ToF

// START --- IR eye camera trigger
#define IR_EYE_CAMERA_NODE DT_NODELABEL(ir_eye_camera_trigger)
PINCTRL_DT_DEFINE(IR_EYE_CAMERA_NODE);
static_assert(DT_PROP_LEN(IR_EYE_CAMERA_NODE, pinctrl_0) == 1,
              "For ir_eye_camera_trigger, we expect the pinctrl-0 property to "
              "contain one entry in the device tree node");
static struct stm32_pclken ir_eye_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(ir_eye_camera_trigger));
#define IR_EYE_CAMERA_TRIGGER_TIMER                                            \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(IR_EYE_CAMERA_NODE)))
// END --- IR eye

// START --- IR face camera trigger
#define IR_FACE_CAMERA_NODE DT_NODELABEL(ir_face_camera_trigger)
PINCTRL_DT_DEFINE(IR_FACE_CAMERA_NODE);
static_assert(DT_PROP_LEN(IR_FACE_CAMERA_NODE, pinctrl_0) == 1,
              "For ir_face_camera_trigger, we expect the pinctrl-0 property to "
              "contain one entry in the device tree node");

static struct stm32_pclken ir_face_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(ir_face_camera_trigger));
#define IR_FACE_CAMERA_TRIGGER_TIMER                                           \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(IR_FACE_CAMERA_NODE)))
// END --- IR face

#define CAMERA_TRIGGER_TIMER      IR_FACE_CAMERA_TRIGGER_TIMER
#define CAMERA_TRIGGER_TIMER_IRQn TIM3_IRQn

// AKA: TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER ==
// IR_FACE_CAMERA_TRIGGER_TIMER
static_assert(TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER &&
                  IR_EYE_CAMERA_TRIGGER_TIMER == IR_FACE_CAMERA_TRIGGER_TIMER,
              "We expect that all camera triggers are different channels on "
              "the same timer");

// START --- 740nm LED
#define LED_740NM_NODE DT_NODELABEL(led_740nm)
PINCTRL_DT_DEFINE(LED_740NM_NODE);
static_assert(DT_PROP_LEN(LED_740NM_NODE, pinctrl_0) == 1,
              "For the 740nm LED, we expect the pinctrl-0 property to contain "
              "one entry in the device tree node");
static struct stm32_pclken led_740nm_pclken = DT_INST_CLK(LED_740NM_NODE);
#define LED_740NM_TIMER         (TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_740NM_NODE))
#define LED_740NM_TIMER_CHANNEL LL_TIM_CHANNEL_CH2
// END --- 740nm LED

// START --- 850nm LEDs
#define LED_850NM_NODE DT_NODELABEL(led_850nm)
PINCTRL_DT_DEFINE(LED_850NM_NODE);
static_assert(DT_PROP_LEN(LED_850NM_NODE, pinctrl_0) == 2,
              "For the 850nm LED, we expect the pinctrl-0 property to contain "
              "two entries in the device tree node");
static struct stm32_pclken led_850nm_pclken = DT_INST_CLK(LED_850NM_NODE);
#define LED_850NM_HR_TIMER                                                     \
    (HRTIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_850NM_NODE))
// END --- 850nm LEDs

// START --- 940nm LED
#define LED_940NM_NODE DT_NODELABEL(led_940nm)
PINCTRL_DT_DEFINE(LED_940NM_NODE);
static_assert(DT_PROP_LEN(LED_940NM_NODE, pinctrl_0) == 2,
              "For the 940nm LED, we expect the pinctrl-0 property to contain "
              "two entries in the device tree node");
static struct stm32_pclken led_940nm_pclken = DT_INST_CLK(LED_940NM_NODE);
#define LED_940NM_HR_TIMER                                                     \
    (HRTIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_940NM_NODE))
// END --- 940nm LED

#define HR_TIMER (HRTIM_TypeDef *)HRTIM1_BASE

static_assert(LED_850NM_HR_TIMER == LED_940NM_HR_TIMER &&
                  LED_940NM_HR_TIMER == HR_TIMER,
              "850nm and 940nm timers must be the same high resolution timer "
              "and that timer must be HRTIM1");

#define CLEAR_TIMER      (TIM_TypeDef *)DT_REG_ADDR(DT_NODELABEL(timers15))
#define CLEAR_TIMER_IRQn TIM1_BRK_TIM15_IRQn
static struct stm32_pclken hr_reset_pclken = {
    .bus = DT_CLOCKS_CELL(DT_NODELABEL(timers15), bus),
    .enr = DT_CLOCKS_CELL(DT_NODELABEL(timers15), bits)};

// START -- combined: for easy initialization of the above
static struct stm32_pclken *all_pclken[] = {&led_850nm_pclken,
                                            &led_740nm_pclken,
                                            &led_940nm_pclken,
                                            &tof_2d_camera_trigger_pclken,
                                            &ir_eye_camera_trigger_pclken,
                                            &ir_face_camera_trigger_pclken,
                                            &hr_reset_pclken};

static const struct pinctrl_dev_config *pin_controls[] = {
    PINCTRL_DT_DEV_CONFIG_GET(LED_850NM_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(LED_740NM_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(LED_940NM_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(TOF_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(IR_EYE_CAMERA_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(IR_FACE_CAMERA_NODE),
    &(struct pinctrl_dev_config){}, // Dummy for timer 15, whose output pins we
                                    // do not use
};

static_assert(ARRAY_SIZE(pin_controls) == ARRAY_SIZE(all_pclken),
              "Each array must be the same length");
// END --- combined

static struct ir_camera_timer_settings global_timer_settings;

#define LED_940NM 3
#define LED_850NM 4

static bool enable_ir_eye_camera;
static bool enable_ir_face_camera;
static bool enable_2d_tof_camera;

static InfraredLEDs_Wavelength enabled_led_wavelength;

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

// This check ensures that the `base` parameter of the
// stm32_dt_pinctrl_configure function goes unused
#if DT_HAS_COMPAT_STATUS_OKAY(st_stm32f1_pinctrl)
#error We do not support stm32f1
#endif
        // skip last index since it is a dummy
        if (i != ARRAY_SIZE(all_pclken) - 1) {
            r = pinctrl_apply_state(pin_controls[i], PINCTRL_STATE_DEFAULT);
            if (r < 0) {
                LOG_ERR("pinctrl"
                        "setup failed (%d)",
                        r);
                return r;
            }
        }
    }

    return r;
}

#define SET_OUTPUT_EVENT_SOURCE_CONFIG(set_event)                              \
    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TC1, set_event);    \
    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TC2, set_event);    \
    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TD1, set_event);    \
    LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TD2, set_event)

ISR_DIRECT_DECLARE(set_fps_isr)
{
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_DisableIT_UPDATE(CAMERA_TRIGGER_TIMER);

    LL_TIM_SetPrescaler(CAMERA_TRIGGER_TIMER, global_timer_settings.psc);
    LL_TIM_SetPrescaler(LED_740NM_TIMER, global_timer_settings.psc);
    LL_TIM_SetAutoReload(CAMERA_TRIGGER_TIMER, global_timer_settings.arr);
    LL_TIM_SetAutoReload(LED_740NM_TIMER, global_timer_settings.arr / 2);

    if (enable_ir_eye_camera) {
        LL_TIM_OC_SetCompareCH3(CAMERA_TRIGGER_TIMER,
                                global_timer_settings.ccr);
    } else {
        LL_TIM_OC_SetCompareCH3(CAMERA_TRIGGER_TIMER, 0);
    }

    if (enable_ir_face_camera) {
        LL_TIM_OC_SetCompareCH4(CAMERA_TRIGGER_TIMER,
                                global_timer_settings.ccr);
    } else {
        LL_TIM_OC_SetCompareCH4(CAMERA_TRIGGER_TIMER, 0);
    }

    if (enable_2d_tof_camera) {
        LL_TIM_OC_SetCompareCH2(CAMERA_TRIGGER_TIMER,
                                global_timer_settings.ccr);
    } else {
        LL_TIM_OC_SetCompareCH2(CAMERA_TRIGGER_TIMER, 0);
    }

    if (enabled_led_wavelength ==
            InfraredLEDs_Wavelength_WAVELENGTH_940NM_LEFT ||
        enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_940NM) {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TC1,
                                     LL_HRTIM_OUTPUTSET_EEV_3);
    } else {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TC1,
                                     LL_HRTIM_OUTPUTSET_NONE);
    }

    if (enabled_led_wavelength ==
            InfraredLEDs_Wavelength_WAVELENGTH_940NM_RIGHT ||
        enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_940NM) {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TC2,
                                     LL_HRTIM_OUTPUTSET_EEV_3);
    } else {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TC2,
                                     LL_HRTIM_OUTPUTSET_NONE);
    }

    if (enabled_led_wavelength ==
            InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT ||
        enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_850NM) {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TD1,
                                     LL_HRTIM_OUTPUTSET_EEV_3);
    } else {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TD1,
                                     LL_HRTIM_OUTPUTSET_NONE);
    }

    if (enabled_led_wavelength ==
            InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT ||
        enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_850NM) {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TD2,
                                     LL_HRTIM_OUTPUTSET_EEV_3);
    } else {
        LL_HRTIM_OUT_SetOutputSetSrc(HR_TIMER, LL_HRTIM_OUTPUT_TD2,
                                     LL_HRTIM_OUTPUTSET_NONE);
    }

    if (enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
        static_assert(LED_740NM_TIMER_CHANNEL == LL_TIM_CHANNEL_CH2,
                      "Need to change the function call here to the new 740nm "
                      "timer channel");
        LL_TIM_OC_SetCompareCH2(LED_740NM_TIMER,
                                global_timer_settings.ccr_740nm);
    } else {
        static_assert(LED_740NM_TIMER_CHANNEL == LL_TIM_CHANNEL_CH2,
                      "Need to change the function call here to the new 740nm "
                      "timer channel");
        LL_TIM_OC_SetCompareCH2(LED_740NM_TIMER, 0);
    }

    if (global_timer_settings.fps != 0) {
        LL_TIM_SetPrescaler(CLEAR_TIMER, global_timer_settings.psc);
        LL_TIM_SetAutoReload(CLEAR_TIMER, global_timer_settings.ccr);
    } else {
        SET_OUTPUT_EVENT_SOURCE_CONFIG(LL_HRTIM_OUTPUTSET_NONE);
    }

    return 0; // no scheduling decision
}

ISR_DIRECT_DECLARE(hrtim_clear_isr)
{
    LL_TIM_ClearFlag_UPDATE(CLEAR_TIMER);

    // These operations are equivalent to calling LL_HRTIM_OUT_ForceLevel()
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_C].RSTx1R = HRTIM_RST1R_SRT;
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_C].RSTx2R = HRTIM_RST2R_SRT;
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].RSTx1R = HRTIM_RST1R_SRT;
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].RSTx2R = HRTIM_RST2R_SRT;

    return 0; // no scheduling decision
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

#if !defined(CONFIG_SOC_SERIES_STM32L0X) && !defined(CONFIG_SOC_SERIES_STM32L1X)
    // enable outputs and counter
    if (IS_TIM_BREAK_INSTANCE(CAMERA_TRIGGER_TIMER)) {
        LL_TIM_EnableAllOutputs(CAMERA_TRIGGER_TIMER);
    }
#endif

    LL_TIM_OC_StructInit(&oc_init);
    oc_init.OCMode = LL_TIM_OCMODE_PWM1;
    oc_init.OCState = LL_TIM_OCSTATE_ENABLE;
    oc_init.CompareValue = 0;
    oc_init.OCPolarity = LL_TIM_OCPOLARITY_HIGH;

    if (LL_TIM_OC_Init(CAMERA_TRIGGER_TIMER, LL_TIM_CHANNEL_CH2, &oc_init) !=
        SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }
    if (LL_TIM_OC_Init(CAMERA_TRIGGER_TIMER, LL_TIM_CHANNEL_CH3, &oc_init) !=
        SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }
    if (LL_TIM_OC_Init(CAMERA_TRIGGER_TIMER, LL_TIM_CHANNEL_CH4, &oc_init) !=
        SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }

    LL_TIM_EnableARRPreload(CAMERA_TRIGGER_TIMER);

    LL_TIM_OC_EnablePreload(CAMERA_TRIGGER_TIMER, LL_TIM_CHANNEL_CH2);
    LL_TIM_OC_EnablePreload(CAMERA_TRIGGER_TIMER, LL_TIM_CHANNEL_CH3);
    LL_TIM_OC_EnablePreload(CAMERA_TRIGGER_TIMER, LL_TIM_CHANNEL_CH4);

    LL_TIM_SetTriggerOutput(CAMERA_TRIGGER_TIMER, LL_TIM_TRGO_UPDATE);

    IRQ_DIRECT_CONNECT(CAMERA_TRIGGER_TIMER_IRQn, 3, set_fps_isr, 0);
    irq_enable(CAMERA_TRIGGER_TIMER_IRQn);

    LL_TIM_EnableCounter(CAMERA_TRIGGER_TIMER);

    return 0;
}

static int
setup_740nm(void)
{
    LL_TIM_InitTypeDef init;
    LL_TIM_OC_InitTypeDef oc_init;

    LL_TIM_StructInit(&init);

    init.Prescaler = 0;
    init.CounterMode = LL_TIM_COUNTERMODE_UP;
    init.Autoreload = 0;
    init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

    if (LL_TIM_Init(LED_740NM_TIMER, &init) != SUCCESS) {
        LOG_ERR("Could not initialize 740nm timer");
        return -EIO;
    }

    if (IS_TIM_BREAK_INSTANCE(LED_740NM_TIMER)) {
        LL_TIM_EnableAllOutputs(LED_740NM_TIMER);
    }

    LL_TIM_OC_StructInit(&oc_init);
    oc_init.OCMode = LL_TIM_OCMODE_PWM1;
    oc_init.OCState = LL_TIM_OCSTATE_DISABLE;
    oc_init.OCNState = LL_TIM_OCSTATE_ENABLE;
    oc_init.CompareValue = 0;
    oc_init.OCNPolarity = LL_TIM_OCPOLARITY_HIGH;

    if (LL_TIM_OC_Init(LED_740NM_TIMER, LED_740NM_TIMER_CHANNEL, &oc_init) !=
        SUCCESS) {
        LOG_ERR("Could not initialize timer channel output");
        return -EIO;
    }

    LL_TIM_SetOnePulseMode(LED_740NM_TIMER, LL_TIM_ONEPULSEMODE_REPETITIVE);

    LL_TIM_SetUpdateSource(LED_740NM_TIMER, LL_TIM_UPDATESOURCE_COUNTER);

    LL_TIM_SetSlaveMode(LED_740NM_TIMER,
                        LL_TIM_SLAVEMODE_COMBINED_RESETTRIGGER);
    static_assert(CAMERA_TRIGGER_TIMER == TIM3,
                  "The slave mode trigger input source needs to be changed "
                  "here if CAMERA_TRIGGER_TIMER is not longer timer 3");

    LL_TIM_SetTriggerInput(LED_740NM_TIMER,
                           LL_TIM_TS_ITR2); // timer 3

    LL_TIM_OC_EnablePreload(LED_740NM_TIMER, LED_740NM_TIMER_CHANNEL);

    LL_TIM_EnableARRPreload(LED_740NM_TIMER);

    LL_TIM_EnableCounter(LED_740NM_TIMER);

    return 0;
}

static void
setup_timer_settings_change()
{
    static struct ir_camera_timer_settings old_timer_settings = {0};

    // Auto-reload preload is enabled. This means that the auto-reload preload
    // register is deposited into the auto-reload register only on a timer
    // update, which will never occur if the auto-reload value was previously
    // zero. So in that case, we manually issue an update event.
    if (old_timer_settings.fps == 0) {
        set_fps_isr_body();
        LL_TIM_GenerateEvent_UPDATE(CAMERA_TRIGGER_TIMER);
    } else {
        // Update all parameters right after next timer update
        LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
        LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
    }

    old_timer_settings = global_timer_settings;
}

ret_code_t
ir_camera_system_set_on_time_740nm_us(uint16_t on_time_us)
{
    static_assert(
        LED_740NM_TIMER_CHANNEL == LL_TIM_CHANNEL_CH2,
        "Need to change the function call here to the new 740nm timer channel");

    ret_code_t ret = timer_740nm_ccr_from_on_time_us(
        on_time_us, &global_timer_settings, &global_timer_settings);

    if (ret == RET_SUCCESS) {
        setup_timer_settings_change();
        LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    }

    return ret;
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
            LOG_ERR("Error setting FPS");
        } else {
            setup_timer_settings_change();
            LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
        }
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
            LOG_ERR("Error setting on-time");
        } else {
            setup_timer_settings_change();
            LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
        }
    }

    return ret;
}

#define CONFIG_HRTIMER_EVENT(event, source)                                    \
    LL_HRTIM_EE_SetSrc(HR_TIMER, event, source);                               \
    LL_HRTIM_EE_SetSensitivity(HR_TIMER, event,                                \
                               LL_HRTIM_EE_SENSITIVITY_RISINGEDGE)

// TODO decouple hrtim1 initialization from here, as hrtim1 is now shared
// between ir_camera_system and liquid_lens.
static int
setup_940nm_850nm_common(void)
{
    LL_HRTIM_ConfigDLLCalibration(HR_TIMER,
                                  LL_HRTIM_DLLCALIBRATION_MODE_CONTINUOUS,
                                  LL_HRTIM_DLLCALIBRATION_RATE_3);
    LL_HRTIM_StartDLLCalibration(HR_TIMER);

    while (!LL_HRTIM_IsActiveFlag_DLLRDY(HR_TIMER))
        ;

    LOG_INF("Calibration complete");

    LL_HRTIM_TIM_CounterDisable(HR_TIMER, LL_HRTIM_TIMER_C); // 940nm timer
    LL_HRTIM_TIM_CounterDisable(HR_TIMER, LL_HRTIM_TIMER_D); // 850nm timer

    SET_OUTPUT_EVENT_SOURCE_CONFIG(LL_HRTIM_OUTPUTSET_NONE);

    LL_HRTIM_EnableOutput(HR_TIMER, LL_HRTIM_OUTPUT_TD2);
    LL_HRTIM_EnableOutput(HR_TIMER, LL_HRTIM_OUTPUT_TD1);
    LL_HRTIM_EnableOutput(HR_TIMER, LL_HRTIM_OUTPUT_TC1);
    LL_HRTIM_EnableOutput(HR_TIMER, LL_HRTIM_OUTPUT_TC2);

    CONFIG_HRTIMER_EVENT(LL_HRTIM_EVENT_3, LL_HRTIM_EEV3SRC_TIM3_TRGO);

    LL_HRTIM_TIM_CounterEnable(HR_TIMER, LL_HRTIM_TIMER_C);
    LL_HRTIM_TIM_CounterEnable(HR_TIMER, LL_HRTIM_TIMER_D);

    LL_TIM_InitTypeDef init;

    LL_TIM_StructInit(&init);

    init.Prescaler = 0;
    init.CounterMode = LL_TIM_COUNTERMODE_UP;
    init.Autoreload = 0;
    init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;

    if (LL_TIM_Init(CLEAR_TIMER, &init) != SUCCESS) {
        LOG_ERR("Could not initialize HR clear timer");
        return -EIO;
    }

    if (IS_TIM_BREAK_INSTANCE(CLEAR_TIMER)) {
        LL_TIM_EnableAllOutputs(CLEAR_TIMER);
    }

    LL_TIM_SetOnePulseMode(CLEAR_TIMER, LL_TIM_ONEPULSEMODE_SINGLE);

    LL_TIM_SetUpdateSource(CLEAR_TIMER, LL_TIM_UPDATESOURCE_COUNTER);

    LL_TIM_SetSlaveMode(CLEAR_TIMER, LL_TIM_SLAVEMODE_COMBINED_RESETTRIGGER);
    static_assert(CAMERA_TRIGGER_TIMER == TIM3,
                  "The slave mode trigger input source needs to be changed "
                  "here if CAMERA_TRIGGER_TIMER is not longer timer 3");
    LL_TIM_SetTriggerInput(CLEAR_TIMER, LL_TIM_TS_ITR2); // timer 3

    IRQ_DIRECT_CONNECT(CLEAR_TIMER_IRQn, 2, hrtim_clear_isr, 0);
    irq_enable(CLEAR_TIMER_IRQn);
    LL_TIM_EnableIT_UPDATE(CLEAR_TIMER);

    return 0;
}

void
ir_camera_system_enable_leds(InfraredLEDs_Wavelength wavelength)
{
    enabled_led_wavelength = wavelength;
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
}

InfraredLEDs_Wavelength
ir_camera_system_get_enabled_leds(void)
{
    return enabled_led_wavelength;
}

void
ir_camera_system_enable_ir_eye_camera(void)
{
    enable_ir_eye_camera = true;
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
}

void
ir_camera_system_disable_ir_eye_camera(void)
{
    enable_ir_eye_camera = false;
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
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
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
}

void
ir_camera_system_disable_ir_face_camera(void)
{
    enable_ir_face_camera = false;
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
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
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
}

void
ir_camera_system_disable_2d_tof_camera(void)
{
    enable_2d_tof_camera = false;
    LL_TIM_ClearFlag_UPDATE(CAMERA_TRIGGER_TIMER);
    LL_TIM_EnableIT_UPDATE(CAMERA_TRIGGER_TIMER);
}

bool
ir_camera_system_2d_tof_camera_is_enabled(void)
{
    return enable_2d_tof_camera;
}

ret_code_t
ir_camera_system_init(void)
{
    int err_code = 0;

    err_code = enable_clocks_and_configure_pins();
    if (err_code < 0) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    err_code = setup_740nm();
    if (err_code < 0) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    err_code = setup_940nm_850nm_common();
    if (err_code < 0) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    err_code = setup_camera_triggers();
    if (err_code < 0) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}
