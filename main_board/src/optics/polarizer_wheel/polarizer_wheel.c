/******************************************************************************
 * @file polarizer_wheel.c
 * @brief Source file for Polarizer Wheel functions
 *
 * This file defines the application level interface functions for the Polarizer
 * Wheel such as initialize, configure, and control
 *
 * Architecture:
 * - A persistent thread handles all logic (state machine, acceleration, etc.)
 * - ISRs are minimal: update counters and signal semaphores
 * - Communication from ISR to thread via semaphores
 * - Commands from API are queued and processed by thread
 *
 ******************************************************************************/
#include "polarizer_wheel.h"
#include "drv8434s/drv8434s.h"
#include "orb_state.h"
#include <app_assert.h>
#include <app_config.h>
#include <common.pb.h>
#include <pubsub/pubsub.h>
#include <stdlib.h>
#include <stm32g474xx.h>
#include <stm32g4xx_ll_tim.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>

#include "orb_logs.h"

LOG_MODULE_REGISTER(polarizer);
ORB_STATE_REGISTER(polarizer);

/* Thread stack and data for the main polarizer thread */
K_THREAD_STACK_DEFINE(stack_area_polarizer_wheel,
                      THREAD_STACK_SIZE_POLARIZER_WHEEL_HOME);
static struct k_thread thread_data_polarizer_wheel;

/* Direction enumeration */
enum polarizer_wheel_direction_e {
    POLARIZER_WHEEL_DIRECTION_BACKWARD = -1,
    POLARIZER_WHEEL_DIRECTION_FORWARD = 1,
};

/* State machine states */
enum polarizer_wheel_state_e {
    STATE_UNINITIALIZED = 0,
    STATE_IDLE,
    STATE_HOMING,
    STATE_CALIBRATING,
    // encoder is used to verify position and make adjustments
    STATE_POSITIONING_WITH_ENCODER,
    // no encoder feedback, open-loop positioning
    STATE_POSITIONING,
};

/* Commands that can be sent to the thread */
enum polarizer_wheel_cmd_e {
    CMD_NONE = 0,
    CMD_HOME,
    CMD_SET_ANGLE,
    CMD_CALIBRATE,
};

/* Acceleration state */
enum acceleration_state_e {
    ACCELERATION_IDLE = 0, /* no acceleration: fixed speed */
    ACCELERATION_ACTIVE,   /* distance-based accel/decel in progress */
};

/* Command structure for set_angle */
typedef struct {
    uint32_t frequency;
    uint32_t angle_decidegrees;
    bool shortest_path;
} set_angle_cmd_t;

typedef struct {
    /* Current state machine state */
    enum polarizer_wheel_state_e state;

    /* Timestamp (ms) when idle current should be scaled down, or 0 if not
     * pending */
    uint32_t idle_current_scale_down_time_ms;

    /* Homing state */
    struct {
        uint8_t notch_count;
        bool success;
    } homing;

    /* Bump calibration state - measures bump widths in dedicated calibration
     * routine */
    struct {
        /* Bump widths in microsteps for each position */
        uint32_t bump_width_pass_through;
        uint32_t bump_width_vertical;
        uint32_t bump_width_horizontal;
        /* Tracking during calibration */
        uint8_t bump_index; /* 0=vertical, 1=extra(skip), 2=horizontal,
                               3=pass_through */
        uint32_t bump_entry_position; /* step position when entering bump */
        bool inside_bump; /* true when on bump (between rising and falling edge)
                           */
        bool calibration_complete;
        bool
            needs_calibration; /* true if calibration should run after homing */
    } calibration;

    /* Step counting */
    struct {
        /* microsteps into range: [0; POLARIZER_WHEEL_MICROSTEPS_360_DEGREES] */
        atomic_t current;
        atomic_t target;
        enum polarizer_wheel_direction_e direction;
    } step_count;

    /* Positioning state */
    struct {
        /* target notch edge position in microsteps */
        int32_t target_notch_edge;
        bool encoder_enabled;
        uint32_t frequency;
        /* flag to track if encoder was triggered during positioning */
        atomic_t encoder_triggered;
        uint32_t start_time_ms;
        /* for state reporting */
        orb_mcu_main_PolarizerWheelState_Position previous_position;
        orb_mcu_main_PolarizerWheelState_Position target_position;
        uint32_t step_diff_microsteps;
    } positioning;

    /* Acceleration state - distance-based ramp control */
    struct {
        uint32_t current_frequency;
        uint32_t min_frequency; /* frequency at start/end of travel */
        enum acceleration_state_e state;
        int32_t start_position;
        uint32_t total_distance; /* total steps to travel */
    } acceleration;

    /* Pending command */
    struct {
        enum polarizer_wheel_cmd_e type;
        set_angle_cmd_t set_angle;
    } pending_cmd;

#if CONFIG_POLARIZER_LOG_LEVEL_DBG
    struct {
        uint32_t min_frequency;
        uint32_t max_frequency;
    } debug_stats;
#endif
} polarizer_wheel_instance_t;

static polarizer_wheel_instance_t g_polarizer_wheel_instance = {0};

/* Semaphores for ISR to thread communication */
static K_SEM_DEFINE(step_sem, 0, 1); /* a motor step occured */
static K_SEM_DEFINE(encoder_sem, 0,
                    1);             /* signaled on encoder notch detection */
static K_SEM_DEFINE(cmd_sem, 0, 1); /* signaled when command is pending */

/* Mutex to protect command queuing */
static K_MUTEX_DEFINE(cmd_mutex);

/* Delay before scaling down motor current after idle (ms) */
#define POLARIZER_IDLE_CURRENT_DELAY_MS 2000

/**
 * Calculate timeout for the next k_poll iteration.
 * Returns K_FOREVER if no timeout is pending, otherwise returns
 * the time until idle current scale-down should occur.
 */
static inline k_timeout_t
get_poll_timeout(void)
{
    uint32_t scale_down_time =
        g_polarizer_wheel_instance.idle_current_scale_down_time_ms;
    if (scale_down_time == 0) {
        return K_FOREVER;
    }

    int32_t remaining_ms = (int32_t)(scale_down_time - k_uptime_get_32());
    if (remaining_ms <= 0) {
        return K_NO_WAIT;
    }
    return K_MSEC(remaining_ms);
}

/* Peripherals */
static const struct device *polarizer_spi_bus_controller =
    DEVICE_DT_GET(DT_PARENT(DT_PARENT(DT_NODELABEL(polarizer_wheel))));
static const struct gpio_dt_spec polarizer_spi_cs_gpio =
    GPIO_DT_SPEC_GET(DT_PARENT(DT_NODELABEL(polarizer_wheel)), spi_cs_gpios);
static const struct pwm_dt_spec polarizer_step_pwm_spec_evt =
    PWM_DT_SPEC_GET(DT_PATH(polarizer_step_evt));
static const struct pwm_dt_spec polarizer_step_pwm_spec_dvt =
    PWM_DT_SPEC_GET(DT_PATH(polarizer_step));
static const struct pwm_dt_spec *polarizer_step_pwm_spec =
    &polarizer_step_pwm_spec_dvt;
static const struct gpio_dt_spec polarizer_enable_spec =
    GPIO_DT_SPEC_GET(DT_PARENT(DT_NODELABEL(polarizer_wheel)), en_gpios);
static const struct gpio_dt_spec polarizer_step_dir_spec =
    GPIO_DT_SPEC_GET(DT_PARENT(DT_NODELABEL(polarizer_wheel)), dir_gpios);
static const struct gpio_dt_spec polarizer_encoder_enable_spec =
    GPIO_DT_SPEC_GET(DT_NODELABEL(polarizer_wheel), encoder_enable_gpios);
static const struct gpio_dt_spec polarizer_encoder_spec =
    GPIO_DT_SPEC_GET(DT_NODELABEL(polarizer_wheel), encoder_gpios);

/* Timer handle and IRQ number */
static TIM_TypeDef *polarizer_step_timer =
    (TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)));

static uint32_t pwm_timer_irq_n = COND_CODE_1(
    DT_IRQ_HAS_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)), cc),
    (DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)), cc, irq)),
    (DT_IRQ_BY_NAME(DT_PARENT(DT_NODELABEL(polarizer_step_pwm)), global, irq)));

static struct gpio_callback polarizer_encoder_cb_data;

/* DRV8434 driver configuration */
static const DRV8434S_DriverCfg_t drv8434_cfg = {
    .spi = (struct spi_dt_spec)SPI_DT_SPEC_GET(
        DT_PARENT(DT_NODELABEL(polarizer_wheel)),
        SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPHA | SPI_TRANSFER_MSB,
        0),
    .spi_cs_gpio = &polarizer_spi_cs_gpio};

/* Homing parameters */
#define POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MAX                         \
    (4 * POLARIZER_WHEEL_MICROSTEPS_PER_STEP) /* 4*7.5 = 30 */
#define POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MIN                         \
    (2.5 * POLARIZER_WHEEL_MICROSTEPS_PER_STEP) /* 2.5*7.5 = 18.75 */
#define POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS  3
#define POLARIZER_WHEEL_NOTCH_DETECT_ATTEMPTS 9

BUILD_ASSERT(POLARIZER_WHEEL_NOTCH_DETECT_ATTEMPTS > 4);

/* Forward declarations */
static int
polarizer_halt(void);
static int
polarizer_set_frequency(uint32_t frequency);
static int
enable_step_interrupt(void);
static int
disable_step_interrupt(void);
static void
execute_calibration(void);

/*
 * Calculate the shortest signed distance between two positions on the circular
 * wheel.
 */
static int32_t
circular_signed_distance(int32_t from, int32_t to)
{
    int32_t diff = to - from;
    int32_t half_range = POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 2;

    if (diff > half_range) {
        diff -= POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
    } else if (diff < -half_range) {
        diff += POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
    }
    return diff;
}

/*
 * Linear acceleration/deceleration configuration and helpers
 */
/* Default linear acceleration (8000 steps/s²) */
#define LINEAR_ACCELERATION_DEFAULT_STEPS_PER_S2 8000

/* Default max speed (200ms per turn) */
#define MAX_SPEED_DEFAULT_MS_PER_TURN 200

/* Configurable linear acceleration in steps/s² (runtime modifiable) */
static uint32_t g_linear_acceleration_steps_per_s2 =
    LINEAR_ACCELERATION_DEFAULT_STEPS_PER_S2;

/* Configurable max speed in ms/turn (runtime modifiable) */
static uint32_t g_max_speed_ms_per_turn = MAX_SPEED_DEFAULT_MS_PER_TURN;

/* Get current acceleration in µsteps/s² */
static inline uint32_t
get_linear_acceleration_usteps_per_s2(void)
{
    return g_linear_acceleration_steps_per_s2 *
           POLARIZER_WHEEL_MICROSTEPS_PER_STEP;
}

/* Get current max frequency in µsteps/s */
static inline uint32_t
get_max_frequency(void)
{
    return POLARIZER_MICROSTEPS_PER_SECOND(g_max_speed_ms_per_turn);
}

void
polarizer_wheel_set_acceleration(uint32_t accel_steps_per_s2)
{
    if (accel_steps_per_s2 == 0) {
        g_linear_acceleration_steps_per_s2 =
            LINEAR_ACCELERATION_DEFAULT_STEPS_PER_S2;
    } else {
        g_linear_acceleration_steps_per_s2 = accel_steps_per_s2;
    }
}

uint32_t
polarizer_wheel_get_acceleration(void)
{
    return g_linear_acceleration_steps_per_s2;
}

void
polarizer_wheel_set_max_speed(uint32_t ms_per_turn)
{
    if (ms_per_turn == 0) {
        g_max_speed_ms_per_turn = MAX_SPEED_DEFAULT_MS_PER_TURN;
    } else {
        g_max_speed_ms_per_turn = ms_per_turn;
    }
}

uint32_t
polarizer_wheel_get_max_speed(void)
{
    return g_max_speed_ms_per_turn;
}

/**
 * Calculate frequency using linear acceleration/deceleration.
 * Uses kinematic equation: v² = v₀² + 2*a*s
 * where v is velocity (frequency), a is acceleration, s is displacement (steps)
 *
 * The ramp is symmetric: frequency increases from min_freq at start,
 * reaches peak at midpoint, then decreases back to min_freq at end.
 * The caller provides ramp_step which goes 0 -> midpoint -> 0.
 * The frequency is capped at max_freq to prevent overshooting.
 *
 * @param ramp_step Distance from nearest endpoint (0 at start/end, max at
 * midpoint)
 * @param min_freq Minimum frequency in Hz (µsteps/s) at start/end of travel
 * @return Calculated frequency for the current step
 */
static uint32_t
calculate_linear_ramp_frequency(const uint32_t ramp_step,
                                const uint32_t min_freq,
                                const uint32_t max_freq)
{
    /* v² = v₀² + 2*a*s - velocity based on distance from endpoint */
    const uint64_t min_freq_sq = (uint64_t)min_freq * min_freq;
    const uint64_t accel_term =
        2ULL * get_linear_acceleration_usteps_per_s2() * ramp_step;
    const uint64_t freq_sq = min_freq_sq + accel_term;

    /* Integer square root using Newton's method */
    uint64_t x = freq_sq;
    uint64_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + freq_sq / x) / 2;
    }

    /* Max speed plateau */
    x = MIN(x, max_freq);

    return (uint32_t)x;
}

/* Clear the polarizer wheel step interrupt flag */
static ret_code_t
clear_step_interrupt(void)
{
    static void __maybe_unused (*const clear_capture_interrupt[])(
        TIM_TypeDef *) = {LL_TIM_ClearFlag_CC1, LL_TIM_ClearFlag_CC2,
                          LL_TIM_ClearFlag_CC3, LL_TIM_ClearFlag_CC4};

    clear_capture_interrupt[polarizer_step_pwm_spec->channel - 1](
        polarizer_step_timer);

    return RET_SUCCESS;
}

static int
disable_step_interrupt(void)
{
    static void __maybe_unused (*const disable_capture_interrupt[])(
        TIM_TypeDef *) = {LL_TIM_DisableIT_CC1, LL_TIM_DisableIT_CC2,
                          LL_TIM_DisableIT_CC3, LL_TIM_DisableIT_CC4};

    clear_step_interrupt();
    disable_capture_interrupt[polarizer_step_pwm_spec->channel - 1](
        polarizer_step_timer);

    return RET_SUCCESS;
}

static int
enable_step_interrupt(void)
{
    static void __maybe_unused (*const enable_capture_interrupt[])(
        TIM_TypeDef *) = {LL_TIM_EnableIT_CC1, LL_TIM_EnableIT_CC2,
                          LL_TIM_EnableIT_CC3, LL_TIM_EnableIT_CC4};

    clear_step_interrupt();
    enable_capture_interrupt[polarizer_step_pwm_spec->channel - 1](
        polarizer_step_timer);

    return RET_SUCCESS;
}

static int
polarizer_set_frequency(const uint32_t frequency)
{
    if (frequency > get_max_frequency() ||
        frequency < POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_MINIMUM) {
        return RET_ERROR_INVALID_PARAM;
    }
    return pwm_set_dt(polarizer_step_pwm_spec, NSEC_PER_SEC / frequency,
                      (NSEC_PER_SEC / frequency) / 2);
}

static int
polarizer_halt(void)
{
    const int ret = pwm_set_dt(polarizer_step_pwm_spec, 0, 0);
    disable_step_interrupt();

    /* Reset acceleration state */
    g_polarizer_wheel_instance.acceleration.state = ACCELERATION_IDLE;
    g_polarizer_wheel_instance.acceleration.current_frequency = 0;
    g_polarizer_wheel_instance.acceleration.min_frequency = 0;

    return ret;
}

/* Enable encoder hardware and interrupt */
static int
enable_encoder(void)
{
    int ret = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_ACTIVE);
    if (ret) {
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                          GPIO_INT_EDGE_RISING);
    if (ret) {
        return ret;
    }

    g_polarizer_wheel_instance.positioning.encoder_enabled = true;
    return RET_SUCCESS;
}

/* Disable encoder hardware and interrupt */
static int
disable_encoder(void)
{
    int ret;
    ret = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret) {
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                          GPIO_INT_DISABLE);
    if (ret) {
        return ret;
    }

    g_polarizer_wheel_instance.positioning.encoder_enabled = false;
    return RET_SUCCESS;
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

static int
polarizer_rotate(const uint32_t frequency)
{
    /* Cancel any pending idle current scale-down */
    g_polarizer_wheel_instance.idle_current_scale_down_time_ms = 0;

    const int ret = drv8434s_scale_current(DRV8434S_TRQ_DAC_100);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    enable_step_interrupt();
    return polarizer_set_frequency(frequency);
}

static int
report_reached_state(void)
{
    orb_mcu_main_PolarizerWheelState state_report = {0};

    uint32_t elapsed_ms = k_uptime_get_32() -
                          g_polarizer_wheel_instance.positioning.start_time_ms;

    state_report.previous_position =
        g_polarizer_wheel_instance.positioning.previous_position;
    state_report.current_position =
        g_polarizer_wheel_instance.positioning.target_position;
    state_report.step_loss_microsteps =
        g_polarizer_wheel_instance.positioning.step_diff_microsteps;
    state_report.transition_time_ms = elapsed_ms;

    // Motion settings
    state_report.acceleration_steps_per_s2 = polarizer_wheel_get_acceleration();
    state_report.max_speed_ms_per_turn = polarizer_wheel_get_max_speed();

    // Calibration data
    state_report.has_calibration = true;
    state_report.calibration.valid =
        g_polarizer_wheel_instance.calibration.calibration_complete;
    if (state_report.calibration.valid) {
        state_report.calibration.pass_through_width =
            g_polarizer_wheel_instance.calibration.bump_width_pass_through;
        state_report.calibration.vertical_width =
            g_polarizer_wheel_instance.calibration.bump_width_vertical;
        state_report.calibration.horizontal_width =
            g_polarizer_wheel_instance.calibration.bump_width_horizontal;
    }

    LOG_INF(
        "Polarizer state: %d -> %d [%u], step_diff=%u, time=%u ms, "
        "accel=%uµsteps/s2, max_speed=%u ms/turn",
        state_report.previous_position, state_report.current_position,
        (uint32_t)atomic_get(&g_polarizer_wheel_instance.step_count.current),
        state_report.step_loss_microsteps, state_report.transition_time_ms,
        state_report.acceleration_steps_per_s2,
        state_report.max_speed_ms_per_turn);

    return publish_new(&state_report, sizeof(state_report),
                       orb_mcu_main_McuToJetson_polarizer_wheel_state_tag,
                       CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX);
}

/**
 * Convert angle in decidegrees to PolarizerWheelState Position enum
 */
static orb_mcu_main_PolarizerWheelState_Position
angle_to_position(uint32_t angle_decidegrees)
{
    switch (angle_decidegrees) {
    case POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE:
        return orb_mcu_main_PolarizerWheelState_Position_PASS_THROUGH;
    case POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE:
        return orb_mcu_main_PolarizerWheelState_Position_VERTICAL;
    case POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE:
        return orb_mcu_main_PolarizerWheelState_Position_HORIZONTAL;
    default:
        return orb_mcu_main_PolarizerWheelState_Position_UNKNOWN;
    }
}

/**
 * Get current position as PolarizerWheelState Position enum based on step count
 */
static orb_mcu_main_PolarizerWheelState_Position
get_current_position(void)
{
    int32_t current_step =
        atomic_get(&g_polarizer_wheel_instance.step_count.current);

    uint32_t angle_decidegrees =
        (uint32_t)current_step * 3600 / POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;

    // POLARIZER_WHEEL_MICROSTEPS_PER_STEP is 7.5 degrees
    // tolerance set to ~1 degree
    const uint32_t tolerance = POLARIZER_WHEEL_MICROSTEPS_PER_STEP / 7;
    if (angle_decidegrees <= tolerance ||
        angle_decidegrees >= (3600 - tolerance)) {
        return orb_mcu_main_PolarizerWheelState_Position_PASS_THROUGH;
    } else if (angle_decidegrees >= (1200 - tolerance) &&
               angle_decidegrees <= (1200 + tolerance)) {
        return orb_mcu_main_PolarizerWheelState_Position_VERTICAL;
    } else if (angle_decidegrees >= (2400 - tolerance) &&
               angle_decidegrees <= (2400 + tolerance)) {
        return orb_mcu_main_PolarizerWheelState_Position_HORIZONTAL;
    }

    return orb_mcu_main_PolarizerWheelState_Position_UNKNOWN;
}

/**
 * Get the edge-to-center distance for a given target position.
 * Uses calibrated bump width if available, otherwise falls back to default.
 *
 * @param target_step Target position in microsteps (0, 120°, or 240°
 * equivalent)
 * @return Distance from bump edge to center in microsteps
 */
static uint32_t
get_edge_to_center_for_position(int32_t target_step)
{
    if (!g_polarizer_wheel_instance.calibration.calibration_complete) {
        return POLARIZER_WHEEL_MICROSTEPS_NOTCH_EDGE_TO_CENTER;
    }

    /* Determine which position this target corresponds to */
    const int32_t pass_through_pos = 0;
    const int32_t vertical_pos = POLARIZER_WHEEL_MICROSTEPS_120_DEGREES;
    const int32_t horizontal_pos = POLARIZER_WHEEL_MICROSTEPS_120_DEGREES * 2;

    /* Allow some tolerance for position matching */
    const int32_t tolerance = POLARIZER_WHEEL_MICROSTEPS_PER_STEP;

    if (abs(target_step - pass_through_pos) < tolerance ||
        abs(target_step - POLARIZER_WHEEL_MICROSTEPS_360_DEGREES) < tolerance) {
        return g_polarizer_wheel_instance.calibration.bump_width_pass_through /
               2;
    } else if (abs(target_step - vertical_pos) < tolerance) {
        return g_polarizer_wheel_instance.calibration.bump_width_vertical / 2;
    } else if (abs(target_step - horizontal_pos) < tolerance) {
        return g_polarizer_wheel_instance.calibration.bump_width_horizontal / 2;
    }

    /* For custom angles, use default */
    return POLARIZER_WHEEL_MICROSTEPS_NOTCH_EDGE_TO_CENTER;
}

/**
 * Calculate the notch edge position for encoder-assisted positioning
 */
static int32_t
calculate_notch_edge(int32_t target_step,
                     enum polarizer_wheel_direction_e direction)
{
    uint32_t edge_to_center = get_edge_to_center_for_position(target_step);
    int32_t edge;

    if (direction == POLARIZER_WHEEL_DIRECTION_FORWARD) {
        edge = target_step - (int32_t)edge_to_center;
        if (edge < 0) {
            edge += POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
        }
    } else {
        edge = target_step + (int32_t)edge_to_center;
        if (edge >= POLARIZER_WHEEL_MICROSTEPS_360_DEGREES) {
            edge -= POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
        }
    }
    return edge;
}

/*******************************************************************************
 * ISR Handlers - Keep minimal! Only update counters and signal semaphores.
 ******************************************************************************/

/**
 * ISR for encoder notch detection
 * Only signals the encoder semaphore - all logic is in the thread
 */
static void
encoder_callback(const struct device *dev, struct gpio_callback *cb,
                 uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(polarizer_encoder_spec.pin)) {
        if (g_polarizer_wheel_instance.state == STATE_CALIBRATING) {
            /* During calibration, signal on both rising and falling edges.
             * The thread determines edge type by reading the pin state. */
            k_sem_give(&encoder_sem);
        } else {
            /* For homing/positioning, only signal on rising edge (entering
             * bump) */
            if (gpio_pin_get_dt(&polarizer_encoder_spec) == 1) {
                k_sem_give(&encoder_sem);
            }
        }
    }
}

/**
 * ISR for motor step pulse
 * Updates step counter and signals step semaphore
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

        /* Update step counter */
        if (g_polarizer_wheel_instance.step_count.direction ==
            POLARIZER_WHEEL_DIRECTION_BACKWARD) {
            atomic_dec(&g_polarizer_wheel_instance.step_count.current);
        } else {
            atomic_inc(&g_polarizer_wheel_instance.step_count.current);
        }

        /* Handle wrap-around */
        if (atomic_get(&g_polarizer_wheel_instance.step_count.current) >=
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES) {
            atomic_clear(&g_polarizer_wheel_instance.step_count.current);
        }
        if (atomic_get(&g_polarizer_wheel_instance.step_count.current) < 0) {
            atomic_set(&g_polarizer_wheel_instance.step_count.current,
                       POLARIZER_WHEEL_MICROSTEPS_360_DEGREES - 1);
        }

        /* Signal step event to thread */
        k_sem_give(&step_sem);
    }
}

/*******************************************************************************
 * Thread Processing Functions
 ******************************************************************************/

/**
 * Process a motor step event - handles stopping, acceleration/deceleration
 * and encoder management.
 *
 * Priority order within this function:
 * 1. Check if target reached -> STOP immediately
 * 2. Handle acceleration/deceleration (frequency updates)
 * 3. Check deceleration trigger distance
 * 4. Enable encoder when close to target
 */
static void
process_step_event(void)
{
    const int32_t current_pos =
        atomic_get(&g_polarizer_wheel_instance.step_count.current);
    const int32_t target_pos =
        atomic_get(&g_polarizer_wheel_instance.step_count.target);

    /*
     * [HIGHEST PRIORITY] Check if target reached - stop immediately
     * This must be checked FIRST to minimize overshoot
     */
    if (target_pos == current_pos) {
        /* For positioning mode: check if encoder was triggered */
        if (g_polarizer_wheel_instance.state ==
                STATE_POSITIONING_WITH_ENCODER &&
            atomic_get(
                &g_polarizer_wheel_instance.positioning.encoder_triggered) ==
                0) {
            polarizer_halt();
            LOG_ERR("Encoder not triggered during positioning! "
                    "Expected edge at %d, current pos: %d",
                    g_polarizer_wheel_instance.positioning.target_notch_edge,
                    current_pos);
            /* Trigger homing to reset polarizer state */
            g_polarizer_wheel_instance.pending_cmd.type = CMD_HOME;
            k_sem_give(&cmd_sem);
            return;
        }

        polarizer_halt();

        /* Report state when reaching standard positions.
         * For encoder-assisted mode, only report if encoder was triggered.
         * For open-loop mode, always report for standard positions. */
        const bool is_standard_position =
            g_polarizer_wheel_instance.positioning.target_position !=
            orb_mcu_main_PolarizerWheelState_Position_UNKNOWN;
        const bool encoder_assisted =
            g_polarizer_wheel_instance.state == STATE_POSITIONING_WITH_ENCODER;
        const bool encoder_ok =
            !encoder_assisted ||
            atomic_get(
                &g_polarizer_wheel_instance.positioning.encoder_triggered) == 1;

        if (is_standard_position && encoder_ok) {
            report_reached_state();
        }

        /* Disable encoder if enabled */
        if (g_polarizer_wheel_instance.positioning.encoder_enabled) {
            disable_encoder();
        }

        g_polarizer_wheel_instance.state = STATE_IDLE;

        /* Schedule delayed current scale-down */
        g_polarizer_wheel_instance.idle_current_scale_down_time_ms =
            k_uptime_get_32() + POLARIZER_IDLE_CURRENT_DELAY_MS;

#if CONFIG_POLARIZER_LOG_LEVEL_DBG
        uint32_t elapsed_ms =
            k_uptime_get_32() -
            g_polarizer_wheel_instance.positioning.start_time_ms;
        LOG_DBG("Reached target=%d; time=%u ms; min_freq=%u; max_freq=%u",
                target_pos, elapsed_ms,
                g_polarizer_wheel_instance.debug_stats.min_frequency,
                g_polarizer_wheel_instance.debug_stats.max_frequency);
#endif
        return; /* Target reached, nothing more to do */
    }

    /* Handle acceleration/deceleration (frequency computed from distance) */
    if (g_polarizer_wheel_instance.acceleration.state == ACCELERATION_ACTIVE) {
        const uint32_t distance_traveled =
            (uint32_t)abs(circular_signed_distance(
                g_polarizer_wheel_instance.acceleration.start_position,
                current_pos));
        const uint32_t total_dist =
            g_polarizer_wheel_instance.acceleration.total_distance;
        const uint32_t midpoint = total_dist / 2;

        /*
         * Symmetric triangular velocity profile:
         * - First half: accelerate (ramp_step increases from 0 to midpoint)
         * - Second half: decelerate (ramp_step decreases from midpoint to 0)
         * Peak velocity is determined solely by acceleration and course length.
         */
        const uint32_t ramp_step = (distance_traveled < midpoint)
                                       ? distance_traveled
                                       : (total_dist - distance_traveled);

        uint32_t new_freq = calculate_linear_ramp_frequency(
            ramp_step, g_polarizer_wheel_instance.acceleration.min_frequency,
            get_max_frequency());

        const int ret = polarizer_set_frequency(new_freq);
        ASSERT_SOFT(ret);
        if (ret == 0) {
            g_polarizer_wheel_instance.acceleration.current_frequency =
                new_freq;
        }

#if CONFIG_POLARIZER_LOG_LEVEL_DBG
        if (new_freq < g_polarizer_wheel_instance.debug_stats.min_frequency) {
            g_polarizer_wheel_instance.debug_stats.min_frequency = new_freq;
        }
        if (new_freq > g_polarizer_wheel_instance.debug_stats.max_frequency) {
            g_polarizer_wheel_instance.debug_stats.max_frequency = new_freq;
        }
#endif
    }
}

/**
 * Process encoder event during positioning
 */
static void
process_encoder_event_positioning(void)
{
    if (atomic_get(&g_polarizer_wheel_instance.positioning.encoder_triggered) ==
        1) {
        /* Already triggered, ignore */
        return;
    }

    /* Mark encoder as triggered */
    atomic_set(&g_polarizer_wheel_instance.positioning.encoder_triggered, 1);

    /* Calculate step difference for reporting */
    int32_t current_position =
        atomic_get(&g_polarizer_wheel_instance.step_count.current);
    int32_t step_diff = circular_signed_distance(
        g_polarizer_wheel_instance.positioning.target_notch_edge,
        current_position);

    int32_t step_loss = (g_polarizer_wheel_instance.step_count.direction ==
                         POLARIZER_WHEEL_DIRECTION_FORWARD)
                            ? -step_diff
                            : step_diff;

    g_polarizer_wheel_instance.positioning.step_diff_microsteps =
        (uint32_t)abs(step_loss);

    LOG_DBG("Step %s detected: %d steps (current=%d, target=%d, dir=%d)",
            step_loss > 0 ? "gain" : "loss", abs(step_loss), current_position,
            g_polarizer_wheel_instance.positioning.target_notch_edge,
            g_polarizer_wheel_instance.step_count.direction);

    /* Correct step counter to expected edge position */
    atomic_set(&g_polarizer_wheel_instance.step_count.current,
               g_polarizer_wheel_instance.positioning.target_notch_edge);

    LOG_DBG("Encoder-assisted: edge=%d, target=%ld",
            g_polarizer_wheel_instance.positioning.target_notch_edge,
            atomic_get(&g_polarizer_wheel_instance.step_count.target));
}

/**
 * Start a relative rotation
 */
static int
polarizer_wheel_step_relative(const uint32_t frequency,
                              const int32_t step_count)
{
    if (frequency == 0 || step_count == 0 ||
        abs(step_count) > POLARIZER_WHEEL_MICROSTEPS_360_DEGREES) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (step_count < 0) {
        set_direction(POLARIZER_WHEEL_DIRECTION_BACKWARD);
        int32_t target =
            atomic_get(&g_polarizer_wheel_instance.step_count.current) +
            step_count;
        if (target < 0) {
            target += POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
        }
        atomic_set(&g_polarizer_wheel_instance.step_count.target, target);
    } else {
        set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);
        int32_t target =
            (atomic_get(&g_polarizer_wheel_instance.step_count.current) +
             step_count) %
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
        atomic_set(&g_polarizer_wheel_instance.step_count.target, target);
    }

    return polarizer_rotate(frequency);
}

/**
 * Execute homing procedure
 * Called from the main thread.
 *
 * Note: This function blocks the thread while waiting for encoder events.
 * This is acceptable because:
 * 1. Homing only runs at startup or on error recovery
 * 2. Step counting is handled by ISR updating atomic counter
 * 3. No other operations should run during homing
 */
static void
execute_homing(void)
{
    clear_step_interrupt();

    g_polarizer_wheel_instance.state = STATE_HOMING;
    g_polarizer_wheel_instance.acceleration.state = ACCELERATION_IDLE;
    g_polarizer_wheel_instance.positioning.previous_position =
        orb_mcu_main_PolarizerWheelState_Position_UNKNOWN;
    g_polarizer_wheel_instance.positioning.target_position =
        orb_mcu_main_PolarizerWheelState_Position_PASS_THROUGH;
    g_polarizer_wheel_instance.positioning.step_diff_microsteps = 0;
    g_polarizer_wheel_instance.positioning.start_time_ms = k_uptime_get_32();

    /* Enable encoder for notch detection */
    enable_encoder();

    bool notch_0_detected = false;
    g_polarizer_wheel_instance.homing.notch_count = 0;
    set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);

    while (!notch_0_detected && g_polarizer_wheel_instance.homing.notch_count <
                                    POLARIZER_WHEEL_NOTCH_DETECT_ATTEMPTS) {
        size_t spin_attempt = 0;

        while (spin_attempt < POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS) {
            atomic_clear(&g_polarizer_wheel_instance.step_count.current);
            k_sem_reset(&encoder_sem);

            /* Spin the wheel 240 degrees */
            int ret = polarizer_wheel_step_relative(
                POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                POLARIZER_WHEEL_MICROSTEPS_120_DEGREES * 2);
            if (ret != RET_SUCCESS) {
                LOG_ERR("Unable to spin polarizer wheel: %d, attempt %u", ret,
                        spin_attempt);
                g_polarizer_wheel_instance.homing.success = false;
                polarizer_halt();
                disable_encoder();
                ORB_STATE_SET_CURRENT(RET_ERROR_INTERNAL, "unable to spin");
                g_polarizer_wheel_instance.state = STATE_UNINITIALIZED;
                return;
            }

            /* Wait for encoder event or timeout */
            ret = k_sem_take(&encoder_sem, K_SECONDS(4));
            if (ret == 0) {
                /* Encoder triggered - stop motor */
                polarizer_halt();
                break;
            }
            spin_attempt++;
        }

        if (spin_attempt != 0) {
            LOG_WRN("Spin attempt %u, current step counter: %ld", spin_attempt,
                    atomic_get(&g_polarizer_wheel_instance.step_count.current));
            if (spin_attempt == POLARIZER_WHEEL_HOMING_SPIN_ATTEMPTS) {
                ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED,
                                      "no encoder: no wheel? stalled?");
                g_polarizer_wheel_instance.homing.success = false;
                g_polarizer_wheel_instance.state = STATE_UNINITIALIZED;
                polarizer_halt();
                disable_encoder();
                LOG_WRN(
                    "Encoder not detected, is there a wheel? is it moving?");
                return;
            }
        }

        LOG_INF("homing: steps: %ld, notch count: %d",
                atomic_get(&g_polarizer_wheel_instance.step_count.current),
                g_polarizer_wheel_instance.homing.notch_count);

        /* Check if this is the close notch pair (notch 0) */
        int32_t current_steps =
            atomic_get(&g_polarizer_wheel_instance.step_count.current);
        if (g_polarizer_wheel_instance.homing.notch_count != 0 &&
            current_steps < POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MAX &&
            current_steps > POLARIZER_CLOSE_NOTCH_DETECTION_MICROSTEPS_MIN) {
            notch_0_detected = true;
        }

        g_polarizer_wheel_instance.homing.notch_count++;
        atomic_clear(&g_polarizer_wheel_instance.step_count.current);
    }

    if (notch_0_detected) {
        /* Success - move to passthrough position with encoder assistance */
        k_sem_reset(&encoder_sem);

        /* Start movement towards passthrough position (120 degrees away) */
        int ret = polarizer_wheel_step_relative(
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
            POLARIZER_WHEEL_MICROSTEPS_NOTCH_EDGE_TO_CENTER +
                POLARIZER_WHEEL_MICROSTEPS_120_DEGREES);
        ASSERT_SOFT(ret);

        /* Wait for encoder to detect passthrough notch */
        ret = k_sem_take(&encoder_sem, K_SECONDS(4));
        if (ret == 0) {
            /* Encoder triggered - correct step counter to notch edge */
            int32_t current_pos =
                atomic_get(&g_polarizer_wheel_instance.step_count.current);
            int32_t expected_edge = POLARIZER_WHEEL_MICROSTEPS_120_DEGREES;

            LOG_DBG("Homing encoder: current=%d, expected_edge=%d", current_pos,
                    expected_edge);

            atomic_set(&g_polarizer_wheel_instance.step_count.current,
                       expected_edge);

            /* Update target to finish at center */
            int32_t target =
                expected_edge + POLARIZER_WHEEL_MICROSTEPS_NOTCH_EDGE_TO_CENTER;
            atomic_set(&g_polarizer_wheel_instance.step_count.target, target);

            /* Wait for remaining steps to complete */
            uint32_t timeout_ms = 1000;
            uint32_t start_time = k_uptime_get_32();
            while (atomic_get(&g_polarizer_wheel_instance.step_count.current) !=
                       target &&
                   (k_uptime_get_32() - start_time) < timeout_ms) {
                k_sem_take(&step_sem, K_MSEC(10));
            }
        } else {
            LOG_WRN("Encoder not triggered during homing to passthrough");
        }

        disable_encoder();
        polarizer_halt();

        LOG_INF("Polarizer wheel homed");
        if (g_polarizer_wheel_instance.calibration.calibration_complete) {
            ORB_STATE_SET_CURRENT(
                RET_SUCCESS, "homed,cal:%u,%u,%u",
                g_polarizer_wheel_instance.calibration.bump_width_pass_through,
                g_polarizer_wheel_instance.calibration.bump_width_vertical,
                g_polarizer_wheel_instance.calibration.bump_width_horizontal);
        } else {
            ORB_STATE_SET_CURRENT(RET_SUCCESS, "homed");
        }

        g_polarizer_wheel_instance.state = STATE_IDLE;
        g_polarizer_wheel_instance.homing.success = true;
        atomic_clear(&g_polarizer_wheel_instance.step_count.current);

        report_reached_state();

        /* Trigger calibration if needed (e.g., during startup) */
        if (g_polarizer_wheel_instance.calibration.needs_calibration) {
            g_polarizer_wheel_instance.calibration.needs_calibration = false;
            LOG_INF("Starting bump width calibration after homing");
            execute_calibration();
            return; /* execute_calibration will call execute_homing again */
        }
    } else {
        disable_encoder();
        g_polarizer_wheel_instance.homing.success = false;
        g_polarizer_wheel_instance.state = STATE_UNINITIALIZED;
        polarizer_halt();

        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED,
                              "bumps not correctly detected");
    }

    /* Schedule idle current scale-down */
    g_polarizer_wheel_instance.idle_current_scale_down_time_ms =
        k_uptime_get_32() + POLARIZER_IDLE_CURRENT_DELAY_MS;
}

/**
 * Execute bump width calibration.
 * Spins at least one full turns from pass-through position to measure bump
 * widths. Must be called after homing is complete (wheel at pass_through
 * position).
 *
 * Bump order when spinning forward from pass_through:
 * 1. Exit pass_through (falling edge) - can't measure, we start centered
 * 2. vertical (rising + falling) - measure
 * 3. extra bump (rising + falling) - skip
 * 4. horizontal (rising + falling) - measure
 * 5. pass_through (rising + falling) - measure
 * 6. Repeat for second turn (for verification/averaging if needed)
 */
static void
execute_calibration(void)
{
    if (g_polarizer_wheel_instance.state != STATE_IDLE) {
        LOG_ERR("Calibration requires IDLE state, current: %d",
                g_polarizer_wheel_instance.state);
        return;
    }

    if (!g_polarizer_wheel_instance.homing.success) {
        LOG_ERR("Calibration requires successful homing first");
        return;
    }

    if (get_current_position() !=
        orb_mcu_main_PolarizerWheelState_Position_PASS_THROUGH) {
        LOG_ERR("Calibration requires to be at pass through position first");
        return;
    }

    LOG_INF("Starting bump width calibration");
    g_polarizer_wheel_instance.state = STATE_CALIBRATING;
    g_polarizer_wheel_instance.acceleration.state = ACCELERATION_IDLE;

    /* Initialize calibration state */
    g_polarizer_wheel_instance.calibration.bump_width_pass_through = 0;
    g_polarizer_wheel_instance.calibration.bump_width_vertical = 0;
    g_polarizer_wheel_instance.calibration.bump_width_horizontal = 0;
    g_polarizer_wheel_instance.calibration.bump_index = 0;
    g_polarizer_wheel_instance.calibration.bump_entry_position = 0;
    g_polarizer_wheel_instance.calibration.inside_bump = false;
    g_polarizer_wheel_instance.calibration.calibration_complete = false;

    /* Configure encoder for both edge detection */
    int ret = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_ACTIVE);
    if (ret) {
        LOG_ERR("Failed to enable encoder: %d", ret);
        g_polarizer_wheel_instance.state = STATE_IDLE;
        return;
    }

    ret = gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                          GPIO_INT_EDGE_BOTH);
    if (ret) {
        LOG_ERR("Failed to configure encoder interrupt: %d", ret);
        g_polarizer_wheel_instance.state = STATE_IDLE;
        return;
    }

    k_sem_reset(&encoder_sem);

    set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);

    /*
     * Process encoder events during calibration spin.
     * From pass_through center, spinning forward:
     * - vertical (rise/fall), horizontal (rise/fall), extra (rise/fall),
     *   pass_through (rise/fall)
     * - Second turn repeats the pattern
     *
     * Bump index mapping:
     * 0,4 = vertical, 1,5 = extra (skip), 2,6 = horizontal, 3,7 = pass_through
     *
     * We spin 2x 270 degrees (step_relative is limited to 360 degrees max)
     */
    bool calibration_done = false;

    /* Spin 2 times 270 degrees */
    for (int spin = 0; spin < 2 && !calibration_done; spin++) {
        k_sem_reset(&encoder_sem);

        ret = polarizer_wheel_step_relative(
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
            POLARIZER_WHEEL_MICROSTEPS_360_DEGREES * 3 / 4);
        if (ret != RET_SUCCESS) {
            LOG_ERR("Unable to spin for calibration (spin %d): %d", spin, ret);
            gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec,
                                            GPIO_INT_DISABLE);
            g_polarizer_wheel_instance.state = STATE_IDLE;
            return;
        }

        uint32_t spin_timeout_ms = 4000; /* 4 seconds per 180 degree spin */
        uint32_t spin_start = k_uptime_get_32();

        while ((k_uptime_get_32() - spin_start) < spin_timeout_ms) {
            ret = k_sem_take(&encoder_sem, K_MSEC(50));
            if (ret == 0) {
                /* Encoder edge detected */
                int32_t current_pos =
                    atomic_get(&g_polarizer_wheel_instance.step_count.current);
                bool is_high = gpio_pin_get_dt(&polarizer_encoder_spec) == 1;

                if (is_high) {
                    /* Rising edge: entering a bump */
                    g_polarizer_wheel_instance.calibration.bump_entry_position =
                        current_pos;
                    g_polarizer_wheel_instance.calibration.inside_bump = true;
                    LOG_DBG("Calibration: entering bump at pos %d (index %d)",
                            current_pos,
                            g_polarizer_wheel_instance.calibration.bump_index);
                } else {
                    /* Falling edge: exiting a bump */
                    if (g_polarizer_wheel_instance.calibration.inside_bump) {
                        /* Calculate bump width, handling wrap-around at 360° */
                        int32_t entry_pos =
                            (int32_t)g_polarizer_wheel_instance.calibration
                                .bump_entry_position;
                        int32_t width = current_pos - entry_pos;
                        if (width < 0) {
                            /* Wrapped around 0 position */
                            width += POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
                        }
                        uint32_t bump_width = (uint32_t)width;

                        /* Map bump index to position:
                         * 0,4 = vertical, 1,5 = extra (skip),
                         * 2,6 = horizontal, 3,7 = pass_through */
                        uint8_t position_index =
                            g_polarizer_wheel_instance.calibration.bump_index %
                            4;

                        switch (position_index) {
                        case 0: /* vertical */
                            if (g_polarizer_wheel_instance.calibration
                                    .bump_width_vertical == 0) {
                                g_polarizer_wheel_instance.calibration
                                    .bump_width_vertical = bump_width;
                            }
                            LOG_INF(
                                "Calibration: vertical width = %u microsteps",
                                bump_width);
                            break;
                        case 1: /* extra bump - skip */
                            LOG_DBG("Calibration: extra bump width = %u "
                                    "microsteps (skipped)",
                                    bump_width);
                            break;
                        case 2: /* horizontal */
                            if (g_polarizer_wheel_instance.calibration
                                    .bump_width_horizontal == 0) {
                                g_polarizer_wheel_instance.calibration
                                    .bump_width_horizontal = bump_width;
                            }
                            LOG_INF(
                                "Calibration: horizontal width = %u microsteps",
                                bump_width);
                            break;
                        case 3: /* pass_through */
                            if (g_polarizer_wheel_instance.calibration
                                    .bump_width_pass_through == 0) {
                                g_polarizer_wheel_instance.calibration
                                    .bump_width_pass_through = bump_width;
                            }
                            LOG_INF("Calibration: pass_through width = %u "
                                    "microsteps",
                                    bump_width);
                            break;
                        }

                        g_polarizer_wheel_instance.calibration.bump_index++;
                        g_polarizer_wheel_instance.calibration.inside_bump =
                            false;

                        /* Check if we have all measurements >
                         * POLARIZER_WHEEL_MICROSTEPS_PER_STEP <=> 7.5º the bump
                         * widths are 11º
                         */
                        if (g_polarizer_wheel_instance.calibration
                                    .bump_width_pass_through >
                                POLARIZER_WHEEL_MICROSTEPS_PER_STEP &&
                            g_polarizer_wheel_instance.calibration
                                    .bump_width_vertical >
                                POLARIZER_WHEEL_MICROSTEPS_PER_STEP &&
                            g_polarizer_wheel_instance.calibration
                                    .bump_width_horizontal >
                                POLARIZER_WHEEL_MICROSTEPS_PER_STEP) {
                            LOG_INF("Calibration: all bumps measured");
                            calibration_done = true;
                            break;
                        }
                    }
                }
            }

            /* Check if motor stopped (target reached) */
            if (atomic_get(&g_polarizer_wheel_instance.step_count.current) ==
                atomic_get(&g_polarizer_wheel_instance.step_count.target)) {
                LOG_DBG("Calibration: spin %d complete", spin);
                break;
            }
        }

        /* Wait for motor to fully stop before next spin */
        polarizer_halt();
    }

    polarizer_halt();

    /* Disable both-edge detection, restore rising-edge only */
    gpio_pin_interrupt_configure_dt(&polarizer_encoder_spec, GPIO_INT_DISABLE);

    /* Verify calibration succeeded */
    if (g_polarizer_wheel_instance.calibration.bump_width_pass_through > 0 &&
        g_polarizer_wheel_instance.calibration.bump_width_vertical > 0 &&
        g_polarizer_wheel_instance.calibration.bump_width_horizontal > 0) {
        g_polarizer_wheel_instance.calibration.calibration_complete = true;
        ORB_STATE_SET_CURRENT(
            RET_SUCCESS, "calibrated: %u,%u,%u",
            g_polarizer_wheel_instance.calibration.bump_width_pass_through,
            g_polarizer_wheel_instance.calibration.bump_width_vertical,
            g_polarizer_wheel_instance.calibration.bump_width_horizontal);
        LOG_INF("Bump calibration complete: pass_through=%u, vertical=%u, "
                "horizontal=%u microsteps",
                g_polarizer_wheel_instance.calibration.bump_width_pass_through,
                g_polarizer_wheel_instance.calibration.bump_width_vertical,
                g_polarizer_wheel_instance.calibration.bump_width_horizontal);
    } else {
        LOG_WRN("Bump calibration incomplete: pass_through=%u, vertical=%u, "
                "horizontal=%u",
                g_polarizer_wheel_instance.calibration.bump_width_pass_through,
                g_polarizer_wheel_instance.calibration.bump_width_vertical,
                g_polarizer_wheel_instance.calibration.bump_width_horizontal);
    }

    /* Queue homing to return to known position.
     * Set needs_calibration to false to prevent re-triggering calibration. */
    g_polarizer_wheel_instance.state = STATE_IDLE;
    g_polarizer_wheel_instance.homing.success = false;
    g_polarizer_wheel_instance.calibration.needs_calibration = false;
    LOG_INF("Calibration done, queuing homing");
    execute_homing();
}

/**
 * Execute set_angle command
 * Called from thread context after command is dequeued.
 * Parameters are pre-validated by the public API.
 */
static ret_code_t
execute_set_angle(uint32_t frequency, uint32_t angle_decidegrees,
                  bool shortest_path)
{
    /* State should be IDLE since we checked before queuing, but verify */
    if (g_polarizer_wheel_instance.state == STATE_UNINITIALIZED) {
        LOG_ERR("execute_set_angle called in uninitialized state");
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (g_polarizer_wheel_instance.state != STATE_IDLE) {
        LOG_ERR("execute_set_angle called in non-idle state: %d",
                g_polarizer_wheel_instance.state);
        return RET_ERROR_BUSY;
    }

    atomic_val_t target_step = ((int)angle_decidegrees *
                                POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 3600) %
                               POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;

    g_polarizer_wheel_instance.positioning.previous_position =
        get_current_position();
    g_polarizer_wheel_instance.positioning.target_position =
        angle_to_position(angle_decidegrees);
    g_polarizer_wheel_instance.positioning.step_diff_microsteps = 0;
    g_polarizer_wheel_instance.positioning.start_time_ms = k_uptime_get_32();

    if (g_polarizer_wheel_instance.positioning.previous_position ==
            g_polarizer_wheel_instance.positioning.target_position &&
        g_polarizer_wheel_instance.positioning.previous_position !=
            orb_mcu_main_PolarizerWheelState_Position_UNKNOWN) {
        report_reached_state();
        return RET_SUCCESS;
    }

    /* Determine direction based on shortest_path flag */
    int32_t signed_dist;
    if (shortest_path) {
        /* Use shortest path - may go backward */
        signed_dist = circular_signed_distance(
            atomic_get(&g_polarizer_wheel_instance.step_count.current),
            target_step);
        if (signed_dist > 0) {
            set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);
        } else {
            set_direction(POLARIZER_WHEEL_DIRECTION_BACKWARD);
        }
    } else {
        /* Always go forward (more reliable) */
        set_direction(POLARIZER_WHEEL_DIRECTION_FORWARD);
        int32_t current =
            atomic_get(&g_polarizer_wheel_instance.step_count.current);
        signed_dist = (int32_t)target_step - current;
        if (signed_dist <= 0) {
            /* Wrap around: go the long way forward */
            signed_dist += POLARIZER_WHEEL_MICROSTEPS_360_DEGREES;
        }
    }

    LOG_INF("Set angle: %u deci-deg, pos: %d, steps: %ld, dir: %d",
            angle_decidegrees,
            g_polarizer_wheel_instance.positioning.target_position,
            atomic_get(&target_step),
            g_polarizer_wheel_instance.step_count.direction);

    /* Use encoder-assisted positioning for standard positions at fixed speed.
     * Encoder is not used during acceleration ramp. */
    const bool use_encoder =
        g_polarizer_wheel_instance.positioning.target_position !=
            orb_mcu_main_PolarizerWheelState_Position_UNKNOWN &&
        frequency != 0;

    /* Determine velocity mode:
     * - frequency == 0: use triangular acceleration ramp based on course length
     * - frequency != 0: use constant velocity at the specified frequency */
    const bool use_ramp = (frequency == 0);
    const uint32_t actual_frequency =
        use_ramp ? POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT : frequency;

    if (use_encoder) {
        g_polarizer_wheel_instance.positioning.target_notch_edge =
            calculate_notch_edge(
                target_step, g_polarizer_wheel_instance.step_count.direction);

        g_polarizer_wheel_instance.positioning.encoder_enabled = false;
        atomic_clear(&g_polarizer_wheel_instance.positioning.encoder_triggered);
        enable_encoder();

        LOG_DBG("Encoder-assisted: angle=%u, target=%ld, edge=%d, dir=%d",
                angle_decidegrees, target_step,
                g_polarizer_wheel_instance.positioning.target_notch_edge,
                g_polarizer_wheel_instance.step_count.direction);
    }

    if (use_ramp) {
        /* Set up triangular acceleration ramp based on course length.
         * Peak velocity is determined by fixed acceleration and distance. */
        const int32_t start_pos =
            atomic_get(&g_polarizer_wheel_instance.step_count.current);
        const uint32_t total_dist = (uint32_t)abs(signed_dist);

        g_polarizer_wheel_instance.acceleration.state = ACCELERATION_ACTIVE;
        g_polarizer_wheel_instance.acceleration.min_frequency =
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT;
        g_polarizer_wheel_instance.acceleration.current_frequency =
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT;
        g_polarizer_wheel_instance.acceleration.start_position = start_pos;
        g_polarizer_wheel_instance.acceleration.total_distance = total_dist;
        LOG_DBG("Triangular ramp: min %u Hz, total %u steps, accel %u "
                "steps/s², max speed %u ms/turn",
                g_polarizer_wheel_instance.acceleration.min_frequency,
                total_dist, polarizer_wheel_get_acceleration(),
                polarizer_wheel_get_max_speed());

        g_polarizer_wheel_instance.positioning.frequency =
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT;
    } else {
        /* Constant velocity mode */
        g_polarizer_wheel_instance.acceleration.state = ACCELERATION_IDLE;
        g_polarizer_wheel_instance.acceleration.current_frequency = frequency;
        g_polarizer_wheel_instance.acceleration.min_frequency = frequency;
        g_polarizer_wheel_instance.positioning.frequency = frequency;
        LOG_DBG("Constant velocity: %u Hz", frequency);
    }

    atomic_set(&g_polarizer_wheel_instance.step_count.target, target_step);

#if CONFIG_POLARIZER_LOG_LEVEL_DBG
    g_polarizer_wheel_instance.debug_stats.min_frequency = UINT32_MAX;
    g_polarizer_wheel_instance.debug_stats.max_frequency = 0;
#endif

    ret_code_t ret_val = polarizer_rotate(actual_frequency);

    if (ret_val == 0) {
        g_polarizer_wheel_instance.state =
            use_encoder ? STATE_POSITIONING_WITH_ENCODER : STATE_POSITIONING;
    } else {
        LOG_WRN("Unable to spin the wheel: %d", ret_val);
        g_polarizer_wheel_instance.state = STATE_IDLE;
    }

    return ret_val;
}

/*******************************************************************************
 * Main Thread
 ******************************************************************************/

/**
 * Main polarizer wheel thread
 * Processes events from ISRs and commands from API
 */
static void
polarizer_wheel_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    /* Use k_poll for efficient multi-semaphore waiting */
    struct k_poll_event events[3] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                                 K_POLL_MODE_NOTIFY_ONLY, &step_sem),
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                                 K_POLL_MODE_NOTIFY_ONLY, &encoder_sem),
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                                 K_POLL_MODE_NOTIFY_ONLY, &cmd_sem),
    };

    while (1) {
        /* Wait for any event or timeout */
        int ret = k_poll(events, ARRAY_SIZE(events), get_poll_timeout());
        if (ret != 0 && ret != -EAGAIN) {
            continue;
        }

        /*
         * Event processing priority (highest to lowest):
         * 1. Step events - contains stop logic, must be processed first
         * 2. Encoder events - position correction during movement
         * 3. Commands - new movement requests
         * 4. Idle current timeout - non-critical housekeeping
         */

        /* [HIGH PRIORITY] Process step event - stop logic lives here */
        if (events[0].state == K_POLL_STATE_SEM_AVAILABLE) {
            k_sem_take(&step_sem, K_NO_WAIT);
            events[0].state = K_POLL_STATE_NOT_READY;

            if (g_polarizer_wheel_instance.state ==
                    STATE_POSITIONING_WITH_ENCODER ||
                g_polarizer_wheel_instance.state == STATE_POSITIONING ||
                g_polarizer_wheel_instance.state == STATE_HOMING) {
                process_step_event();
            }
        }

        /* [MEDIUM PRIORITY] Process encoder event
         * Note: During homing, encoder events are handled synchronously in
         * execute_homing() via blocking k_sem_take, so we only process
         * encoder events for positioning mode here.
         */
        if (events[1].state == K_POLL_STATE_SEM_AVAILABLE) {
            k_sem_take(&encoder_sem, K_NO_WAIT);
            events[1].state = K_POLL_STATE_NOT_READY;

            if (g_polarizer_wheel_instance.state ==
                STATE_POSITIONING_WITH_ENCODER) {
                process_encoder_event_positioning();
            }
        }

        /* [NORMAL PRIORITY] Process command */
        if (events[2].state == K_POLL_STATE_SEM_AVAILABLE) {
            k_sem_take(&cmd_sem, K_NO_WAIT);
            events[2].state = K_POLL_STATE_NOT_READY;

            enum polarizer_wheel_cmd_e cmd =
                g_polarizer_wheel_instance.pending_cmd.type;
            g_polarizer_wheel_instance.pending_cmd.type = CMD_NONE;

            switch (cmd) {
            case CMD_HOME:
                execute_homing();
                break;
            case CMD_SET_ANGLE:
                execute_set_angle(
                    g_polarizer_wheel_instance.pending_cmd.set_angle.frequency,
                    g_polarizer_wheel_instance.pending_cmd.set_angle
                        .angle_decidegrees,
                    g_polarizer_wheel_instance.pending_cmd.set_angle
                        .shortest_path);
                break;
            case CMD_CALIBRATE:
                execute_calibration();
                break;
            default:
                break;
            }
        }

        /* [LOW PRIORITY] Handle idle current scale-down on timeout
         * This involves SPI communication and should not delay critical events
         */
        if (g_polarizer_wheel_instance.idle_current_scale_down_time_ms != 0 &&
            k_uptime_get_32() >=
                g_polarizer_wheel_instance.idle_current_scale_down_time_ms) {
            g_polarizer_wheel_instance.idle_current_scale_down_time_ms = 0;
            if (g_polarizer_wheel_instance.state == STATE_IDLE) {
                int scale_ret = drv8434s_scale_current(DRV8434S_TRQ_DAC_25);
                if (scale_ret) {
                    LOG_ERR("Failed to scale down idle current: %d", scale_ret);
                } else {
                    LOG_DBG("Scaled down motor current after idle timeout");
                }
            }
        }
    }
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

ret_code_t
polarizer_wheel_set_angle(const uint32_t frequency,
                          const uint32_t angle_decidegrees,
                          const bool shortest_path)
{
    /* Validate parameters upfront.
     * frequency == 0 is a special case: use triangular acceleration ramp.
     * Otherwise, frequency must be within valid range for constant velocity. */
    if (angle_decidegrees > 3600 || frequency > get_max_frequency() ||
        (frequency != 0 &&
         frequency < POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_MINIMUM)) {
        return RET_ERROR_INVALID_PARAM;
    }

    /* Lock to prevent concurrent command queuing */
    int ret = k_mutex_lock(&cmd_mutex, K_MSEC(100));
    if (ret != 0) {
        return RET_ERROR_BUSY;
    }

    if (g_polarizer_wheel_instance.state == STATE_UNINITIALIZED) {
        k_mutex_unlock(&cmd_mutex);
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (g_polarizer_wheel_instance.state != STATE_IDLE) {
        k_mutex_unlock(&cmd_mutex);
        return RET_ERROR_BUSY;
    }

    /* Queue the command */
    g_polarizer_wheel_instance.pending_cmd.type = CMD_SET_ANGLE;
    g_polarizer_wheel_instance.pending_cmd.set_angle.frequency = frequency;
    g_polarizer_wheel_instance.pending_cmd.set_angle.angle_decidegrees =
        angle_decidegrees;
    g_polarizer_wheel_instance.pending_cmd.set_angle.shortest_path =
        shortest_path;
    k_sem_give(&cmd_sem);

    k_mutex_unlock(&cmd_mutex);
    return RET_SUCCESS;
}

ret_code_t
polarizer_wheel_home_async(void)
{
    /* Lock to prevent concurrent command queuing */
    int ret = k_mutex_lock(&cmd_mutex, K_MSEC(100));
    if (ret != 0) {
        return RET_ERROR_BUSY;
    }

    if (g_polarizer_wheel_instance.state != STATE_IDLE &&
        g_polarizer_wheel_instance.state != STATE_UNINITIALIZED) {
        k_mutex_unlock(&cmd_mutex);
        return RET_ERROR_BUSY;
    }

    /* Queue the homing command */
    g_polarizer_wheel_instance.pending_cmd.type = CMD_HOME;
    g_polarizer_wheel_instance.homing.success = false;
    k_sem_give(&cmd_sem);

    k_mutex_unlock(&cmd_mutex);
    return RET_SUCCESS;
}

ret_code_t
polarizer_wheel_calibrate_async(void)
{
    /* Lock to prevent concurrent command queuing */
    int ret = k_mutex_lock(&cmd_mutex, K_MSEC(100));
    if (ret != 0) {
        return RET_ERROR_BUSY;
    }

    if (g_polarizer_wheel_instance.state == STATE_UNINITIALIZED ||
        !g_polarizer_wheel_instance.homing.success) {
        k_mutex_unlock(&cmd_mutex);
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (g_polarizer_wheel_instance.state != STATE_IDLE) {
        k_mutex_unlock(&cmd_mutex);
        return RET_ERROR_BUSY;
    }

    if (get_current_position() !=
        orb_mcu_main_PolarizerWheelState_Position_PASS_THROUGH) {
        k_mutex_unlock(&cmd_mutex);
        LOG_ERR("Calibration requires to be at pass through position first");
        return RET_ERROR_INVALID_STATE;
    }

    /* Queue the calibration command */
    g_polarizer_wheel_instance.pending_cmd.type = CMD_CALIBRATE;
    k_sem_give(&cmd_sem);

    k_mutex_unlock(&cmd_mutex);
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

    /* Clear the runtime context */
    memset(&g_polarizer_wheel_instance, 0, sizeof(g_polarizer_wheel_instance));
    g_polarizer_wheel_instance.state = STATE_UNINITIALIZED;

    /* Configure GPIO pins */
    ret_val =
        gpio_pin_configure_dt(&polarizer_spi_cs_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    ret_val = gpio_pin_configure_dt(&polarizer_enable_spec, GPIO_OUTPUT_ACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    ret_val = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_ACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    ret_val =
        gpio_pin_configure_dt(&polarizer_step_dir_spec, GPIO_OUTPUT_INACTIVE);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

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

    /* Initialize motor driver */
    ret_val = drv8434s_init(&drv8434_cfg);
    if (ret_val != 0) {
        ASSERT_SOFT(ret_val);
        ret_val = RET_ERROR_INTERNAL;
        goto exit;
    }

    const DRV8434S_DeviceCfg_t drv8434s_cfg = {
        .ctrl2.EN_OUT = DRV8434S_REG_CTRL2_VAL_ENOUT_DISABLE,
        .ctrl2.TOFF = DRV8434S_REG_CTRL2_VAL_TOFF_7US,
        .ctrl2.DECAY = DRV8434S_REG_CTRL2_VAL_DECAY_SMARTRIPPLE,
        .ctrl3.SPI_DIR = DRV8434S_REG_CTRL3_VAL_SPIDIR_PIN,
        .ctrl3.SPI_STEP = DRV8434S_REG_CTRL3_VAL_SPISTEP_PIN,
        .ctrl3.MICROSTEP_MODE =
            DRV8434S_MICROSTEP_MODE(POLARIZER_WHEEL_MICROSTEPS_PER_STEP),
        .ctrl4.LOCK = DRV8434S_REG_CTRL4_VAL_UNLOCK,
        .ctrl7.RC_RIPPLE = DRV8434S_REG_CTRL7_VAL_RCRIPPLE_1PERCENT,
        .ctrl7.EN_SSC = DRV8434S_REG_CTRL7_VAL_ENSSC_ENABLE,
        .ctrl7.TRQ_SCALE = DRV8434S_REG_CTRL7_VAL_TRQSCALE_NOSCALE};

    int timeout = 3;
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
    } while (--timeout >= 0 && ret_val != RET_SUCCESS);

    if (ret_val == RET_SUCCESS) {
        ret_val = drv8434s_enable();
        ASSERT_SOFT(ret_val);
        if (ret_val != RET_SUCCESS) {
            goto exit;
        }
    } else {
        goto exit;
    }

    /* Enable PWM interrupt */
    irq_connect_dynamic(pwm_timer_irq_n, 0, polarizer_wheel_step_isr, NULL, 0);
    irq_enable(pwm_timer_irq_n);

    /* Create the main polarizer thread */
    k_thread_create(&thread_data_polarizer_wheel, stack_area_polarizer_wheel,
                    K_THREAD_STACK_SIZEOF(stack_area_polarizer_wheel),
                    (k_thread_entry_t)polarizer_wheel_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_POLARIZER_WHEEL, 0, K_NO_WAIT);
    k_thread_name_set(&thread_data_polarizer_wheel, "polarizer");

    /* Queue initial homing with calibration */
    g_polarizer_wheel_instance.calibration.needs_calibration = true;
    ret_val = polarizer_wheel_home_async();

exit:
    if (ret_val != RET_SUCCESS) {
        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED, "init failed");
        g_polarizer_wheel_instance.state = STATE_UNINITIALIZED;
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

ret_code_t
polarizer_wheel_get_bump_widths(polarizer_wheel_bump_widths_t *widths)
{
    if (widths == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (!g_polarizer_wheel_instance.calibration.calibration_complete) {
        widths->valid = false;
        widths->pass_through = 0;
        widths->vertical = 0;
        widths->horizontal = 0;
        return RET_ERROR_NOT_INITIALIZED;
    }

    widths->pass_through =
        g_polarizer_wheel_instance.calibration.bump_width_pass_through;
    widths->vertical =
        g_polarizer_wheel_instance.calibration.bump_width_vertical;
    widths->horizontal =
        g_polarizer_wheel_instance.calibration.bump_width_horizontal;
    widths->valid = true;

    return RET_SUCCESS;
}

#if CONFIG_ZTEST

ret_code_t
polarizer_wheel_get_encoder_state(int *state)
{
    if (state == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    /* Save current encoder enable state */
    int initial_enable_state = gpio_pin_get_dt(&polarizer_encoder_enable_spec);

    /* Enable encoder if not already enabled to get a valid reading */
    int ret = gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                                    GPIO_OUTPUT_ACTIVE);
    if (ret) {
        return RET_ERROR_INTERNAL;
    }

    /* Small delay to allow encoder to stabilize */
    k_usleep(100);

    *state = gpio_pin_get_dt(&polarizer_encoder_spec);

    /* Restore encoder enable to initial state */
    if (initial_enable_state == 0) {
        gpio_pin_configure_dt(&polarizer_encoder_enable_spec,
                              GPIO_OUTPUT_INACTIVE);
    }

    return RET_SUCCESS;
}

#endif
