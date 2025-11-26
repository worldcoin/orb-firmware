/******************************************************************************
 * @file polarizer_wheel.c
 * @brief Source file for Polarizer Wheel functions
 *
 * This file defines the application level interface functions for the Polarizer
 * Wheel such as initialize, configure, and control
 *
 ******************************************************************************/
#include "polarizer_wheel.h"
#include "drv8434s/drv8434s.h"
#include "orb_state.h"
#include <app_assert.h>
#include <app_config.h>
#include <common.pb.h>
#include <math.h>
#include <stdlib.h>
#include <stm32g474xx.h>
#include <stm32g4xx_ll_tim.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>

#include "orb_logs.h"

LOG_MODULE_REGISTER(polarizer, CONFIG_POLARIZER_LOG_LEVEL);
ORB_STATE_REGISTER(polarizer);

K_THREAD_STACK_DEFINE(stack_area_polarizer_wheel_home,
                      THREAD_STACK_SIZE_POLARIZER_WHEEL_HOME);
static struct k_thread thread_data_polarizer_wheel_home;

enum polarizer_wheel_direction_e {
    POLARIZER_WHEEL_DIRECTION_BACKWARD = -1,
    POLARIZER_WHEEL_DIRECTION_FORWARD = 1,
};

typedef struct {
    struct {
        uint8_t notch_count;
        bool success;
    } homing;

    struct {
        // microsteps into range: [0; POLARIZER_WHEEL_MICROSTEPS_360_DEGREES]
        atomic_t current;
        atomic_t target;
        enum polarizer_wheel_direction_e direction;
    } step_count;
} polarizer_wheel_instance_t;

static polarizer_wheel_instance_t g_polarizer_wheel_instance = {0};

// Peripherals (Motor Driver SPI, motor driver enable, encoder enable, encoder
// feedback, step PWM)
static const struct device *polarizer_spi_bus_controller =
    DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(polarizer_controller)));
static const struct gpio_dt_spec polarizer_spi_cs_gpio =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_spi_cs_gpios);
static const struct pwm_dt_spec polarizer_step_pwm_spec_evt =
    PWM_DT_SPEC_GET(DT_PATH(polarizer_step_evt));
static const struct pwm_dt_spec polarizer_step_pwm_spec_dvt =
    PWM_DT_SPEC_GET(DT_PATH(polarizer_step));
static const struct pwm_dt_spec *polarizer_step_pwm_spec =
    &polarizer_step_pwm_spec_dvt;
static const struct gpio_dt_spec polarizer_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_enable_gpios);
static const struct gpio_dt_spec polarizer_step_dir_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_direction_gpios);
static const struct gpio_dt_spec polarizer_encoder_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user),
                     polarizer_stepper_encoder_enable_gpios);
static const struct gpio_dt_spec polarizer_encoder_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_encoder_gpios);

// timer handle and irq number
static TIM_TypeDef *polarizer_step_timer =
    (TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)));

static uint32_t pwm_timer_irq_n = COND_CODE_1(
    DT_IRQ_HAS_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)), cc),
    (DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)), cc, irq)),
    (DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)), global, irq)));

static struct gpio_callback polarizer_encoder_cb_data;

// Set up the DRV8434 driver configuration
static const DRV8434S_DriverCfg_t drv8434_cfg = {
    .spi = (struct spi_dt_spec)SPI_DT_SPEC_GET(
        DT_NODELABEL(polarizer_controller),
        SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPHA | SPI_TRANSFER_MSB,
        0),
    .spi_cs_gpio = &polarizer_spi_cs_gpio};

// if less than 550 µsteps between two notches: notch with small gap detected
// we can then go to the 0/passthrough by applying a 120º+center degree movement
#define POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MAX 550
#define POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MIN 350
#define POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS           3
#define POLARIZER_WHEEL_NOTCH_DETECT_ATTEMPTS          9

// there are 4 notches so we need a minimum of 4 notch detection attempts
BUILD_ASSERT(POLARIZER_WHEEL_NOTCH_DETECT_ATTEMPTS > 4);

static K_SEM_DEFINE(home_sem, 0, 1);

// Enable encoder interrupt
static ret_code_t
enable_encoder(void)
{
    const int ret = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                          GPIO_OUTPUT_ACTIVE);
    if (ret) {
        return ret;
    }

    return gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                           GPIO_INT_EDGE_RISING);
}

// Disable the interrupt
static ret_code_t
disable_encoder(void)
{
    const int ret = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                          GPIO_OUTPUT_INACTIVE);
    if (ret) {
        return ret;
    }

    return gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                           GPIO_INT_DISABLE);
}

// clear the polarizer wheel step interrupt flag
static ret_code_t
clear_step_interrupt(void)
{
    /* Channel to clearing capture flag mapping. */
    static void __maybe_unused (*const clear_capture_interrupt[])(
        TIM_TypeDef *) = {LL_TIM_ClearFlag_CC1, LL_TIM_ClearFlag_CC2,
                          LL_TIM_ClearFlag_CC3, LL_TIM_ClearFlag_CC4};

    clear_capture_interrupt[polarizer_step_pwm_spec->channel - 1](
        polarizer_step_timer);

    return RET_SUCCESS;
}

// Enable polarizer wheel step interrupt
static ret_code_t
enable_step_interrupt(void)
{
    /* Channel to enable capture interrupt mapping. */
    static void __maybe_unused (*const enable_capture_interrupt[])(
        TIM_TypeDef *) = {LL_TIM_EnableIT_CC1, LL_TIM_EnableIT_CC2,
                          LL_TIM_EnableIT_CC3, LL_TIM_EnableIT_CC4};

    clear_step_interrupt();
    enable_capture_interrupt[polarizer_step_pwm_spec->channel - 1](
        polarizer_step_timer);

    return RET_SUCCESS;
}

static ret_code_t
disable_step_interrupt(void)
{
    /* Channel to disable capture interrupt mapping. */
    static void __maybe_unused (*const disable_capture_interrupt[])(
        TIM_TypeDef *) = {LL_TIM_DisableIT_CC1, LL_TIM_DisableIT_CC2,
                          LL_TIM_DisableIT_CC3, LL_TIM_DisableIT_CC4};

    clear_step_interrupt();
    disable_capture_interrupt[polarizer_step_pwm_spec->channel - 1](
        polarizer_step_timer);

    return RET_SUCCESS;
}

static int
polarizer_move(const uint32_t frequency)
{
    enable_step_interrupt();
    return pwm_set_dt(polarizer_step_pwm_spec, NSEC_PER_SEC / frequency,
                      (NSEC_PER_SEC / frequency) / 2);
}

static int
polarizer_stop()
{
    const int ret = pwm_set_dt(polarizer_step_pwm_spec, 0, 0);
    disable_step_interrupt();
    return ret;
}

static int
set_direction(const enum polarizer_wheel_direction_e direction)
{
    int ret;
    switch (direction) {
    case POLARIZER_WHEEL_DIRECTION_BACKWARD:
        ret = gpio_pin_set_dt(&polarizer_step_dir_spec, 1);
        break;
    case POLARIZER_WHEEL_DIRECTION_FORWARD:
        ret = gpio_pin_set_dt(&polarizer_step_dir_spec, 0);
        break;
    default:
        ret = RET_ERROR_INVALID_PARAM;
    }

    if (ret == 0) {
        g_polarizer_wheel_instance.step_count.direction = direction;
    }

    return ret;
}

/**
 * ISR to handle gpio interrupt on notch detection
 */
static void
encoder_callback(const struct device *dev, struct gpio_callback *cb,
                 uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(polarizer_encoder_spec.pin)) {
        if (gpio_pin_get_dt(&polarizer_encoder_spec) == 1) {
            LOG_DBG("notches detected: %u",
                    g_polarizer_wheel_instance.homing.notch_count);
            /* stop early */
            polarizer_stop();
            k_sem_give(&home_sem);
        }
    }
}

/**
 * ISR to handle new pwm pulse (eq new step)
 */
static void
polarizer_wheel_step_isr(const void *arg)
{
    ARG_UNUSED(arg);

    static uint32_t __maybe_unused (*const is_capture_active[])(
        const TIM_TypeDef *) = {
        LL_TIM_IsActiveFlag_CC1, LL_TIM_IsActiveFlag_CC2,
        LL_TIM_IsActiveFlag_CC3, LL_TIM_IsActiveFlag_CC4};

    if (is_capture_active[polarizer_step_pwm_spec->channel - 1](
            polarizer_step_timer)) {
        clear_step_interrupt();

        if (g_polarizer_wheel_instance.step_count.direction ==
            POLARIZER_WHEEL_DIRECTION_BACKWARD) {
            atomic_dec(&g_polarizer_wheel_instance.step_count.current);
        } else {
            atomic_inc(&g_polarizer_wheel_instance.step_count.current);
        }

        // handle wraps around
        if (atomic_get(&g_polarizer_wheel_instance.step_count.current) >=
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES) {
            atomic_clear(&g_polarizer_wheel_instance.step_count.current);
        }
        if (atomic_get(&g_polarizer_wheel_instance.step_count.current) < 0) {
            atomic_set(&g_polarizer_wheel_instance.step_count.current,
                       POLARIZER_WHEEL_MICROSTEPS_360_DEGREES - 1);
        }

        if (g_polarizer_wheel_instance.step_count.target ==
            g_polarizer_wheel_instance.step_count.current) {
            LOG_DBG("Reached target (%lu), stopping motor",
                    g_polarizer_wheel_instance.step_count.target);
            polarizer_stop();
        }
    }
}

static ret_code_t
polarizer_wheel_step_relative(const uint32_t frequency,
                              const int32_t step_count)
{
    int ret_val = RET_SUCCESS;

    if (frequency == 0 || step_count == 0 ||
        step_count > POLARIZER_WHEEL_MICROSTEPS_360_DEGREES) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (step_count < 0) {
        set_direction(POLARIZER_WHEEL_DIRECTION_BACKWARD);
        if (atomic_get(&g_polarizer_wheel_instance.step_count.current) +
                step_count <
            0) {
            const int32_t target =
                POLARIZER_WHEEL_MICROSTEPS_360_DEGREES +
                (atomic_get(&g_polarizer_wheel_instance.step_count.current) +
                 step_count);
            atomic_set(&g_polarizer_wheel_instance.step_count.target, target);
        } else {
            const int32_t target =
                atomic_get(&g_polarizer_wheel_instance.step_count.current) +
                step_count;
            atomic_set(&g_polarizer_wheel_instance.step_count.target, target);
        }
    } else {
        set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);

        const int32_t target =
            (atomic_get(&g_polarizer_wheel_instance.step_count.current) +
             step_count) %
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
        atomic_set(&g_polarizer_wheel_instance.step_count.target, target);
    }

    ret_val = polarizer_move(frequency);

    return ret_val;
}

static void
homing_failed()
{
    g_polarizer_wheel_instance.homing.success = false;
    int ret = polarizer_stop();
    ASSERT_SOFT(ret);

    ret = disable_encoder();
    ASSERT_SOFT(ret);
}

static void
polarizer_wheel_auto_homing_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    clear_step_interrupt();

    // enable encoder interrupt to detect notches
    enable_encoder();

    /*
     * Below is a representation of the notches on the wheel (encoder):
     * |--->--|---->-|----|--| (and back)
     * 0------1------2----3--0 notch number
     * let's detect notch 0 by counting number of steps between notches when
     * going in forward direction.
     */
    bool notch_0_detected = false;
    g_polarizer_wheel_instance.homing.notch_count = 0;
    set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);
    while (!notch_0_detected && g_polarizer_wheel_instance.homing.notch_count <
                                    POLARIZER_WHEEL_NOTCH_DETECT_ATTEMPTS) {
        size_t spin_attempt = 0;
        while (spin_attempt < POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS) {
            // clear the step count before each spin attempt
            atomic_clear(&g_polarizer_wheel_instance.step_count.current);
            k_sem_reset(&home_sem);

            // spin the wheel 240º, to be done up to
            // POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS times
            int ret = polarizer_wheel_step_relative(
                POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                POLARIZER_WHEEL_MICROSTEPS_120_DEGREES * 2);
            if (ret != RET_SUCCESS) {
                LOG_ERR("Unable to spin polarizer wheel: %d, attempt %u", ret,
                        spin_attempt);
                homing_failed();
                ORB_STATE_SET_CURRENT(ret, "unable to spin");
                return;
            }
            ret = k_sem_take(&home_sem, K_SECONDS(4));
            if (ret == 0) {
                break;
            }
            spin_attempt++;
        }

        if (spin_attempt != 0) {
            LOG_WRN("Spin attempt %u, current step counter: %u", spin_attempt,
                    (uint32_t)atomic_get(
                        &g_polarizer_wheel_instance.step_count.current));
            if (spin_attempt == POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS) {
                // encoder not detected, no wheel?
                ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED,
                                      "no encoder: no wheel? staled?");
                homing_failed();
                LOG_WRN(
                    "Encoder not detected, is there a wheel? is it moving?");
                return;
            }
        }

        LOG_INF("homing: steps: %ld, notch count: %d",
                atomic_get(&g_polarizer_wheel_instance.step_count.current),
                g_polarizer_wheel_instance.homing.notch_count);

        // at the very beginning (notch_count == 0), close notches (distance
        // between notch #3 and #0), can be anywhere; so on first notch
        // detection, all we do is to reset the step counter. From that point,
        // we start counting steps between notches to detect the small gap, and
        // thus, notch #0
        if (g_polarizer_wheel_instance.homing.notch_count != 0 &&
            g_polarizer_wheel_instance.step_count.current <
                POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MAX &&
            g_polarizer_wheel_instance.step_count.current >
                POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MIN) {
            polarizer_stop();
            notch_0_detected = true;
        }

        g_polarizer_wheel_instance.homing.notch_count++;
        atomic_clear(&g_polarizer_wheel_instance.step_count.current);
    }

    if (notch_0_detected) {
        // ✅ success
        // wheel is on notch #0
        // send wheel home / passthrough by applying constant number of
        // microsteps
        int ret = polarizer_wheel_step_relative(
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
            POLARIZER_WHEEL_MICROSTEPS_NOTCH_EDGE_TO_CENTER +
                POLARIZER_WHEEL_MICROSTEPS_120_DEGREES);
        ASSERT_SOFT(ret);

        // wait for completion and disconnect interrupt
        k_sleep(K_SECONDS(4));

        ASSERT_SOFT(polarizer_stop());
        ASSERT_SOFT(disable_encoder());

        LOG_INF("Polarizer wheel homed");
        ORB_STATE_SET_CURRENT(RET_SUCCESS, "homed");
        g_polarizer_wheel_instance.homing.success = true;
    } else {
        // ❌ failure homing
        // encoder bumps not detected at expected positions
        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED,
                              "bumps not correctly detected");
        homing_failed();
    }

    // reset step counter
    atomic_clear(&g_polarizer_wheel_instance.step_count.current);
}

ret_code_t
polarizer_wheel_set_angle(uint32_t frequency, uint32_t angle_decidegrees)
{
    ret_code_t ret_val = RET_SUCCESS;

    if (angle_decidegrees > 3600 ||
        frequency > POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_1SEC_PER_TURN) {
        return RET_ERROR_INVALID_PARAM;
    }

    atomic_val_t target_step = ((int)angle_decidegrees *
                                POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 3600) %
                               POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
    atomic_val_t current =
        atomic_get(&g_polarizer_wheel_instance.step_count.current);

    if (target_step == current) {
        return RET_SUCCESS;
    }

    if (target_step >= current) {
        if ((target_step - current) <
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 2) {
            set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);
        } else {
            set_direction(POLARIZER_WHEEL_DIRECTION_BACKWARD);
        }
    } else {
        if (current - target_step >
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 2) {
            set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);
        } else {
            set_direction(POLARIZER_WHEEL_DIRECTION_BACKWARD);
        }
    }

    LOG_INF("angle(deci): %u, target_step: %ld, current: %ld, dir: %d",
            angle_decidegrees, target_step, current,
            g_polarizer_wheel_instance.step_count.direction);
    atomic_set(&g_polarizer_wheel_instance.step_count.target, target_step);

    ret_val = polarizer_move(frequency);

    return ret_val;
}

ret_code_t
polarizer_wheel_home_async(void)
{
    static bool started_once = false;

    if (started_once == false ||
        k_thread_join(&thread_data_polarizer_wheel_home, K_NO_WAIT) == 0) {
        // homing not in progress, status must be successful, otherwise
        // it tells us that thread isn't joined
        k_thread_create(
            &thread_data_polarizer_wheel_home, stack_area_polarizer_wheel_home,
            K_THREAD_STACK_SIZEOF(stack_area_polarizer_wheel_home),
            (k_thread_entry_t)polarizer_wheel_auto_homing_thread, NULL, NULL,
            NULL, THREAD_PRIORITY_POLARIZER_WHEEL_HOME, 0, K_NO_WAIT);
        k_thread_name_set(&thread_data_polarizer_wheel_home,
                          "polarizer_homing");
        started_once = true;
    } else {
        return RET_ERROR_BUSY;
    }

    return RET_SUCCESS;
}

static bool
devices_ready(void)
{
    return device_is_ready(polarizer_spi_bus_controller) &&
           device_is_ready(polarizer_spi_cs_gpio.port) &&
           device_is_ready(polarizer_step_pwm_spec->dev) &&
           device_is_ready(polarizer_enable_spec.port) &&
           device_is_ready(polarizer_step_dir_spec.port) &&
           device_is_ready(polarizer_encoder_enable_spec.port) &&
           device_is_ready(polarizer_encoder_spec.port);
}

ret_code_t
polarizer_wheel_init(const orb_mcu_Hardware *hw_version)
{
    int ret_val = RET_SUCCESS;
    if (hw_version == NULL) {
        ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_PARAM,
                              "invalid/NULL hw_version");
        return RET_ERROR_INVALID_PARAM;
    }

    if (hw_version->version <=
        orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_4) {
        polarizer_step_pwm_spec = &polarizer_step_pwm_spec_evt;
        polarizer_step_timer = (TIM_TypeDef *)DT_REG_ADDR(
            DT_PARENT(DT_NODELABEL(polarizer_step_pwm_evt)));
        pwm_timer_irq_n = COND_CODE_1(
            DT_IRQ_HAS_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm_evt)),
                            cc),
            (DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm_evt)), cc,
                            irq)),
            (DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm_evt)),
                            global, irq)));
    }

    if (!device_is_ready(polarizer_step_pwm_spec->dev)) {
        const int ret = device_init(polarizer_step_pwm_spec->dev);
        ASSERT_SOFT(ret);
    }

    if (!devices_ready()) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        ret_val = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    // clear the polarizer wheel runtime context
    memset(&g_polarizer_wheel_instance, 0, sizeof(g_polarizer_wheel_instance));

    // Polarizer spi chip select is controlled manually, configure inactive
    ret_val =
        gpio_pin_configure_dt(&polarizer_spi_cs_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    // Enable the DRV8434 motor driver
    ret_val = gpio_pin_configure_dt(&polarizer_enable_spec, GPIO_OUTPUT_ACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    // Enable the polarizer motor encoder
    ret_val = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    // Configure the polarizer motor direction pin
    ret_val =
        gpio_pin_configure_dt(&polarizer_step_dir_spec, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    // Configure the encoder pin as an input and set up callback, keep
    // interrupt disabled
    ret_val = gpio_pin_configure_dt(&polarizer_encoder_spec, GPIO_INPUT);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    gpio_init_callback(&polarizer_encoder_cb_data, encoder_callback,
                       BIT(polarizer_encoder_spec.pin));
    ret_val = gpio_add_callback(polarizer_encoder_spec.port,
                                &polarizer_encoder_cb_data);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    ret_val = disable_encoder();
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    // Initialize the DRV8434s driver
    ret_val = drv8434s_init(&drv8434_cfg);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    // Set up the DRV8434s device configuration
    const DRV8434S_DeviceCfg_t drv8434s_cfg = {
        .ctrl2.EN_OUT = DRV8434S_REG_CTRL2_VAL_ENOUT_DISABLE,
        .ctrl2.TOFF = DRV8434S_REG_CTRL2_VAL_TOFF_16US,
        .ctrl2.DECAY = DRV8434S_REG_CTRL2_VAL_DECAY_SMARTRIPPLE,
        .ctrl3.SPI_DIR = DRV8434S_REG_CTRL3_VAL_SPIDIR_PIN,
        .ctrl3.SPI_STEP = DRV8434S_REG_CTRL3_VAL_SPISTEP_PIN,
        .ctrl3.MICROSTEP_MODE = DRV8434S_REG_CTRL3_VAL_MICROSTEP_MODE_1_128,
        // .ctrl4.CLR_FLT = true,
        .ctrl4.LOCK = DRV8434S_REG_CTRL4_VAL_UNLOCK,
        .ctrl7.RC_RIPPLE = DRV8434S_REG_CTRL7_VAL_RCRIPPLE_1PERCENT,
        .ctrl7.EN_SSC = DRV8434S_REG_CTRL7_VAL_ENSSC_ENABLE,
        .ctrl7.TRQ_SCALE = DRV8434S_REG_CTRL7_VAL_TRQSCALE_NOSCALE};

    size_t timeout = 3;
    do {
        ret_val = drv8434s_clear_fault();
        if (ret_val) {
            if (timeout == 0) {
                ASSERT_SOFT(ret_val);
            }
            continue;
        }

        ret_val = drv8434s_write_config(&drv8434s_cfg);
        if (ret_val) {
            if (timeout == 0) {
                ASSERT_SOFT(ret_val);
            }
            continue;
        }

        ret_val = drv8434s_read_config();
        if (ret_val) {
            if (timeout == 0) {
                ASSERT_SOFT(ret_val);
            }
            continue;
        }

        ret_val = drv8434s_verify_config();
        if (ret_val) {
            if (timeout == 0) {
                ASSERT_SOFT(ret_val);
            }
        }
    } while (timeout-- && ret_val != RET_SUCCESS);

    // Enable the DRV8434s motor driver if configuration is successful
    // Scale the current to 75% of the maximum current
    if (ret_val == RET_SUCCESS) {
        drv8434s_scale_current(DRV8434S_TRQ_DAC_75);
        ret_val = drv8434s_enable();
        ASSERT_SOFT(ret_val);
        if (ret_val != RET_SUCCESS) {
            goto exit;
        }
    } else {
        goto exit;
    }

    // enable pwm interrupt
    irq_connect_dynamic(pwm_timer_irq_n, 0, polarizer_wheel_step_isr, NULL, 0);
    irq_enable(pwm_timer_irq_n);

    // home polarizer wheel
    ret_val = polarizer_wheel_home_async();

exit:
    if (ret_val != RET_SUCCESS) {
        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED, "init failed");
    } else {
        ORB_STATE_SET_CURRENT(RET_SUCCESS, "init success");
    }
    return ret_val;
}

bool
polarizer_wheel_homed(void)
{
    return g_polarizer_wheel_instance.homing.success;
}
