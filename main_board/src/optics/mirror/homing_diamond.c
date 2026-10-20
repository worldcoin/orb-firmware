#include "homing.h"

#include "orb_logs.h"
#include <app_assert.h>
#include <app_config.h>
#include <main.pb.h>
#include <pubsub/pubsub.h>
#include <zephyr/drivers/stepper.h>
#include <zephyr/drivers/stepper/stepper_trinamic.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>

LOG_MODULE_DECLARE(mirror, CONFIG_MIRROR_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(stack_area_motor_init,
                             THREAD_STACK_SIZE_MIRROR_INIT);
static struct k_thread thread_data_mirror_homing;
static struct k_sem homing_in_progress_sem;

// To get motor driver status, we need to poll its register (interrupt pins not
// connected)
// Below are timing definitions
#define AUTOHOMING_POLL_DELAY_MS 30
#define AUTOHOMING_TIMEOUT_MS    10000
#define AUTOHOMING_TIMEOUT_LOOP_COUNT                                          \
    (AUTOHOMING_TIMEOUT_MS / AUTOHOMING_POLL_DELAY_MS)

static const struct tmc_ramp_generator_data full_speed_ramp = {
    .a1 = 32768,
    .v1 = 600000,
    .amax = 4096,
    .vmax = MOTOR_FS_VMAX,
    .dmax = 4096,
    .d1 = 32768,
    .vstop = 16,
    .iholdrun = (16 << 8) | (0 << 0) | (1 << 16),
};

static const struct tmc_ramp_generator_data homing_ramp = {
    .a1 = 32768,
    .v1 = 150000,
    .amax = 4096,
    .vmax = MOTOR_FS_VMAX / 4,
    .dmax = 4096,
    .d1 = 32768,
    .vstop = 16,
    .iholdrun = (16 << 8) | (0 << 0) | (1 << 16),
};

void
mirror_auto_homing_overreach_end_thread(void *p1, void *p2, void *p3)
{
    motors_refs_t *motors = p1;
    UNUSED(p2);
    UNUSED(p3);

    const struct device *phi_dev = mirror_get_stepper_dev(MOTOR_PHI_ANGLE);
    const struct device *theta_dev = mirror_get_stepper_dev(MOTOR_THETA_ANGLE);

    int32_t timeout = AUTOHOMING_TIMEOUT_LOOP_COUNT;
    enum mirror_homing_state auto_homing_state = AH_UNINIT;
    while (auto_homing_state != AH_SUCCESS && timeout > 0) {
        bool phi_moving = true, theta_moving = true;
        stepper_is_moving(phi_dev, &phi_moving);
        stepper_is_moving(theta_dev, &theta_moving);
        bool are_standstill = !phi_moving && !theta_moving;

        LOG_DBG("  %s, st %u, timeout: %d",
                are_standstill ? "standing" : "moving", auto_homing_state,
                timeout);

        switch (auto_homing_state) {
        case AH_UNINIT:
            // set reference position to 0
            stepper_set_reference_position(phi_dev, 0);

            // set homing ramp (reduced speed)
            tmc50xx_stepper_set_ramp(phi_dev, &homing_ramp);
            tmc50xx_stepper_set_ramp(theta_dev, &homing_ramp);

            {
                int32_t steps = -MOTOR_PHI_CENTER_FROM_FLAT_END_STEPS;
                LOG_INF("Steps to one end: %i", steps);
                stepper_move_to(phi_dev, steps);
            }
            auto_homing_state = AH_SHIFTED_SIDEWAYS;
            break;

        case AH_SHIFTED_SIDEWAYS:
            // wait motor is not moving anymore
            if (are_standstill) {
                // set reference position to 0 at end stop
                stepper_set_reference_position(phi_dev, 0);

                // in case motor hit the wall, take off from the border
                stepper_move_to(phi_dev, MOTOR_PHI_OFF_THE_WALL_STEPS);
                auto_homing_state = AH_UP_TO_WALL;
            }
            break;

        case AH_UP_TO_WALL:
            if (are_standstill) {
                stepper_set_reference_position(theta_dev, 0);
                {
                    int32_t steps = -MOTOR_THETA_FULL_RANGE_STEPS;
                    LOG_INF("Steps to one end: %i", steps);
                    stepper_move_to(theta_dev, steps);
                }
                auto_homing_state = AH_THETA_TO_CENTER;
            }
            break;

        case AH_THETA_TO_CENTER:
            if (are_standstill) {
                // set reference position to 0 at end stop
                stepper_set_reference_position(theta_dev, 0);

                // go to middle position
                stepper_move_to(
                    theta_dev,
                    motors[MOTOR_THETA_ANGLE].steps_at_center_position);

                auto_homing_state = AH_THETA_HOMED;
            }
            break;

        case AH_THETA_HOMED:
            if (are_standstill) {
                // motor found center on theta axis
                uint32_t angle_range_millidegrees =
                    2 * calculate_millidegrees_from_center_position(
                            motors[MOTOR_THETA_ANGLE].full_stroke_steps / 2,
                            MOTOR_THETA_ARM_LENGTH_MM);
                LOG_INF("Motor theta, x0: center: %d microsteps, range: %u "
                        "millidegrees",
                        motors[MOTOR_THETA_ANGLE].steps_at_center_position,
                        angle_range_millidegrees);

                orb_mcu_main_MotorRange range = {
                    .which_motor = orb_mcu_main_MotorRange_Motor_VERTICAL_THETA,
                    .range_microsteps =
                        motors[MOTOR_THETA_ANGLE].full_stroke_steps,
                    .range_millidegrees = angle_range_millidegrees};
                publish_new(&range, sizeof(range),
                            orb_mcu_main_McuToJetson_motor_range_tag,
                            CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX);

                auto_homing_state = AH_GO_HOME;
            }
            break;

        case AH_GO_HOME:
            // mirror is center on theta and not moving anymore,
            // let's go home now
            if (are_standstill) {
                LOG_INF("Go home");
                stepper_move_to(phi_dev, MOTOR_PHI_FULL_RANGE_STEPS);
                auto_homing_state = AH_WAIT_STANDSTILL;
            }
            break;

        case AH_WAIT_STANDSTILL:
            if (are_standstill) {
                // homed
                LOG_INF("Mirror is home");

                // set reference position to current position
                stepper_set_reference_position(phi_dev,
                                               MOTOR_PHI_FULL_RANGE_STEPS);

                uint32_t angle_range_millidegrees =
                    2 * calculate_millidegrees_from_center_position(
                            motors[MOTOR_PHI_ANGLE].full_stroke_steps / 2,
                            MOTOR_PHI_ARM_LENGTH_MM);
                orb_mcu_main_MotorRange range = {
                    .which_motor = orb_mcu_main_MotorRange_Motor_HORIZONTAL_PHI,
                    .range_microsteps =
                        motors[MOTOR_PHI_ANGLE].full_stroke_steps,
                    .range_millidegrees = angle_range_millidegrees};
                publish_new(&range, sizeof(range),
                            orb_mcu_main_McuToJetson_motor_range_tag,
                            CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX);

                auto_homing_state = AH_SUCCESS;
            }
            break;
        default:
            LOG_WRN("Unhandled: %d", auto_homing_state);
            break;
        }

        k_msleep(AUTOHOMING_POLL_DELAY_MS);
        --timeout;
    }

    // restore full-speed ramp for both motors
    tmc50xx_stepper_set_ramp(theta_dev, &full_speed_ramp);
    tmc50xx_stepper_set_ramp(phi_dev, &full_speed_ramp);

    // keep auto-homing state
    if (timeout == 0) {
        motors[MOTOR_THETA_ANGLE].motor_state = RET_ERROR_INVALID_STATE;
        motors[MOTOR_PHI_ANGLE].motor_state = RET_ERROR_INVALID_STATE;
    } else {
        motors[MOTOR_THETA_ANGLE].motor_state = RET_SUCCESS;
        motors[MOTOR_PHI_ANGLE].motor_state = RET_SUCCESS;
    }

    motors[MOTOR_THETA_ANGLE].angle_millidegrees =
        MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES;
    motors[MOTOR_PHI_ANGLE].angle_millidegrees = 0;

    k_sem_give(&homing_in_progress_sem);
}

bool
mirror_auto_homing_in_progress()
{
    return k_sem_count_get(&homing_in_progress_sem) == 0;
}

ret_code_t
mirror_homing_overreach_ends_async(motors_refs_t motors[MOTORS_COUNT])
{
    static bool is_init = false;
    if (!is_init) {
        int err_code = k_sem_init(&homing_in_progress_sem, 1, 1);
        if (err_code) {
            ASSERT_SOFT(err_code);
            return RET_ERROR_INTERNAL;
        }
        is_init = true;
    }

    if (k_sem_take(&homing_in_progress_sem, K_NO_WAIT) == -EBUSY) {
        LOG_WRN("Mirror homing already in progress");
        return RET_ERROR_BUSY;
    }

    // On a cold POR/boot of the Orb; going from a disconnected or
    // discharged battery to connected/charged, it takes time for the power
    // rails to come up and stabilize for the LM25117 buck controller and
    // subsequently the TMC5041 stepper motor driver to be able to act on
    // SPI commands
    const k_timeout_t delay =
        motors[MOTOR_PHI_ANGLE].motor_state == RET_ERROR_NOT_INITIALIZED ||
                motors[MOTOR_THETA_ANGLE].motor_state ==
                    RET_ERROR_NOT_INITIALIZED
            ? K_MSEC(2000)
            : K_NO_WAIT;

    k_thread_create(&thread_data_mirror_homing, stack_area_motor_init,
                    K_THREAD_STACK_SIZEOF(stack_area_motor_init),
                    (k_thread_entry_t)mirror_auto_homing_overreach_end_thread,
                    (void *)motors, NULL, NULL, THREAD_PRIORITY_MIRROR_INIT, 0,
                    delay);
    k_thread_name_set(&thread_data_mirror_homing, "mirror_homing");

    return RET_SUCCESS;
}
