#include "mirror.h"
#include "mirror_private.h"

#include "homing.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"

#include <app_assert.h>
#include <app_config.h>
#include <math.h>
#include <stdlib.h>
#include <utils.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(mirror, CONFIG_MIRROR_LOG_LEVEL);

K_THREAD_STACK_DEFINE(stack_area_mirror_work_queue, 2048);

static struct k_work_q mirror_work_queue;

static struct async_mirror_command {
    struct k_work_delayable work;
    int32_t angle_millidegrees;
} theta_angle_set_work_item, phi_angle_set_work_item;

// before starting auto-homing, we drive to motor in the opposite direction
// of the first end reached with stall detection, to make sure the motor is not
// close from the first end
#define AUTOHOMING_AWAY_FROM_BARRIER_STEPS ((int32_t)20000)

#define TMC5041_IC_VERSION 0x10

#define TMC5041_REG_GCONF 0x00
#define REG_INPUT         0x04

const double motors_arm_length_mm[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_ARM_LENGTH_MM,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_ARM_LENGTH_MM};

// initial values [IRUN, SGT]
const uint8_t motor_irun_sgt[MOTORS_COUNT][2] = {
    [MOTOR_THETA_ANGLE] = {0x13, 6},
    [MOTOR_PHI_ANGLE] = {0x13, 6},
};

/** Motor at Xactual = 0 steps is:
 *      - looking upwards, steps increase when going down
 *      - looking inwards / to the right, steps increase going left.
 *  Meaning center (`steps_at_center_position`) is number of microsteps to
 *  go to center given the zero position
 */
static motors_refs_t motors_refs[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] =
        {
            .steps_at_center_position = MOTOR_THETA_CENTER_FROM_END_STEPS,
            .full_stroke_steps = MOTOR_THETA_FULL_RANGE_STEPS,
        },
    [MOTOR_PHI_ANGLE] = {
        .steps_at_center_position = MOTOR_PHI_CENTER_FROM_END_STEPS,
        .full_stroke_steps = MOTOR_PHI_FULL_RANGE_STEPS,
    }};

static void
mirror_set_stepper_position(int32_t position_steps, motor_t mirror)
{
    motor_controller_spi_write(TMC5041_REGISTERS[REG_IDX_XTARGET][mirror],
                               position_steps);
}

/**
 * Set relative angle from the center position
 * @param angle_from_center_millidegrees
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @return
 */
static ret_code_t
mirror_set_angle_from_center(int32_t angle_from_center_millidegrees,
                             motor_t motor)
{
    if (motor >= MOTORS_COUNT) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (motors_refs[motor].motor_state) {
        return motors_refs[motor].motor_state;
    }

    if (motor == MOTOR_PHI_ANGLE) {
        angle_from_center_millidegrees = -angle_from_center_millidegrees;
    }

    int32_t stepper_position_from_center_microsteps =
        calculate_microsteps_from_center_position(
            angle_from_center_millidegrees, motors_arm_length_mm[motor]);

    int32_t stepper_position_absolute_microsteps =
        motors_refs[motor].steps_at_center_position +
        stepper_position_from_center_microsteps;

    motors_refs[motor].angle_millidegrees =
        angle_from_center_millidegrees + mirror_center_angles[motor];

    LOG_INF("Setting mirror %u to %d milli-degrees (%d microsteps)", motor,
            motors_refs[motor].angle_millidegrees,
            stepper_position_absolute_microsteps);

    mirror_set_stepper_position(stepper_position_absolute_microsteps, motor);

    LOG_INF("new mirror pos from center: %d °, %d microsteps",
            angle_from_center_millidegrees,
            stepper_position_from_center_microsteps);

    return RET_SUCCESS;
}

static ret_code_t
mirror_check_angle(uint32_t angle_millidegrees, motor_t motor)
{
    switch (motor) {
    case MOTOR_THETA_ANGLE:
        if (angle_millidegrees > MIRROR_ANGLE_THETA_MAX_MILLIDEGREES ||
            angle_millidegrees < MIRROR_ANGLE_THETA_MIN_MILLIDEGREES) {
            LOG_ERR("Mirror theta angle of %d out of range [%d;%d]",
                    angle_millidegrees, MIRROR_ANGLE_THETA_MIN_MILLIDEGREES,
                    MIRROR_ANGLE_THETA_MAX_MILLIDEGREES);
            return RET_ERROR_INVALID_PARAM;
        }
        return RET_SUCCESS;
    case MOTOR_PHI_ANGLE:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored                                                 \
    "-Wtype-limits" // on diamond MIRROR_ANGLE_PHI_MIN_MILLIDEGREES is 0
        if (angle_millidegrees > MIRROR_ANGLE_PHI_MAX_MILLIDEGREES ||
            angle_millidegrees < MIRROR_ANGLE_PHI_MIN_MILLIDEGREES) {
#pragma GCC diagnostic pop

            LOG_ERR("Mirror phi angle of %u out of range [%u;%u]",
                    angle_millidegrees, MIRROR_ANGLE_PHI_MIN_MILLIDEGREES,
                    MIRROR_ANGLE_PHI_MAX_MILLIDEGREES);
            return RET_ERROR_INVALID_PARAM;
        }
        return RET_SUCCESS;
    default:
        return RET_ERROR_INVALID_PARAM;
        break;
    }
}

/**
 * Set relative angle from the current position
 *
 * @param angle_millidegrees from current position
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if the mirror cannot be moved to the requested
 * position given its current position
 */
static ret_code_t
mirror_set_angle_relative(const int32_t angle_millidegrees, const motor_t motor)
{
    const int32_t stepper_position_absolute_microsteps =
        (int32_t)motor_controller_spi_read(
            TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);

    int32_t stepper_position_from_center_microsteps =
        stepper_position_absolute_microsteps -
        motors_refs[motor].steps_at_center_position;

    if (motor == MOTOR_PHI_ANGLE) {
        stepper_position_from_center_microsteps =
            -stepper_position_from_center_microsteps;
    }

    const int32_t angle_from_center_millidegrees =
        calculate_millidegrees_from_center_position(
            stepper_position_from_center_microsteps,
            motors_arm_length_mm[motor]);

    int32_t target_angle_from_center_millidegrees =
        angle_from_center_millidegrees + angle_millidegrees;

    // the math above might end up outside the available
    // mechanical range, so let's clamp to values inside the range.
    if (motor == MOTOR_PHI_ANGLE) {
        target_angle_from_center_millidegrees =
            CLAMP(target_angle_from_center_millidegrees,
                  (MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES -
                   MIRROR_ANGLE_PHI_MAX_MILLIDEGREES),
                  (MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES -
                   MIRROR_ANGLE_PHI_MIN_MILLIDEGREES));
    } else if (motor == MOTOR_THETA_ANGLE) {
        target_angle_from_center_millidegrees =
            CLAMP(target_angle_from_center_millidegrees,
                  (MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES -
                   MIRROR_ANGLE_THETA_MAX_MILLIDEGREES),
                  (MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES -
                   MIRROR_ANGLE_THETA_MIN_MILLIDEGREES));
    }

    LOG_DBG(
        "Set relative angle: old_pos_microsteps = %d, from_center_microsteps = "
        "%d, angle_from_center = %d, target_angle_from_center = %d",
        stepper_position_absolute_microsteps,
        stepper_position_from_center_microsteps, angle_from_center_millidegrees,
        target_angle_from_center_millidegrees);

    return mirror_set_angle_from_center(target_angle_from_center_millidegrees,
                                        motor);
}

ret_code_t
mirror_set_angle_phi_relative(int32_t angle_millidegrees)
{
    return mirror_set_angle_relative(angle_millidegrees, MOTOR_PHI_ANGLE);
}

ret_code_t
mirror_set_angle_theta_relative(int32_t angle_millidegrees)
{
    return mirror_set_angle_relative(angle_millidegrees, MOTOR_THETA_ANGLE);
}

static ret_code_t
mirror_set_angle(uint32_t angle_millidegrees, motor_t motor)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, motor);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    int32_t angle_from_center_millidegrees =
        (int32_t)angle_millidegrees - mirror_center_angles[motor];

    return mirror_set_angle_from_center(angle_from_center_millidegrees, motor);
}

ret_code_t
mirror_set_angle_phi(uint32_t angle_millidegrees)
{
    return mirror_set_angle(angle_millidegrees, MOTOR_PHI_ANGLE);
}

ret_code_t
mirror_set_angle_theta(uint32_t angle_millidegrees)
{
    return mirror_set_angle(angle_millidegrees, MOTOR_THETA_ANGLE);
}

static void
mirror_angle_theta_work_wrapper(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    int ret =
        mirror_set_angle_theta(theta_angle_set_work_item.angle_millidegrees);
    ASSERT_SOFT(ret);
}

static void
mirror_angle_phi_work_wrapper(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    int ret = mirror_set_angle_phi(phi_angle_set_work_item.angle_millidegrees);
    ASSERT_SOFT(ret);
}

ret_code_t
mirror_set_angle_phi_async(int32_t angle_millidegrees, uint32_t delay_ms)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, MOTOR_PHI_ANGLE);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    if (k_work_delayable_busy_get(&phi_angle_set_work_item.work)) {
        LOG_ERR("async: Mirror phi set work item is busy!");
        return RET_ERROR_BUSY;
    }

    phi_angle_set_work_item.angle_millidegrees = angle_millidegrees;
    k_work_schedule_for_queue(&mirror_work_queue, &phi_angle_set_work_item.work,
                              K_MSEC(delay_ms));
    return RET_SUCCESS;
}

ret_code_t
mirror_set_angle_theta_async(int32_t angle_millidegrees, uint32_t delay_ms)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, MOTOR_THETA_ANGLE);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    if (k_work_delayable_busy_get(&theta_angle_set_work_item.work)) {
        LOG_ERR("async: Mirror theta set work item is busy!");
        return RET_ERROR_BUSY;
    }

    theta_angle_set_work_item.angle_millidegrees = angle_millidegrees;
    k_work_schedule_for_queue(
        &mirror_work_queue, &theta_angle_set_work_item.work, K_MSEC(delay_ms));

    return RET_SUCCESS;
}

bool
mirror_homed_successfully(void)
{
    return motors_refs[MOTOR_PHI_ANGLE].motor_state == RET_SUCCESS &&
           motors_refs[MOTOR_THETA_ANGLE].motor_state == RET_SUCCESS;
}

uint32_t
mirror_get_phi_angle_millidegrees(void)
{
    return motors_refs[MOTOR_PHI_ANGLE].angle_millidegrees;
}

uint32_t
mirror_get_theta_angle_millidegrees(void)
{
    return motors_refs[MOTOR_THETA_ANGLE].angle_millidegrees;
}

ret_code_t
mirror_autohoming(const motor_t *motor)
{
#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (motor == NULL || *motor >= MOTORS_COUNT) {
        return RET_ERROR_INVALID_PARAM;
    }

    return mirror_homing_one_end(&motors_refs[*motor], *motor);
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    UNUSED_PARAMETER(motor);

    return mirror_homing_overreach_ends_async(motors_refs);
#endif
}

ret_code_t
mirror_go_home(void)
{
    int ret = RET_SUCCESS;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    /* home is center */
    ret |= mirror_set_angle(MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES,
                            MOTOR_THETA_ANGLE);
    ret |=
        mirror_set_angle(MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES, MOTOR_PHI_ANGLE);
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    /* home if flat */
    ret |=
        mirror_set_angle_theta_async(MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES, 0);
    ret |= mirror_set_angle_phi_async(0, 500);
#endif
    return ret;
}

ret_code_t
mirror_init(void)
{
    uint32_t read_value;

    if (!motor_spi_ready()) {
        LOG_ERR("motion controller SPI device not ready");
        return RET_ERROR_INVALID_STATE;
    } else {
        LOG_INF("Motion controller SPI ready");
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    // write TMC5041_REG_GCONF to 0x300 to invert motor direction (bit 8 & 9)
    motor_controller_spi_write(TMC5041_REG_GCONF, 0x300);
#endif

    read_value = motor_controller_spi_read(TMC5041_REG_GCONF);
    LOG_INF("GCONF: 0x%08x", read_value);
    k_msleep(10);

    read_value = motor_controller_spi_read(REG_INPUT);
    LOG_INF("Input: 0x%08x", read_value);
    uint8_t ic_version = (read_value >> 24 & 0xFF);

    if (ic_version != TMC5041_IC_VERSION) {
        LOG_ERR("Error reading TMC5041");
        return RET_ERROR_OFFLINE;
    }

    // Set motor in positioning mode
    motor_controller_spi_send_commands(
        position_mode_full_speed[MOTOR_PHI_ANGLE],
        ARRAY_SIZE(position_mode_full_speed[MOTOR_PHI_ANGLE]));
    motor_controller_spi_send_commands(
        position_mode_full_speed[MOTOR_THETA_ANGLE],
        ARRAY_SIZE(position_mode_full_speed[MOTOR_THETA_ANGLE]));

    ret_code_t err_code = mirror_homing_overreach_ends_async(motors_refs);
    if (err_code) {
        LOG_ERR("Error homing: %u", err_code);
        return RET_ERROR_INVALID_STATE;
    }

    k_work_init_delayable(&theta_angle_set_work_item.work,
                          mirror_angle_theta_work_wrapper);
    k_work_init_delayable(&phi_angle_set_work_item.work,
                          mirror_angle_phi_work_wrapper);

    k_work_queue_init(&mirror_work_queue);
    const struct k_work_queue_config config = {
        .name = "mirror_work_queue", .no_yield = false, .essential = false};
    k_work_queue_start(&mirror_work_queue, stack_area_mirror_work_queue,
                       K_THREAD_STACK_SIZEOF(stack_area_mirror_work_queue),
                       THREAD_PRIORITY_MIRROR_INIT, &config);

    return RET_SUCCESS;
}
