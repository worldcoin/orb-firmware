/******************************************************************************
 * @file polarizer_wheel.c
 * @brief Source file for Polarizer Wheel functions
 *
 * This file defines the application level interface functions for the Polarizer
 * Wheel such as initialize, configure, and control
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/
#include "polarizer_wheel.h"
#include "polarizer_wheel_defines.h"
#include <app_assert.h>
#include <app_config.h>
#include <math.h>
#include <stdlib.h>
#include <stm32g474xx.h>
#include <stm32g4xx_ll_tim.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(polarizer, CONFIG_POLARIZER_LOG_LEVEL);

K_THREAD_STACK_DEFINE(stack_area_polarizer_wheel_home,
                      THREAD_STACK_SIZE_POLARIZER_WHEEL_HOME);
static struct k_thread thread_data_polarizer_wheel_home;

static polarizer_wheel_instance_t g_polarizer_wheel_instance = {0};

#define TIMER2_IRQn DT_IRQN(DT_NODELABEL(timers2))

// Peripherals (Motor Driver SPI, motor driver enable, encoder enable, encoder
// feedback, step PWM)
static const struct device *polarizer_spi_bus_controller =
    DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(polarizer_controller)));
static const struct gpio_dt_spec polarizer_spi_cs_gpio =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_spi_cs_gpios);
static const struct pwm_dt_spec polarizer_step_pwm_spec =
    PWM_DT_SPEC_GET(DT_PATH(polarizer_step));
static const struct gpio_dt_spec polarizer_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_enable_gpios);
static const struct gpio_dt_spec polarizer_step_dir_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_direction_gpios);
static const struct gpio_dt_spec polarizer_encoder_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user),
                     polarizer_stepper_encoder_enable_gpios);
static const struct gpio_dt_spec polarizer_encoder_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), polarizer_stepper_encoder_gpios);

static TIM_TypeDef *polarizer_step_timer =
    (TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)));
static struct gpio_callback polarizer_encoder_cb_data;

// Set up the DRV8434 driver configuration
static const DRV8434_DriverCfg_t drv8434_cfg = {
    .spi = (struct spi_dt_spec)SPI_DT_SPEC_GET(
        DT_NODELABEL(polarizer_controller),
        SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPHA | SPI_TRANSFER_MSB,
        0),
    .spi_cs_gpio = &polarizer_spi_cs_gpio};

static K_SEM_DEFINE(home_sem, 0, 1);

// Enable encoder interrupt
static ret_code_t
enable_encoder_interrupt(void)
{
    return gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                           GPIO_INT_EDGE_RISING);
}

// Disable the interrupt
static ret_code_t
disable_encoder_interrupt(void)
{
    return gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                           GPIO_INT_DISABLE);
}

// clear the polarizer wheel step interrupt flag
static ret_code_t
clear_step_interrupt(void)
{
    LL_TIM_ClearFlag_CC2(polarizer_step_timer);

    return RET_SUCCESS;
}

// Enable polarizer wheel step interrupt
static ret_code_t
enable_step_interrupt(void)
{
    clear_step_interrupt();
    LL_TIM_EnableIT_CC2(polarizer_step_timer);

    return RET_SUCCESS;
}

// Disable the polarizer wheel step interrupt
static ret_code_t
disable_step_interrupt(void)
{
    LL_TIM_DisableIT_CC2(polarizer_step_timer);

    return RET_SUCCESS;
}

// Define callback function
static void
encoder_callback(const struct device *dev, struct gpio_callback *cb,
                 uint32_t pins)
{
    // Handle the interrupt here
    // This function will be called on rising edge
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(polarizer_encoder_spec.pin)) {
        if (g_polarizer_wheel_instance.homing.state ==
            POLARIZER_WHEEL_AUTO_HOMING_STATE_IN_PROGRESS) {
            if (gpio_pin_get_dt(&polarizer_encoder_spec) == 1) {
                LOG_DBG("notches detected: %u",
                        g_polarizer_wheel_instance.homing.notch_count);
                k_sem_give(&home_sem);
            }
        }
    }
}

static void
polarizer_wheel_step_isr(const void *arg)
{
    ARG_UNUSED(arg);

    if (LL_TIM_IsActiveFlag_CC2(polarizer_step_timer)) {
        atomic_inc(&g_polarizer_wheel_instance.step_count.current);
        if (atomic_get(&g_polarizer_wheel_instance.step_count.current) >
            POLARIZER_WHEEL_STEPS_PER_1_FILTER_WHEEL_POSITION * 3) {
            atomic_clear(&g_polarizer_wheel_instance.step_count.current);
        }

        if (g_polarizer_wheel_instance.step_count.target ==
            g_polarizer_wheel_instance.step_count.current) {
            LOG_DBG("Reached target (%lu), stopping motor",
                    g_polarizer_wheel_instance.step_count.target);
            pwm_set_dt(&polarizer_step_pwm_spec, 0, 0);
        }
        clear_step_interrupt();
    }
}

static ret_code_t
start_polarizer_wheel_step(uint32_t frequency, uint32_t step_count)
{
    int ret_val = RET_SUCCESS;

    if (frequency == 0 || step_count == 0) {
        return RET_ERROR_INVALID_PARAM;
    }

    // Calculate period and pulse width in nanoseconds
    const uint32_t period_ns = NSEC_PER_SEC / frequency;
    const uint32_t pulse_ns = period_ns / 2;

    // Start PWM
    ret_val = pwm_set_dt(&polarizer_step_pwm_spec, period_ns, pulse_ns);
    ASSERT_SOFT(ret_val);

    if (ret_val == 0) {
        // Reset pulse counter
        atomic_clear(&g_polarizer_wheel_instance.step_count.current);
        atomic_set(&g_polarizer_wheel_instance.step_count.target, step_count);
        // Enable channel 2 compare interrupt
        enable_step_interrupt();
    }

    return ret_val;
}

static void
polarizer_wheel_auto_homing_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    // enable pwm interrupt
    irq_connect_dynamic(TIMER2_IRQn, 0, polarizer_wheel_step_isr, NULL, 0);
    clear_step_interrupt();
    irq_enable(TIMER2_IRQn);

    // enable encoder interrupt to detect notches
    enable_encoder_interrupt();

    bool homed = false;
    g_polarizer_wheel_instance.homing.notch_count = 0;
    start_polarizer_wheel_step(
        POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
        POLARIZER_WHEEL_STEPS_PER_1_FILTER_WHEEL_POSITION * 4);

    while (!homed) {
        // wait for notch detection
        k_sem_take(&home_sem, K_FOREVER);

        LOG_INF("homing: steps: %ld, notch count: %d",
                atomic_get(&g_polarizer_wheel_instance.step_count.current),
                g_polarizer_wheel_instance.homing.notch_count);

        // notch #0 can be anywhere, so continue spinning to make sure the step
        // counter resets and actually start counting steps afterwards
        if (g_polarizer_wheel_instance.homing.notch_count != 0 &&
            g_polarizer_wheel_instance.step_count.current < 1000) {
            pwm_set_dt(&polarizer_step_pwm_spec, 0, 0);
            homed = true;
        }

        g_polarizer_wheel_instance.homing.notch_count++;
        atomic_clear(&g_polarizer_wheel_instance.step_count.current);
    }

    start_polarizer_wheel_step(POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
                               POLARIZER_WHEEL_STEPS_CORRECTION);

    // wait for completion and disconnect interrupt
    k_sleep(K_SECONDS(1));
    disable_step_interrupt();
    irq_disable(TIMER2_IRQn);
    irq_disconnect_dynamic(TIMER2_IRQn, 0, polarizer_wheel_step_isr, NULL, 0);

    LOG_INF("Polarizer wheel homed");
    drv8434_scale_current(DRV8434_TRQ_DAC_25);

    // reset step counter
    atomic_clear(&g_polarizer_wheel_instance.step_count.current);
}

polarizer_wheel_position_t
polarizer_wheel_get_position(void)
{
    return g_polarizer_wheel_instance.step_count.current;
}

// TODO: This function will move the wheel from notch to notch but slowly
//  Might need to implement a trapezoidal velocity profile to move the wheel
//  faster without step loss Evaluate before DVT and signup integration
ret_code_t
polarizer_wheel_set_position(polarizer_wheel_position_t position)
{
    ret_code_t ret_val = RET_SUCCESS;

    if (position == POLARIZER_WHEEL_POSITION_UNKNOWN) {
        return RET_ERROR_INVALID_PARAM;
    }
    g_polarizer_wheel_instance.step_count.target = position;

    switch (g_polarizer_wheel_instance.step_count.current) {
    case POLARIZER_WHEEL_POSITION_PASS_THROUGH:
        if (position == POLARIZER_WHEEL_POSITION_0_DEGREE) {
            ret_val = start_polarizer_wheel_step(
                POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
                POLARIZER_WHEEL_STEPS_PER_1_FILTER_WHEEL_POSITION);
        } else if (position == POLARIZER_WHEEL_POSITION_90_DEGREE) {
            ret_val = start_polarizer_wheel_step(
                683, POLARIZER_WHEEL_STEPS_PER_2_FILTER_WHEEL_POSITION);
        }
        break;
    case POLARIZER_WHEEL_POSITION_0_DEGREE:
        if (position == POLARIZER_WHEEL_POSITION_PASS_THROUGH) {
            ret_val = start_polarizer_wheel_step(
                POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
                POLARIZER_WHEEL_STEPS_PER_2_FILTER_WHEEL_POSITION);
        } else if (position == POLARIZER_WHEEL_POSITION_90_DEGREE) {
            ret_val = start_polarizer_wheel_step(
                POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
                POLARIZER_WHEEL_STEPS_PER_1_FILTER_WHEEL_POSITION);
        }
        break;
    case POLARIZER_WHEEL_POSITION_90_DEGREE:
        if (position == POLARIZER_WHEEL_POSITION_PASS_THROUGH) {
            ret_val = start_polarizer_wheel_step(
                POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
                POLARIZER_WHEEL_STEPS_PER_1_FILTER_WHEEL_POSITION);
        } else if (position == POLARIZER_WHEEL_POSITION_0_DEGREE) {
            ret_val = start_polarizer_wheel_step(
                POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING,
                POLARIZER_WHEEL_STEPS_PER_2_FILTER_WHEEL_POSITION);
        }
        break;
    default:
        break;
    }

    if (ret_val != RET_SUCCESS) {
        return ret_val;
    }

    return ret_val;
}

ret_code_t
polarizer_wheel_home_async(void)
{
    static bool started_once = false;

    if (started_once == false ||
        k_thread_join(&thread_data_polarizer_wheel_home, K_NO_WAIT) == 0) {
        k_thread_create(
            &thread_data_polarizer_wheel_home, stack_area_polarizer_wheel_home,
            K_THREAD_STACK_SIZEOF(stack_area_polarizer_wheel_home),
            (k_thread_entry_t)polarizer_wheel_auto_homing_thread, NULL, NULL,
            NULL, THREAD_PRIORITY_POLARIZER_WHEEL_HOME, 0, K_NO_WAIT);
        k_thread_name_set(&thread_data_polarizer_wheel_home,
                          "polarizer_homing");
        started_once = true;
    } else {
        return RET_ERROR_INVALID_STATE;
    }

    return RET_SUCCESS;
}

static bool
devices_ready(void)
{
    return device_is_ready(polarizer_spi_bus_controller) &&
           device_is_ready(polarizer_spi_cs_gpio.port) &&
           device_is_ready(polarizer_step_pwm_spec.dev) &&
           device_is_ready(polarizer_enable_spec.port) &&
           device_is_ready(polarizer_step_dir_spec.port) &&
           device_is_ready(polarizer_encoder_enable_spec.port) &&
           device_is_ready(polarizer_encoder_spec.port);
}

ret_code_t
polarizer_wheel_init(void)
{
    ret_code_t ret_val = RET_SUCCESS;

    if (!devices_ready()) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INVALID_STATE;
    }

    // Enable the polarizer SPI CS pin
    ret_val =
        gpio_pin_configure_dt(&polarizer_spi_cs_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    // Enable the DRV8434 motor driver
    ret_val = gpio_pin_configure_dt(&polarizer_enable_spec, GPIO_OUTPUT_ACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    // Enable the polarizer motor encoder
    ret_val = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_ACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    // Configure and set the polarizer motor direction pin
    ret_val =
        gpio_pin_configure_dt(&polarizer_step_dir_spec, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    // Configure the encoder input pin as an input and set up callback, keep
    // interrupt disabled
    ret_val = gpio_pin_configure_dt(&polarizer_encoder_spec, GPIO_INPUT);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    gpio_init_callback(&polarizer_encoder_cb_data, encoder_callback,
                       BIT(polarizer_encoder_spec.pin));
    ret_val = gpio_add_callback(polarizer_encoder_spec.port,
                                &polarizer_encoder_cb_data);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    ret_val = disable_encoder_interrupt();
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    // Clear the Polarizer Wheel runtime context
    memset(&g_polarizer_wheel_instance, 0, sizeof(g_polarizer_wheel_instance));

    // Initialize the DRV8434 driver
    ret_val = drv8434_init(&drv8434_cfg);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        return RET_ERROR_INTERNAL;
    }

    // Set up the DRV8434 device configuration
    const DRV8434_DeviceCfg_t drv8434_cfg = {
        .ctrl2.EN_OUT = DRV8434_REG_CTRL2_VAL_ENOUT_DISABLE,
        .ctrl2.TOFF = DRV8434_REG_CTRL2_VAL_TOFF_16US,
        .ctrl2.DECAY = DRV8434_REG_CTRL2_VAL_DECAY_SMARTRIPPLE,
        .ctrl3.SPI_DIR = DRV8434_REG_CTRL3_VAL_SPIDIR_PIN,
        .ctrl3.SPI_STEP = DRV8434_REG_CTRL3_VAL_SPISTEP_PIN,
        .ctrl3.MICROSTEP_MODE = DRV8434_REG_CTRL3_VAL_MICROSTEP_MODE_1_128,
        // .ctrl4.CLR_FLT = true,
        .ctrl4.LOCK = DRV8434_REG_CTRL4_VAL_UNLOCK,
        .ctrl7.RC_RIPPLE = DRV8434_REG_CTRL7_VAL_RCRIPPLE_1PERCENT,
        .ctrl7.EN_SSC = DRV8434_REG_CTRL7_VAL_ENSSC_ENABLE,
        .ctrl7.TRQ_SCALE = DRV8434_REG_CTRL7_VAL_TRQSCALE_NOSCALE};

    ret_val = drv8434_clear_fault();
    ASSERT_SOFT(ret_val);

    ret_val = drv8434_write_config(&drv8434_cfg);
    ASSERT_SOFT(ret_val);

    ret_val = drv8434_read_config();
    ASSERT_SOFT(ret_val);

    ret_val = drv8434_verify_config();
    ASSERT_SOFT(ret_val);

    // Enable the DRV8434 motor driver if configuration is successful
    // Scale the current to 75% of the maximum current
    if (ret_val == RET_SUCCESS) {
        drv8434_scale_current(DRV8434_TRQ_DAC_75);
        ret_val = drv8434_enable();
        ASSERT_SOFT(ret_val);
        if (ret_val != RET_SUCCESS) {
            return ret_val;
        }
    } else {
        return ret_val;
    }

    ret_val = polarizer_wheel_home_async();
    return ret_val;
}
