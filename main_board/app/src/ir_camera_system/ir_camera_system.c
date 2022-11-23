#include "ir_camera_system.h"
#include "1d_tof/tof_1d.h"
#include "ir_camera_timer_settings.h"
#include <app_assert.h>
#include <assert.h>
#include <soc.h>
#include <stm32_ll_hrtim.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <stm32_timer_utils/stm32_timer_utils.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ir_camera_system, CONFIG_IR_CAMERA_SYSTEM_LOG_LEVEL);

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
static_assert(
    DT_PROP_LEN(TOF_NODE, channels) == 1,
    "For tof_2d_camera_trigger, we expect one channel in the device tree node");
static_assert(DT_PROP_LEN(TOF_NODE, pinctrl_0) == 1,
              "For tof_2d_camera_trigger, we expect the pinctrl-0 property to "
              "contain one entry in the device tree node");
static struct stm32_pclken tof_2d_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(tof_2d_camera_trigger));
#define TOF_2D_CAMERA_TRIGGER_TIMER                                            \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(TOF_NODE)))
#define TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL                                    \
    (DT_PROP_BY_IDX(TOF_NODE, channels, 0))

// START --- IR eye camera trigger
#define IR_EYE_CAMERA_NODE DT_NODELABEL(ir_eye_camera_trigger)
PINCTRL_DT_DEFINE(IR_EYE_CAMERA_NODE);
static_assert(
    DT_PROP_LEN(IR_EYE_CAMERA_NODE, channels) == 1,
    "For ir_eye_camera_trigger, we expect one channel in the device tree node");
static_assert(DT_PROP_LEN(IR_EYE_CAMERA_NODE, pinctrl_0) == 1,
              "For ir_eye_camera_trigger, we expect the pinctrl-0 property to "
              "contain one entry in the device tree node");
static struct stm32_pclken ir_eye_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(ir_eye_camera_trigger));
#define IR_EYE_CAMERA_TRIGGER_TIMER                                            \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(IR_EYE_CAMERA_NODE)))
#define IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL                                    \
    (DT_PROP_BY_IDX(IR_EYE_CAMERA_NODE, channels, 0))
// END --- IR eye

// START --- IR face camera trigger
#define IR_FACE_CAMERA_NODE DT_NODELABEL(ir_face_camera_trigger)
PINCTRL_DT_DEFINE(IR_FACE_CAMERA_NODE);
static_assert(DT_PROP_LEN(IR_FACE_CAMERA_NODE, channels) == 1,
              "For ir_face_camera_trigger, we expect one channel in the device "
              "tree node");
static_assert(DT_PROP_LEN(IR_FACE_CAMERA_NODE, pinctrl_0) == 1,
              "For ir_face_camera_trigger, we expect the pinctrl-0 property to "
              "contain one entry in the device tree node");
static struct stm32_pclken ir_face_camera_trigger_pclken =
    DT_INST_CLK(DT_NODELABEL(ir_face_camera_trigger));
#define IR_FACE_CAMERA_TRIGGER_TIMER                                           \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(IR_FACE_CAMERA_NODE)))
#define IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL                                   \
    (DT_PROP_BY_IDX(IR_FACE_CAMERA_NODE, channels, 0))
// END --- IR face

// AKA: TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER ==
// IR_FACE_CAMERA_TRIGGER_TIMER
static_assert(TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER &&
                  IR_EYE_CAMERA_TRIGGER_TIMER == IR_FACE_CAMERA_TRIGGER_TIMER,
              "We expect that all camera triggers are different channels on "
              "the same timer");

#define CAMERA_TRIGGER_TIMER IR_FACE_CAMERA_TRIGGER_TIMER
#define CAMERA_TRIGGER_TIMER_IRQn                                              \
    DT_IRQ_BY_NAME(DT_PARENT(IR_FACE_CAMERA_NODE), up, irq)

// START --- 850nm LEDs
#define LED_850NM_NODE DT_NODELABEL(led_850nm)
PINCTRL_DT_DEFINE(LED_850NM_NODE);
static_assert(
    DT_PROP_LEN(LED_850NM_NODE, channels) == 2,
    "For the 850nm LED, we expect two channels in the device tree node");
static_assert(DT_PROP_LEN(LED_850NM_NODE, pinctrl_0) == 2,
              "For the 850nm LED, we expect the pinctrl-0 property to contain "
              "two entries in the device tree node");
static struct stm32_pclken led_850nm_pclken =
    DT_INST_CLK(DT_NODELABEL(led_850nm));
#define LED_850NM_TIMER ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_850NM_NODE)))
#define LED_850NM_TIMER_LEFT_CHANNEL                                           \
    (DT_PROP_BY_IDX(LED_850NM_NODE, channels, 0))
#define LED_850NM_TIMER_RIGHT_CHANNEL                                          \
    (DT_PROP_BY_IDX(LED_850NM_NODE, channels, 1))
// END --- 850nm LEDs

// START --- 940nm LED
#define LED_940NM_NODE DT_NODELABEL(led_940nm)
PINCTRL_DT_DEFINE(LED_940NM_NODE);
static_assert(
    DT_PROP_LEN(LED_940NM_NODE, channels) == 2,
    "For the 940nm LED, we expect two channels in the device tree node");
static_assert(DT_PROP_LEN(LED_940NM_NODE, pinctrl_0) == 2,
              "For the 940nm LED, we expect the pinctrl-0 property to contain "
              "two entries in the device tree node");
static struct stm32_pclken led_940nm_pclken = DT_INST_CLK(LED_940NM_NODE);
#define LED_940NM_TIMER ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_940NM_NODE)))
#define LED_940NM_TIMER_LEFT_CHANNEL                                           \
    (DT_PROP_BY_IDX(LED_940NM_NODE, channels, 0))
#define LED_940NM_TIMER_RIGHT_CHANNEL                                          \
    (DT_PROP_BY_IDX(LED_940NM_NODE, channels, 1))
// END --- 940nm LED

// START --- 740nm LED
#define LED_740NM_NODE DT_NODELABEL(led_740nm)
PINCTRL_DT_DEFINE(LED_740NM_NODE);
static_assert(
    DT_PROP_LEN(LED_740NM_NODE, channels) == 1,
    "For the 740nm LED, we expect one channel in the device tree node");
static_assert(DT_PROP_LEN(LED_740NM_NODE, pinctrl_0) == 1,
              "For the 740nm LED, we expect the pinctrl-0 property to contain "
              "one entry in the device tree node");

static struct stm32_pclken led_740nm_pclken = DT_INST_CLK(LED_740NM_NODE);
#define LED_740NM_TIMER         ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_740NM_NODE)))
#define LED_740NM_TIMER_CHANNEL (DT_PROP_BY_IDX(LED_740NM_NODE, channels, 0))
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

static const struct pinctrl_dev_config *pin_controls[] = {
    PINCTRL_DT_DEV_CONFIG_GET(LED_850NM_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(LED_740NM_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(LED_940NM_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(TOF_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(IR_EYE_CAMERA_NODE),
    PINCTRL_DT_DEV_CONFIG_GET(IR_FACE_CAMERA_NODE),
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

/// Drive super capacitors charging mode:
///    * physical low: usage of PWM which allows for fast response to massive
///    power draw by the IR LEDs, drawback is a passive draw of 2 by default
///    forced by hardware when disconnected
///    * physical high: diode-emulation mode, still charge super caps but
///    doesn't allow high power demands. This mode is set during boot, see
///    `ir_camera_system_init`
static const struct gpio_dt_spec super_caps_charging_mode =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), super_caps_charging_mode_gpios);

static bool
ir_leds_are_on(void)
{
    if (enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_NONE) {
        return false;
    } else if (enabled_led_wavelength ==
               InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
        return (global_timer_settings.fps > 0) &&
               (global_timer_settings.ccr_740nm > 0);
    } else {
        return (global_timer_settings.fps > 0) &&
               (global_timer_settings.ccr > 0);
    }
}

static void
print_wavelength(void)
{
    const char *s = "";
    switch (enabled_led_wavelength) {
    case InfraredLEDs_Wavelength_WAVELENGTH_940NM_RIGHT:;
        s = "940nm R";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_940NM_LEFT:;
        s = "940nm L";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_940NM:;
        s = "940nm LR";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_850NM_RIGHT:;
        s = "850nm R";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_850NM_LEFT:;
        s = "850nm L";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_850NM:;
        s = "850nm LR";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_740NM:;
        s = "740nm";
        break;
    case InfraredLEDs_Wavelength_WAVELENGTH_NONE:;
        s = "None";
        break;
    }

    LOG_DBG("%s", s);
}

static void
print_ir_camera_triggering(void)
{
    LOG_DBG("IR eye? %c", enable_ir_eye_camera ? 'y' : 'n');
    LOG_DBG("IR face? %c", enable_ir_face_camera ? 'y' : 'n');
    LOG_DBG("2dtof? %c", enable_2d_tof_camera ? 'y' : 'n');
}

static void
print_ir_leds_are_on(void)
{
    LOG_DBG("%c", ir_leds_are_on() ? 'y' : 'n');
}

static void
debug_print(void)
{
    timer_settings_print(&global_timer_settings);
    print_wavelength();
    print_ir_leds_are_on();
    print_ir_camera_triggering();
}

#define IR_LED_AUTO_OFF_TIMEOUT_S (60 * 3)

static void
disable_ir_leds()
{
    LOG_WRN("Turning off IR LEDs after %" PRIu32 "s of inactivity",
            IR_LED_AUTO_OFF_TIMEOUT_S);
    ir_camera_system_enable_leds(InfraredLEDs_Wavelength_WAVELENGTH_NONE);
}

static void
configure_timeout(void)
{
    static K_TIMER_DEFINE(ir_leds_auto_off_timer, disable_ir_leds, NULL);

    if (ir_leds_are_on()) {
        // one-shot
        // starting an already started timer will simply reset it
        k_timer_start(&ir_leds_auto_off_timer,
                      K_SECONDS(IR_LED_AUTO_OFF_TIMEOUT_S), K_NO_WAIT);
        LOG_DBG("Resetting timeout (%" PRIu32 "s).", IR_LED_AUTO_OFF_TIMEOUT_S);
    } else {
        // stopping an already stopped timer is ok and has no effect.
        k_timer_stop(&ir_leds_auto_off_timer);
    }
}

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
    int ret;
    zero_led_ccrs();

    // activate super caps charger for high demand when driving IR LEDs
    // from logic low to logic high
    if (enabled_led_wavelength != InfraredLEDs_Wavelength_WAVELENGTH_NONE &&
        gpio_pin_get_dt(&super_caps_charging_mode) == 0) {
        ret = gpio_pin_configure_dt(&super_caps_charging_mode,
                                    GPIO_OUTPUT_ACTIVE);
        ASSERT_SOFT(ret);

        LOG_INF("Super caps charger set for high power demand");

        // time to settle before driving LEDs
        k_msleep(1);
    }

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
        break;
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
        if (gpio_pin_get_dt(&super_caps_charging_mode) == 1) {
            LOG_INF("Super caps charger set for low power demand");
            ret = gpio_pin_configure_dt(&super_caps_charging_mode,
                                        GPIO_OUTPUT_INACTIVE);
            ASSERT_SOFT(ret);
        }
        break;
    }
}

#define TRIGGER_PULSE_WIDTH_US 15

static inline void
set_trigger_cc(bool enabled, int channel)
{
    if (enabled && global_timer_settings.fps > 0) {
        uint16_t ccr =
            ((TRIGGER_PULSE_WIDTH_US * ASSUMED_TIMER_CLOCK_FREQ_MHZ) /
             (global_timer_settings.psc + 1)) +
            1;
        set_timer_compare[channel - 1](CAMERA_TRIGGER_TIMER, ccr);
    } else {
        set_timer_compare[channel - 1](CAMERA_TRIGGER_TIMER, 0);
    }
}

static void
apply_new_timer_settings()
{
    static struct ir_camera_timer_settings old_timer_settings = {0};

    CRITICAL_SECTION_ENTER(k);

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

    CRITICAL_SECTION_EXIT(k);

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
    set_trigger_cc(enable_ir_eye_camera, IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_disable_ir_eye_camera(void)
{
    enable_ir_eye_camera = false;
    set_trigger_cc(enable_ir_eye_camera, IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
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
    set_trigger_cc(enable_ir_face_camera, IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_disable_ir_face_camera(void)
{
    enable_ir_face_camera = false;
    set_trigger_cc(enable_ir_face_camera, IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
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
    set_trigger_cc(enable_2d_tof_camera, TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_disable_2d_tof_camera(void)
{
    enable_2d_tof_camera = false;
    set_trigger_cc(enable_2d_tof_camera, TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
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

    if (!device_is_ready(super_caps_charging_mode.port)) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    // super caps charger to draw less current than default
    // this mode is enabled when IR LEDs are not used
    int ret =
        gpio_pin_configure_dt(&super_caps_charging_mode, GPIO_OUTPUT_INACTIVE);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    err_code = enable_clocks_and_configure_pins(
        all_pclken, ARRAY_SIZE(all_pclken), pin_controls,
        ARRAY_SIZE(pin_controls));

    if (err_code < 0) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    err_code = setup_740nm_940nm_led_timer();
    if (err_code < 0) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    err_code = setup_850nm_led_timer();
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
        debug_print();
        configure_timeout();
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
        debug_print();
        configure_timeout();
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

    debug_print();
    configure_timeout();

    return ret;
}

void
ir_camera_system_enable_leds(InfraredLEDs_Wavelength wavelength)
{
    CRITICAL_SECTION_ENTER(k);

    enabled_led_wavelength = wavelength;

    if (enabled_led_wavelength == InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr / 2);
    } else {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr);
    }

    set_ccr_ir_leds();

    CRITICAL_SECTION_EXIT(k);

    debug_print();
    configure_timeout();
}

InfraredLEDs_Wavelength
ir_camera_system_get_enabled_leds(void)
{
    return enabled_led_wavelength;
}
