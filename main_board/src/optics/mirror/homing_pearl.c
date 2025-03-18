#include "homing.h"
#include "mirror_private.h"

#include "mcu.pb.h"
#include "mirror.h"

#include <app_assert.h>
#include <app_config.h>
#include <pubsub/pubsub.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(mirror, CONFIG_MIRROR_LOG_LEVEL);

K_THREAD_STACK_DEFINE(stack_area_motor_phi_init, THREAD_STACK_SIZE_MOTORS_INIT);
K_THREAD_STACK_DEFINE(stack_area_motor_theta_init,
                      THREAD_STACK_SIZE_MOTORS_INIT);

static struct k_thread thread_data_mirror_homing[MOTORS_COUNT];
static struct k_sem homing_in_progress_sem[MOTORS_COUNT];

static const uint32_t motors_center_from_end_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_CENTER_FROM_END_STEPS,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_CENTER_FROM_END_STEPS};
static const uint32_t motors_full_range_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_FULL_RANGE_STEPS,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_FULL_RANGE_STEPS};

// a bit more than mechanical range
const uint32_t motors_full_course_maximum_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = (500 * 256), [MOTOR_PHI_ANGLE] = (700 * 256)};

// To get motor driver status, we need to poll its register (interrupt pins not
// connected)
// Below are timing definitions
#define AUTOHOMING_POLL_DELAY_MS 30
#define AUTOHOMING_TIMEOUT_MS    7000
#define AUTOHOMING_TIMEOUT_LOOP_COUNT                                          \
    (AUTOHOMING_TIMEOUT_MS / AUTOHOMING_POLL_DELAY_MS)

void
mirror_auto_homing_one_end_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    motors_refs_t *motor_handle = p1;
    uint32_t motor = (uint32_t)p2;
    uint32_t status = 0;
    int32_t timeout = AUTOHOMING_TIMEOUT_LOOP_COUNT;

    motor_handle->auto_homing_state = AH_UNINIT;
    while (motor_handle->auto_homing_state != AH_SUCCESS && timeout) {
        status = motor_controller_spi_read(
            TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);

        LOG_DBG("Status %d 0x%08x, state %u", motor, status,
                motor_handle->auto_homing_state);

        switch (motor_handle->auto_homing_state) {
        case AH_UNINIT: {
            // write xactual = 0
            motor_controller_spi_write(
                TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);

            motor_controller_spi_send_commands(
                position_mode_full_speed[motor],
                ARRAY_SIZE(position_mode_full_speed[motor]));
            int32_t steps = -motors_full_course_maximum_steps[motor];
            LOG_INF("Steps to one end: %i", steps);
            motor_controller_spi_write(
                TMC5041_REGISTERS[REG_IDX_XTARGET][motor], steps);
            motor_handle->auto_homing_state = AH_GO_HOME;
        } break;

        case AH_GO_HOME: {
            if (status & MOTOR_DRV_STATUS_STANDSTILL) {
                // write xactual = 0
                motor_controller_spi_write(
                    TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);

                motor_handle->steps_at_center_position =
                    (int32_t)motors_center_from_end_steps[motor];
                motor_handle->full_course = motors_full_range_steps[motor];

                // go to middle position
                motor_controller_spi_write(
                    TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                    motor_handle->steps_at_center_position);

                motor_handle->auto_homing_state = AH_WAIT_STANDSTILL;
            }
        } break;
        case AH_WAIT_STANDSTILL: {
            if (status & MOTOR_DRV_STATUS_STANDSTILL) {
                uint32_t angle_range_millidegrees =
                    2 *
                    calculate_millidegrees_from_center_position(
                        motor_handle->full_course / 2,
                        (motor == MOTOR_THETA_ANGLE ? MOTOR_THETA_ARM_LENGTH_MM
                                                    : MOTOR_PHI_ARM_LENGTH_MM));
                LOG_INF("Motor %u, x0: center: %d microsteps, range: %u "
                        "millidegrees",
                        motor, motor_handle->steps_at_center_position,
                        angle_range_millidegrees);

                orb_mcu_main_MotorRange range = {
                    .which_motor =
                        (motor == MOTOR_THETA_ANGLE
                             ? orb_mcu_main_MotorRange_Motor_VERTICAL_THETA
                             : orb_mcu_main_MotorRange_Motor_HORIZONTAL_PHI),
                    .range_microsteps = motor_handle->full_course,
                    .range_millidegrees = angle_range_millidegrees};
                publish_new(&range, sizeof(range),
                            orb_mcu_main_McuToJetson_motor_range_tag,
                            CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

                motor_handle->auto_homing_state = AH_SUCCESS;
            }
        } break;
        case AH_SUCCESS:
            break;
        }

        --timeout;
        k_msleep(AUTOHOMING_POLL_DELAY_MS);
    }

    // in any case, we want the motor to be in positioning mode
    motor_controller_spi_send_commands(
        position_mode_full_speed[motor],
        ARRAY_SIZE(position_mode_full_speed[motor]));

    // keep auto-homing state
    if (timeout == 0) {
        motor_handle->motor_state = RET_ERROR_INVALID_STATE;
    } else {
        motor_handle->motor_state = RET_SUCCESS;
    }

    if (motor == MOTOR_THETA_ANGLE) {
        motor_handle->angle_millidegrees =
            MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES;
    } else {
        motor_handle->angle_millidegrees = MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES;
    }

    k_sem_give(&homing_in_progress_sem[motor]);
}

ret_code_t
mirror_homing_one_end(const motors_refs_t *motor_handle, const motor_t motor_id)
{
    if (motor_handle == NULL || motor_id >= MOTORS_COUNT) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (k_sem_take(&homing_in_progress_sem[motor_id], K_NO_WAIT) == -EBUSY) {
        LOG_WRN("Motor %u auto-homing already in progress", motor_id);
        return RET_ERROR_BUSY;
    }

    // On a cold POR/boot of the Orb; going from a disconnected or
    // discharged battery to connected/charged, it takes time for the power
    // rails to come up and stabilize for the LM25117 buck controller and
    // subsequently the TMC5041 stepper motor driver to be able to act on
    // SPI commands
    const k_timeout_t delay =
        (motor_handle->motor_state == RET_ERROR_NOT_INITIALIZED
             ? K_MSEC(2000 * (motor_id + 1))
             : K_NO_WAIT);
    switch (motor_id) {
    case MOTOR_THETA_ANGLE:
        k_thread_create(&thread_data_mirror_homing[motor_id],
                        stack_area_motor_theta_init,
                        K_THREAD_STACK_SIZEOF(stack_area_motor_theta_init),
                        (k_thread_entry_t)mirror_auto_homing_one_end_thread,
                        (void *)motor_handle, (void *)motor_id, NULL,
                        THREAD_PRIORITY_MOTORS_INIT, 0, delay);
        k_thread_name_set(&thread_data_mirror_homing[motor_id],
                          "motor_ah_theta_one_end");
        break;
    case MOTOR_PHI_ANGLE:
        k_thread_create(&thread_data_mirror_homing[motor_id],
                        stack_area_motor_phi_init,
                        K_THREAD_STACK_SIZEOF(stack_area_motor_phi_init),
                        (k_thread_entry_t)mirror_auto_homing_one_end_thread,
                        (void *)motor_handle, (void *)motor_id, NULL,
                        THREAD_PRIORITY_MOTORS_INIT, 0, delay);
        k_thread_name_set(&thread_data_mirror_homing[motor_id],
                          "motor_ah_phi_one_end");
        break;
    default:
        // shouldn't happen, given the check above
        return RET_ERROR_INVALID_PARAM;
    }

    return RET_SUCCESS;
}

bool
mirror_auto_homing_in_progress()
{
    return (k_sem_count_get(&homing_in_progress_sem[MOTOR_THETA_ANGLE]) == 0 ||
            k_sem_count_get(&homing_in_progress_sem[MOTOR_PHI_ANGLE]) == 0);
}

ret_code_t
mirror_homing_async(motors_refs_t motors[MOTORS_COUNT])
{
    static bool is_init = false;
    if (!is_init) {
        int err_code =
            k_sem_init(&homing_in_progress_sem[MOTOR_PHI_ANGLE], 1, 1);
        if (err_code) {
            ASSERT_SOFT(err_code);
            return RET_ERROR_INTERNAL;
        }

        err_code = k_sem_init(&homing_in_progress_sem[MOTOR_THETA_ANGLE], 1, 1);
        if (err_code) {
            ASSERT_SOFT(err_code);
            return RET_ERROR_INTERNAL;
        }
        is_init = true;
    }

    for (int i = 0; i < MOTORS_COUNT; i++) {
        motors[i].motor_state = RET_ERROR_NOT_INITIALIZED;
        int err_code = mirror_homing_one_end(&motors[i], i);
        if (err_code) {
            LOG_ERR("homing motor %u failed", i);
            return RET_ERROR_INTERNAL;
        }
    }

    return RET_SUCCESS;
}
