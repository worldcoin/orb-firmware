#include "mirror.h"
#include "mirror_private.h"

#include "homing.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"
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
    struct k_work work;
    int32_t angle_millidegrees;
} theta_angle_set_work_item, phi_angle_set_work_item;

// before starting auto-homing, we drive to motor in the opposite direction
// of the first end reached with stall detection, to make sure the motor is not
// close from the first end
#define AUTOHOMING_AWAY_FROM_BARRIER_STEPS ((int32_t)20000)

#define TMC5041_IC_VERSION 0x10

#define TMC5041_REG_GCONF 0x00
#define REG_INPUT         0x04

const float motors_arm_length_mm[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_ARM_LENGTH_MM,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_ARM_LENGTH_MM};

// initial values [IRUN, SGT]
const uint8_t motor_irun_sgt[MOTORS_COUNT][2] = {
    [MOTOR_THETA_ANGLE] = {0x13, 6},
    [MOTOR_PHI_ANGLE] = {0x13, 6},
};

static motors_refs_t motors_refs[MOTORS_COUNT] = {0};

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

    int32_t stepper_position_from_center_microsteps =
        calculate_microsteps_from_center_position(
            angle_from_center_millidegrees, motors_arm_length_mm[motor]);

    if (motor == MOTOR_PHI_ANGLE) {
        stepper_position_from_center_microsteps =
            -stepper_position_from_center_microsteps;
    }

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
        if (angle_millidegrees > MIRROR_ANGLE_PHI_MAX_MILLIDEGREES ||
            angle_millidegrees < MIRROR_ANGLE_PHI_MIN_MILLIDEGREES) {
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

    const int32_t target_angle_from_center_millidegrees =
        angle_from_center_millidegrees + angle_millidegrees;

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
    struct async_mirror_command *cmd =
        CONTAINER_OF(item, struct async_mirror_command, work);
    mirror_set_angle_theta(cmd->angle_millidegrees);
}

static void
mirror_angle_phi_work_wrapper(struct k_work *item)
{
    struct async_mirror_command *cmd =
        CONTAINER_OF(item, struct async_mirror_command, work);
    mirror_set_angle_phi(cmd->angle_millidegrees);
}

ret_code_t
mirror_set_angle_phi_async(int32_t angle_millidegrees)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, MOTOR_PHI_ANGLE);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    if (k_work_busy_get(&phi_angle_set_work_item.work)) {
        LOG_ERR("async: Mirror phi set work item is busy!");
        return RET_ERROR_BUSY;
    }

    phi_angle_set_work_item.angle_millidegrees = angle_millidegrees;
    k_work_submit_to_queue(&mirror_work_queue, &phi_angle_set_work_item.work);

    return RET_SUCCESS;
}

ret_code_t
mirror_set_angle_theta_async(int32_t angle_millidegrees)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, MOTOR_THETA_ANGLE);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    if (k_work_busy_get(&theta_angle_set_work_item.work)) {
        LOG_ERR("async: Mirror theta set work item is busy!");
        return RET_ERROR_BUSY;
    }

    theta_angle_set_work_item.angle_millidegrees = angle_millidegrees;
    k_work_submit_to_queue(&mirror_work_queue, &theta_angle_set_work_item.work);

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

#if defined(CONFIG_BOARD_PEARL_MAIN)
ret_code_t
mirror_homing_one_axis(const motor_t motor)
{
    return mirror_homing_one_end(&motors_refs[motor], motor);
}
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
ret_code_t
mirror_homing(void)
{
    return mirror_homing_async(motors_refs);
}
#endif

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

    ret_code_t err_code = mirror_homing_async(motors_refs);
    if (err_code) {
        LOG_ERR("Error homing: %u", err_code);
        return RET_ERROR_INVALID_STATE;
    }

    k_work_init(&theta_angle_set_work_item.work,
                mirror_angle_theta_work_wrapper);
    k_work_init(&phi_angle_set_work_item.work, mirror_angle_phi_work_wrapper);

    k_work_queue_init(&mirror_work_queue);
    k_work_queue_start(&mirror_work_queue, stack_area_mirror_work_queue,
                       K_THREAD_STACK_SIZEOF(stack_area_mirror_work_queue),
                       THREAD_PRIORITY_MOTORS_INIT, NULL);

    return RET_SUCCESS;
}
