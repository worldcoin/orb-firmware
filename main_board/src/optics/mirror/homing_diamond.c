#include "homing.h"

#include <app_assert.h>
#include <app_config.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(mirror, CONFIG_MIRROR_LOG_LEVEL);

#define THREAD_STACK_SIZE_MOTOR_INIT 2048

K_THREAD_STACK_DEFINE(stack_area_motor_init, THREAD_STACK_SIZE_MOTOR_INIT);
static struct k_thread thread_data_mirror_homing;
static struct k_sem homing_in_progress_sem;

// 1 turn = 360º / 18º * 256 µ-steps
// values below in micro-steps
#define MOTOR_THETA_CENTER_FROM_END_STEPS 63000
#define MOTOR_PHI_CENTER_FROM_FLAT_END_STEPS                                   \
    (30.15 * (360 / 18) * 256) // 30.15 turns
#define MOTOR_PHI_CENTER_FROM_END_STEPS                                        \
    (16.325 * (360 / 18) * 256) // 16.325 turns

#define MOTOR_THETA_FULL_RANGE_STEPS 140000
#define MOTOR_PHI_FULL_RANGE_STEPS   102400

static const uint32_t motors_center_from_end_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_CENTER_FROM_END_STEPS,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_CENTER_FROM_END_STEPS};
static const uint32_t motors_full_range_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_FULL_RANGE_STEPS,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_FULL_RANGE_STEPS};

void
mirror_auto_homing_one_end_thread(void *p1, void *p2, void *p3)
{
    motors_refs_t *motors = p1;
    UNUSED(p2);
    UNUSED(p3);
}

bool
mirror_auto_homing_in_progress()
{
    return k_sem_count_get(&homing_in_progress_sem) == 0;
}

ret_code_t
mirror_homing_async(motors_refs_t motors[MOTORS_COUNT])
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
                    (k_thread_entry_t)mirror_auto_homing_one_end_thread,
                    (void *)motors, NULL, NULL, THREAD_PRIORITY_MOTORS_INIT, 0,
                    delay);
    k_thread_name_set(&thread_data_mirror_homing, "mirror_homing");

    return RET_SUCCESS;
}
