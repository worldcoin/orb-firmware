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
#include "system/timer2_irq/timer2_irq.h"
#include <app_config.h>
#include <math.h>
#include <stdlib.h>
#include <stm32g474xx.h>
#include <stm32g4xx_ll_tim.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

// Maximum number of notches needed to be encountered to finish homing
#define POLARIZER_WHEEL_ENCODER_MAX_HOMING_NOTCHES 5
// Minimum number of notches needed to be encountered to start homing
#define POLARIZER_WHEEL_ENCODER_HOMING_NOTCHES_COMPARE_MIN 2

// TODO get this correctly from the device tree
#define POLARIZER_STEP_CHANNEL 2

LOG_MODULE_REGISTER(polarizer, CONFIG_POLARIZER_LOG_LEVEL);

K_THREAD_STACK_DEFINE(stack_area_polarizer_wheel_home,
                      THREAD_STACK_SIZE_POLARIZER_WHEEL_HOME);

static Polarizer_Wheel_Instance_t g_polarizer_wheel_instance;

#define SPI_DEVICE_POLARIZER DT_NODELABEL(polarizer_controller)

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

// TODO get this correctly from the device tree
static TIM_TypeDef *polarizer_step_timer = TIM2;

static struct k_thread thread_data_polarizer_wheel_home;

static struct gpio_callback polarizer_encoder_cb_data;

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

// Enable polarizer wheel step interrupt
static ret_code_t
enable_step_interrupt(void)
{
    polarizer_step_timer->DIER |= TIM_DIER_CC2IE;

    return RET_SUCCESS;
}

// Disable the polarizer wheel step interrupt
static ret_code_t
disable_step_interrupt(void)
{
    polarizer_step_timer->DIER &= ~TIM_DIER_CC2IE;

    return RET_SUCCESS;
}

// clear the polarizer wheel step interrupt flag
static ret_code_t
clear_step_interrupt(void)
{
    polarizer_step_timer->SR &= ~TIM_SR_CC2IF;

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
        if (gpio_pin_get_dt(&polarizer_encoder_spec) == 1) {
            LOG_DBG("encoder detected: %u",
                    g_polarizer_wheel_instance.auto_homing.notches_encountered);
            if (!g_polarizer_wheel_instance.auto_homing.start_notch_found) {
                g_polarizer_wheel_instance.auto_homing.start_notch_found = true;
            } else {
                g_polarizer_wheel_instance.auto_homing.notches_encountered++;
                g_polarizer_wheel_instance.auto_homing.steps_to_last_notch =
                    atomic_get(&g_polarizer_wheel_instance.step_count
                                    .step_count_current);
            }
            // Stop the PWM
            pwm_set_dt(&polarizer_step_pwm_spec, 0, 0);
            g_polarizer_wheel_instance.movement.wheel_moving = false;
            // Disable the step interrupt
            disable_step_interrupt();
            clear_step_interrupt();
        }
    }
}

static void
polarizer_wheel_step_isr(void *arg)
{
    ARG_UNUSED(arg);

    atomic_inc(&g_polarizer_wheel_instance.step_count.step_count_current);
    atomic_t current =
        atomic_get(&g_polarizer_wheel_instance.step_count.step_count_current);
    if (current >= g_polarizer_wheel_instance.step_count.step_count_target) {
        pwm_set_dt(&polarizer_step_pwm_spec, 0, 0);
        g_polarizer_wheel_instance.movement.wheel_moving = false;
        atomic_set(&g_polarizer_wheel_instance.step_count.step_count_current,
                   0);
        g_polarizer_wheel_instance.step_count.step_count_target = 0;
        if (g_polarizer_wheel_instance.movement.target_position !=
            POLARIZER_WHEEL_POSITION_UNKNOWN) {
            g_polarizer_wheel_instance.movement.current_position =
                g_polarizer_wheel_instance.movement.target_position;
            g_polarizer_wheel_instance.movement.target_position =
                POLARIZER_WHEEL_POSITION_UNKNOWN;
        }
        disable_step_interrupt();
    }
    clear_step_interrupt();
}

// Check if all peripherals are ready for use
static bool
polarizer_wheel_peripherals_ready(void)
{
    if (!device_is_ready(polarizer_spi_bus_controller)) {
        return false;
    }
    if (!device_is_ready(polarizer_spi_cs_gpio.port)) {
        return false;
    }
    if (!device_is_ready(polarizer_step_pwm_spec.dev)) {
        return false;
    }
    if (!device_is_ready(polarizer_enable_spec.port)) {
        return false;
    }
    if (!device_is_ready(polarizer_step_dir_spec.port)) {
        return false;
    }
    if (!device_is_ready(polarizer_encoder_enable_spec.port)) {
        return false;
    }
    if (!device_is_ready(polarizer_encoder_spec.port)) {
        return false;
    }
    return true;
}

static ret_code_t
start_polarizer_wheel_step(uint32_t frequency, uint32_t step_count)
{
    int ret_val = RET_SUCCESS;

    if (frequency == 0 || step_count == 0) {
        return RET_ERROR_INVALID_PARAM;
    }
    // Calculate period and pulse width in nanoseconds
    uint32_t period_ns = (1000000000UL / frequency);
    uint32_t pulse_ns = period_ns * 0.5;

    // Reset pulse counter
    g_polarizer_wheel_instance.step_count.step_count_current = 0;
    g_polarizer_wheel_instance.step_count.step_count_target = step_count;

    g_polarizer_wheel_instance.movement.wheel_moving = true;

    // Start PWM
    ret_val = pwm_set_dt(&polarizer_step_pwm_spec, period_ns, pulse_ns);

    // Enable channel 2 compare interrupt
    enable_step_interrupt();

    return ret_val;
}

static void
polarizer_wheel_auto_homing_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    g_polarizer_wheel_instance.auto_homing.auto_homing_state =
        POLARIZER_WHEEL_AUTO_HOMING_STATE_IN_PROGRESS;
    g_polarizer_wheel_instance.auto_homing.notches_encountered = 0;
    g_polarizer_wheel_instance.auto_homing.start_notch_found = false;

    static uint32_t last_step_count = 0;
    // Enable the encoder interrupt to be able to detect notches
    enable_encoder_interrupt();

    while ((g_polarizer_wheel_instance.auto_homing.notches_encountered <=
            POLARIZER_WHEEL_ENCODER_MAX_HOMING_NOTCHES) &&
           (!g_polarizer_wheel_instance.movement.wheel_moving)) {

        if (g_polarizer_wheel_instance.auto_homing.start_notch_found == true) {
            switch (
                g_polarizer_wheel_instance.auto_homing.notches_encountered) {
            case 0:
                break;
            case 1:
                last_step_count =
                    g_polarizer_wheel_instance.auto_homing.steps_to_last_notch;
                break;
            default:
                if (abs((int32_t)(g_polarizer_wheel_instance.auto_homing
                                      .steps_to_last_notch) -
                        (int32_t)(last_step_count)) <= 100) {
                    g_polarizer_wheel_instance.auto_homing.auto_homing_state =
                        POLARIZER_WHEEL_AUTO_HOMING_STATE_SUCCESS;
                    g_polarizer_wheel_instance.movement.current_position =
                        POLARIZER_WHEEL_POSITION_PASS_THROUGH;
                    disable_encoder_interrupt();
                }
                break;
            }
        }
        if ((g_polarizer_wheel_instance.auto_homing.auto_homing_state !=
             POLARIZER_WHEEL_AUTO_HOMING_STATE_SUCCESS) &&
            (g_polarizer_wheel_instance.auto_homing.notches_encountered <
             POLARIZER_WHEEL_ENCODER_MAX_HOMING_NOTCHES)) {
            // 120 degrees in 3 seconds, move a total of 150 degrees
            // should hit encoder much sooner than 150 degrees
            start_polarizer_wheel_step(683, 2048);
            k_sleep(K_SECONDS(3));
        } else {
            if (g_polarizer_wheel_instance.auto_homing.auto_homing_state !=
                POLARIZER_WHEEL_AUTO_HOMING_STATE_SUCCESS) {
                g_polarizer_wheel_instance.auto_homing.auto_homing_state =
                    POLARIZER_WHEEL_AUTO_HOMING_STATE_FAILED;
            }
            break;
        }
    }
    return;
}

ret_code_t
polarizer_wheel_init(void)
{
    ret_code_t ret_val = RET_SUCCESS;

    if (!polarizer_wheel_peripherals_ready()) {
        return RET_ERROR_INVALID_STATE;
    }

    // Enable the polarizer SPI CS pin
    ret_val =
        gpio_pin_configure_dt(&polarizer_spi_cs_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    // Enable the DRV8434 motor driver
    ret_val =
        gpio_pin_configure_dt(&polarizer_enable_spec, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    gpio_pin_set_dt(&polarizer_enable_spec, 1);

    // Enable the polarizer motor encoder
    ret_val = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    gpio_pin_set_dt(&polarizer_encoder_enable_spec, 1);

    // Configure and set the polarizer motor direction pin
    ret_val =
        gpio_pin_configure_dt(&polarizer_step_dir_spec, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    gpio_pin_set_dt(&polarizer_step_dir_spec, 1);

    // Configure the encoder input pin as an input and set up callback, keep
    // interrupt disabled
    ret_val = gpio_pin_configure_dt(&polarizer_encoder_spec, GPIO_INPUT);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    gpio_init_callback(&polarizer_encoder_cb_data, encoder_callback,
                       BIT(polarizer_encoder_spec.pin));
    ret_val = gpio_add_callback(polarizer_encoder_spec.port,
                                &polarizer_encoder_cb_data);
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    ret_val = disable_encoder_interrupt();
    if (ret_val != 0) {
        return RET_ERROR_INTERNAL;
    }

    // Clear the Polarizer Wheel runtime context
    memset(&g_polarizer_wheel_instance, 0, sizeof(Polarizer_Wheel_Instance_t));

    // Set up the DRV8434 driver configuration
    DRV8434_DriverCfg_t drv8434_cfg = {
        .spi_cfg = {.frequency = 1000000,
                    .operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER |
                                 SPI_MODE_CPHA | SPI_TRANSFER_MSB,
                    .cs = {0}},
        .spi_bus_controller = polarizer_spi_bus_controller,
        .spi_cs_gpio = &polarizer_spi_cs_gpio};
    // Initialize the DRV8434 driver
    ret_val = drv8434_init(&drv8434_cfg);

    return ret_val;
}

ret_code_t
polarizer_wheel_configure(void)
{
    // Configure
    ret_code_t ret_val = RET_SUCCESS;
    // Set up the DRV8434 device configuration
    DRV8434_DeviceCfg_t drv8434_cfg = {
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

    drv8434_clear_fault();
    // Configure the DRV8434 device
    drv8434_write_config(&drv8434_cfg);

    // Read back the DRV8434 device configuration
    drv8434_read_config();

    // Verify the DRV8434 device configuration
    ret_val = drv8434_verify_config();

    // Enable the DRV8434 motor driver if configuration is successful
    // Scale the current to 75% of the maximum current
    if (ret_val == RET_SUCCESS) {
        drv8434_scale_current(DRV8434_TRQ_DAC_75);
        ret_val = drv8434_enable();
    } else {
        return ret_val;
    }

    if (ret_val != RET_SUCCESS) {
        return ret_val;
    }

    // Configure the timer interrupt to be able to count stepss
    timer2_register_callback(POLARIZER_STEP_CHANNEL, polarizer_wheel_step_isr,
                             NULL);

    k_tid_t tid = k_thread_create(
        &thread_data_polarizer_wheel_home, stack_area_polarizer_wheel_home,
        K_THREAD_STACK_SIZEOF(stack_area_polarizer_wheel_home),
        (k_thread_entry_t)polarizer_wheel_auto_homing_thread, NULL, NULL, NULL,
        THREAD_PRIORITY_POLARIZER_WHEEL_HOME, 0, K_NO_WAIT);
    k_thread_name_set(tid, "polarizer_wheel_auto_homing");

    return ret_val;
}

Polarizer_Wheel_Position_t
polarizer_wheel_get_position(void)
{
    return g_polarizer_wheel_instance.movement.current_position;
}

ret_code_t
polarizer_wheel_set_position(Polarizer_Wheel_Position_t position)
{
    ret_code_t ret_val = RET_SUCCESS;

    if (position == POLARIZER_WHEEL_POSITION_UNKNOWN) {
        return RET_ERROR_INVALID_PARAM;
    }
    g_polarizer_wheel_instance.movement.target_position = position;

    switch (g_polarizer_wheel_instance.movement.current_position) {
    case POLARIZER_WHEEL_POSITION_PASS_THROUGH:
        if (position == POLARIZER_WHEEL_POSITION_45_DEGREE) {
            ret_val = start_polarizer_wheel_step(683, 2048);
        } else if (position == POLARIZER_WHEEL_POSITION_90_DEGREE) {
            ret_val = start_polarizer_wheel_step(683, 4096);
        }
        break;
    case POLARIZER_WHEEL_POSITION_45_DEGREE:
        if (position == POLARIZER_WHEEL_POSITION_PASS_THROUGH) {
            ret_val = start_polarizer_wheel_step(683, 4096);
        } else if (position == POLARIZER_WHEEL_POSITION_90_DEGREE) {
            ret_val = start_polarizer_wheel_step(683, 2048);
        }
        break;
    case POLARIZER_WHEEL_POSITION_90_DEGREE:
        if (position == POLARIZER_WHEEL_POSITION_PASS_THROUGH) {
            ret_val = start_polarizer_wheel_step(683, 2048);
        } else if (position == POLARIZER_WHEEL_POSITION_45_DEGREE) {
            ret_val = start_polarizer_wheel_step(683, 4096);
        }
        break;
    default:
        break;
    }
    if (ret_val != RET_SUCCESS) {
        return ret_val;
    } else {
        g_polarizer_wheel_instance.movement.current_position =
            POLARIZER_WHEEL_POSITION_UNKNOWN;
    }
    return ret_val;
}
