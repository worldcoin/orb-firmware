#include "ir_camera_system.h"

#define IR_CAMERA_SYSTEM_INTERNAL_USE

#include "ir_camera_system_hw.h"
#include "ir_camera_system_internal.h"
#include "ir_camera_timer_settings.h"
#include "optics/1d_tof/tof_1d.h"
#include "optics/liquid_lens/liquid_lens.h"
#include <app_assert.h>
#include <app_config.h>
#include <assert.h>
#include <math.h>
#include <optics/mirrors/mirrors.h>
#include <soc.h>
#include <stm32_ll_hrtim.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_tim.h>
#include <system/stm32_timer_utils/stm32_timer_utils.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>

#if defined(CONFIG_ZTEST)
#include <zephyr/logging/log.h>
#else
#include "logs_can.h"
#endif
LOG_MODULE_DECLARE(ir_camera_system, CONFIG_IR_CAMERA_SYSTEM_LOG_LEVEL);

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
BUILD_ASSERT(
    DT_PROP_LEN(TOF_NODE, channels) == 1,
    "For tof_2d_camera_trigger, we expect one channel in the device tree node");
BUILD_ASSERT(DT_PROP_LEN(TOF_NODE, pinctrl_0) == 1,
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
BUILD_ASSERT(
    DT_PROP_LEN(IR_EYE_CAMERA_NODE, channels) == 1,
    "For ir_eye_camera_trigger, we expect one channel in the device tree node");
BUILD_ASSERT(DT_PROP_LEN(IR_EYE_CAMERA_NODE, pinctrl_0) == 1,
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
BUILD_ASSERT(DT_PROP_LEN(IR_FACE_CAMERA_NODE, channels) == 1,
             "For ir_face_camera_trigger, we expect one channel in the device "
             "tree node");
BUILD_ASSERT(DT_PROP_LEN(IR_FACE_CAMERA_NODE, pinctrl_0) == 1,
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
BUILD_ASSERT(TOF_2D_CAMERA_TRIGGER_TIMER == IR_EYE_CAMERA_TRIGGER_TIMER &&
                 IR_EYE_CAMERA_TRIGGER_TIMER == IR_FACE_CAMERA_TRIGGER_TIMER,
             "We expect that all camera triggers are different channels on "
             "the same timer");

#define CAMERA_TRIGGER_TIMER IR_FACE_CAMERA_TRIGGER_TIMER
#define CAMERA_TRIGGER_TIMER_CC_IRQn                                           \
    DT_IRQ_BY_NAME(DT_PARENT(IR_FACE_CAMERA_NODE), cc, irq)

// START --- 850nm LEDs
#define LED_850NM_NODE DT_NODELABEL(led_850nm)
PINCTRL_DT_DEFINE(LED_850NM_NODE);
BUILD_ASSERT(
    DT_PROP_LEN(LED_850NM_NODE, channels) == 2,
    "For the 850nm LED, we expect two channels in the device tree node");
BUILD_ASSERT(DT_PROP_LEN(LED_850NM_NODE, pinctrl_0) == 2,
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
BUILD_ASSERT(
    DT_PROP_LEN(LED_940NM_NODE, channels) == 2,
    "For the 940nm LED, we expect two channels in the device tree node");
BUILD_ASSERT(DT_PROP_LEN(LED_940NM_NODE, pinctrl_0) == 2,
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
BUILD_ASSERT(
    DT_PROP_LEN(LED_740NM_NODE, channels) == 1,
    "For the 740nm LED, we expect one channel in the device tree node");
BUILD_ASSERT(DT_PROP_LEN(LED_740NM_NODE, pinctrl_0) == 1,
             "For the 740nm LED, we expect the pinctrl-0 property to contain "
             "one entry in the device tree node");

static struct stm32_pclken led_740nm_pclken = DT_INST_CLK(LED_740NM_NODE);
#define LED_740NM_TIMER         ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(LED_740NM_NODE)))
#define LED_740NM_TIMER_CHANNEL (DT_PROP_BY_IDX(LED_740NM_NODE, channels, 0))
// END --- 740nm LED

BUILD_ASSERT(LED_740NM_TIMER == LED_940NM_TIMER,
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

/* Channel to enable ccr interrupt function */
static void (*const enable_ccr_interrupt[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_EnableIT_CC1, LL_TIM_EnableIT_CC2, LL_TIM_EnableIT_CC3,
    LL_TIM_EnableIT_CC4};

/* Channel to disable ccr interrupt function */
static void (*const disable_ccr_interrupt[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_DisableIT_CC1, LL_TIM_DisableIT_CC2, LL_TIM_DisableIT_CC3,
    LL_TIM_DisableIT_CC4};

/* Channel to clear ccr interrupt flag function */
static void (*const clear_ccr_interrupt_flag[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_ClearFlag_CC1, LL_TIM_ClearFlag_CC2, LL_TIM_ClearFlag_CC3,
    LL_TIM_ClearFlag_CC4};

static void
zero_led_ccrs(void)
{
    set_timer_compare[LED_850NM_TIMER_LEFT_CHANNEL - 1](LED_850NM_TIMER, 0);
    set_timer_compare[LED_850NM_TIMER_RIGHT_CHANNEL - 1](LED_850NM_TIMER, 0);
    set_timer_compare[LED_940NM_TIMER_LEFT_CHANNEL - 1](LED_940NM_TIMER, 0);
    set_timer_compare[LED_940NM_TIMER_RIGHT_CHANNEL - 1](LED_940NM_TIMER, 0);
    set_timer_compare[LED_740NM_TIMER_CHANNEL - 1](LED_740NM_TIMER, 0);
}

static struct ir_camera_timer_settings global_timer_settings;

/// Drive super capacitors charging mode:
///    * physical low: usage of PWM which allows for fast response to massive
///    power draw by the IR LEDs, drawback is a passive draw of 2 by default
///    forced by hardware when disconnected
///    * physical high: diode-emulation mode, still charge super caps but
///    doesn't allow high power demands. This mode is set during boot, see
///    `ir_camera_system_init`
static const struct gpio_dt_spec super_caps_charging_mode =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), super_caps_charging_mode_gpios);

// Focus sweep stuff
static int16_t global_focus_values[MAX_NUMBER_OF_FOCUS_VALUES];
static size_t global_num_focus_values;
static volatile size_t sweep_index;
static bool use_focus_sweep_polynomial;
static IREyeCameraFocusSweepValuesPolynomial focus_sweep_polynomial;

void
ir_camera_system_set_polynomial_coefficients_for_focus_sweep_hw(
    IREyeCameraFocusSweepValuesPolynomial poly)
{
    use_focus_sweep_polynomial = true;
    global_num_focus_values = poly.number_of_frames;
    focus_sweep_polynomial = poly;
}

void
ir_camera_system_set_focus_values_for_focus_sweep_hw(int16_t *focus_values,
                                                     size_t num_focus_values)
{
    global_num_focus_values = num_focus_values;
    memcpy(global_focus_values, focus_values, sizeof(global_focus_values));
    use_focus_sweep_polynomial = false;
}

static int32_t
evaluate_focus_sweep_polynomial(uint32_t frame_no)
{
    // We are evaluating this formula:
    // focus(n) = a + bn + cn^2 + dn^3 + en^4 + fn^5
    //
    // Transforming the formula using Horner's rule we get:
    // f(x0) = a + x0(b + x0(c + x0(d + x0(e + fx0))))
    //
    // Using Horner's rule reduces the number of multiplications
    return lroundf(
        focus_sweep_polynomial.coef_a +
        (frame_no *
         (focus_sweep_polynomial.coef_b +
          frame_no * (focus_sweep_polynomial.coef_c +
                      frame_no * (focus_sweep_polynomial.coef_d +
                                  frame_no * (focus_sweep_polynomial.coef_e +
                                              (focus_sweep_polynomial.coef_f *
                                               frame_no)))))));
}

// In milli-degrees
struct mirror_delta {
    int32_t delta_x;
    int32_t delta_y;
};

// Mirror sweep stuff
static IREyeCameraMirrorSweepValuesPolynomial mirror_sweep_polynomial;
static int32_t initial_mirror_y_pos, initial_mirror_x_pos;

void
ir_camera_system_set_polynomial_coefficients_for_mirror_sweep_hw(
    IREyeCameraMirrorSweepValuesPolynomial poly)
{
    mirror_sweep_polynomial = poly;
}

static struct mirror_delta
evaluate_mirror_sweep_polynomials(uint32_t frame_no)
{
    struct mirror_delta md = {0};

    float radius =
        mirror_sweep_polynomial.radius_coef_a +
        (frame_no * (mirror_sweep_polynomial.radius_coef_b +
                     (frame_no * mirror_sweep_polynomial.radius_coef_c)));
    float angle =
        mirror_sweep_polynomial.angle_coef_a +
        (frame_no * (mirror_sweep_polynomial.angle_coef_b +
                     (frame_no * mirror_sweep_polynomial.angle_coef_c)));

    md.delta_x = (int32_t)(radius * sinf(angle)) * 1000;
    md.delta_y = (int32_t)(radius * cosf(angle)) * 1000;

    return md;
}

#ifdef HIL_TEST
K_SEM_DEFINE(camera_sweep_sem, 0, 1);
#endif

static void
camera_sweep_isr(void *arg)
{
    ARG_UNUSED(arg);

    clear_ccr_interrupt_flag[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER);

    if (get_focus_sweep_in_progress() == true) {
        if (sweep_index == global_num_focus_values) {
            disable_ccr_interrupt[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
                CAMERA_TRIGGER_TIMER);
            LOG_DBG("Focus sweep complete!");
            ir_camera_system_disable_ir_eye_camera_force();
            clear_focus_sweep_in_progress();
#ifdef HIL_TEST
            k_sem_give(&camera_sweep_sem);
#endif
        } else {
            if (use_focus_sweep_polynomial) {
                liquid_set_target_current_ma(
                    evaluate_focus_sweep_polynomial(sweep_index));
            } else {
                liquid_set_target_current_ma(global_focus_values[sweep_index]);
            }
        }
    } else if (get_mirror_sweep_in_progress() == true) {
        if (sweep_index == mirror_sweep_polynomial.number_of_frames) {
            disable_ccr_interrupt[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
                CAMERA_TRIGGER_TIMER);
            LOG_DBG("Mirror sweep complete!");
            ir_camera_system_disable_ir_eye_camera_force();
            clear_mirror_sweep_in_progress();
#ifdef HIL_TEST
            k_sem_give(&camera_sweep_sem);
#endif
        } else {
            struct mirror_delta md =
                evaluate_mirror_sweep_polynomials(sweep_index);
            mirrors_angle_horizontal_async(md.delta_x + initial_mirror_x_pos);
            mirrors_angle_vertical_async(md.delta_y + initial_mirror_y_pos);
        }
    } else {
        LOG_ERR("Nothing is in progress, this should not be possible!");
    }

    sweep_index++;
}

static void
initialize_focus_sweep(void)
{
    if (use_focus_sweep_polynomial) {
        liquid_set_target_current_ma(evaluate_focus_sweep_polynomial(0));
    } else {
        liquid_set_target_current_ma(global_focus_values[0]);
    }

    sweep_index = 1;

    clear_ccr_interrupt_flag[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER);
    enable_ccr_interrupt[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER);

    LOG_DBG("Starting focus sweep!");

    ir_camera_system_enable_ir_eye_camera_force();
}

void
ir_camera_system_perform_focus_sweep_hw(void)
{
    LOG_DBG("Initializing focus sweep.");
    LOG_DBG("Taking %zu focus sweep frames", global_num_focus_values);
    // No focus values means we trivially succeed
    if (global_num_focus_values > 0) {
        set_focus_sweep_in_progress();
        initialize_focus_sweep();
    } else {
        LOG_WRN("Num focus values is 0!");
    }
}

static void
initialize_mirror_sweep(void)
{
    sweep_index = 0;
    initial_mirror_x_pos = mirrors_get_horizontal_position();
    initial_mirror_y_pos = mirrors_get_vertical_position();

    LOG_DBG("Initial mirror x pos: %d", initial_mirror_x_pos);
    LOG_DBG("Initial mirror y pos: %d", initial_mirror_y_pos);

    clear_ccr_interrupt_flag[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER);
    enable_ccr_interrupt[IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL - 1](
        CAMERA_TRIGGER_TIMER);

    LOG_DBG("Starting mirror sweep!");

    ir_camera_system_enable_ir_eye_camera_force();
}

void
ir_camera_system_perform_mirror_sweep_hw(void)
{
    LOG_DBG("Initializing mirror sweep.");
    LOG_DBG("Taking %zu mirror sweep frames",
            mirror_sweep_polynomial.number_of_frames);
    // No mirror values means we trivially succeed
    if (mirror_sweep_polynomial.number_of_frames > 0) {
        set_mirror_sweep_in_progress();
        initialize_mirror_sweep();
    } else {
        LOG_WRN("Num mirror values is 0!");
    }
}

static bool
ir_leds_are_on(void)
{
    if (ir_camera_system_get_enabled_leds() ==
        InfraredLEDs_Wavelength_WAVELENGTH_NONE) {
        return false;
    } else if (ir_camera_system_get_enabled_leds() ==
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
    switch (ir_camera_system_get_enabled_leds()) {
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
    LOG_DBG("IR eye? %c",
            ir_camera_system_ir_eye_camera_is_enabled() ? 'y' : 'n');
    LOG_DBG("IR face? %c",
            ir_camera_system_ir_face_camera_is_enabled() ? 'y' : 'n');
    LOG_DBG("2dtof? %c",
            ir_camera_system_2d_tof_camera_is_enabled() ? 'y' : 'n');
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

    IRQ_CONNECT(CAMERA_TRIGGER_TIMER_CC_IRQn, CAMERA_SWEEP_INTERRUPT_PRIO,
                camera_sweep_isr, NULL, 0);
    irq_enable(CAMERA_TRIGGER_TIMER_CC_IRQn);

    LL_TIM_EnableCounter(CAMERA_TRIGGER_TIMER);

    return 0;
}

static void
set_ccr_ir_leds(void)
{
    int ret;
    zero_led_ccrs();

    // allow usage of IR LEDs if safety conditions are met
    // this overrides Jetson commands
    if (!distance_is_safe()) {
        return;
    }

    // activate super caps charger for high demand when driving IR LEDs
    // from logic low to logic high
    if (ir_camera_system_get_enabled_leds() !=
            InfraredLEDs_Wavelength_WAVELENGTH_NONE &&
        gpio_pin_get_dt(&super_caps_charging_mode) == 0) {
        ret = gpio_pin_configure_dt(&super_caps_charging_mode,
                                    GPIO_OUTPUT_ACTIVE);
        ASSERT_SOFT(ret);

        LOG_INF("Super caps charger set for high power demand");

        // time to settle before driving LEDs
        k_msleep(1);
    }

    switch (ir_camera_system_get_enabled_leds()) {
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

static inline void
set_trigger_cc(bool enabled, int channel)
{
    if (enabled && global_timer_settings.fps > 0) {
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

    CRITICAL_SECTION_ENTER(k);

    LL_TIM_SetPrescaler(CAMERA_TRIGGER_TIMER, global_timer_settings.psc);
    LL_TIM_SetAutoReload(CAMERA_TRIGGER_TIMER, global_timer_settings.arr);

    LL_TIM_SetPrescaler(LED_850NM_TIMER, global_timer_settings.psc);
    LL_TIM_SetAutoReload(LED_850NM_TIMER, global_timer_settings.arr);

    LL_TIM_SetPrescaler(LED_740NM_940NM_COMMON_TIMER,
                        global_timer_settings.psc);
    if (ir_camera_system_get_enabled_leds() ==
        InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr / 2);
    } else {
        LL_TIM_SetAutoReload(LED_740NM_940NM_COMMON_TIMER,
                             global_timer_settings.arr);
    }

    set_trigger_cc(ir_camera_system_ir_eye_camera_is_enabled(),
                   IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL);
    set_trigger_cc(ir_camera_system_ir_face_camera_is_enabled(),
                   IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL);
    set_trigger_cc(ir_camera_system_2d_tof_camera_is_enabled(),
                   TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL);

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
ir_camera_system_enable_ir_eye_camera_hw(void)
{
    set_trigger_cc(true, IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_disable_ir_eye_camera_hw(void)
{
    set_trigger_cc(false, IR_EYE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_enable_ir_face_camera_hw(void)
{
    set_trigger_cc(true, IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_disable_ir_face_camera_hw(void)
{
    set_trigger_cc(false, IR_FACE_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_enable_2d_tof_camera_hw(void)
{
    set_trigger_cc(true, TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

void
ir_camera_system_disable_2d_tof_camera_hw(void)
{
    set_trigger_cc(false, TOF_2D_CAMERA_TRIGGER_TIMER_CHANNEL);
    debug_print();
}

ret_code_t
ir_camera_system_hw_init(void)
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
ir_camera_system_set_fps_hw(uint16_t fps)
{
    ret_code_t ret;

    ret = timer_settings_from_fps(fps, &global_timer_settings,
                                  &global_timer_settings);
    if (ret != RET_SUCCESS) {
        LOG_ERR("Error setting new FPS");
    } else {
        apply_new_timer_settings();
    }

    debug_print();
    configure_timeout();

    return ret;
}

ret_code_t
ir_camera_system_set_on_time_us_hw(uint16_t on_time_us)
{
    ret_code_t ret;

    ret = timer_settings_from_on_time_us(on_time_us, &global_timer_settings,
                                         &global_timer_settings);
    if (ret != RET_SUCCESS) {
        LOG_ERR("Error setting new on-time");
    } else {
        apply_new_timer_settings();
    }

    debug_print();
    configure_timeout();

    return ret;
}

ret_code_t
ir_camera_system_set_on_time_740nm_us_hw(uint16_t on_time_us)
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
ir_camera_system_enable_leds_hw(void)
{
    CRITICAL_SECTION_ENTER(k);

    if (ir_camera_system_get_enabled_leds() ==
        InfraredLEDs_Wavelength_WAVELENGTH_740NM) {
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

uint16_t
ir_camera_system_get_fps_hw(void)
{
    return global_timer_settings.fps;
}
