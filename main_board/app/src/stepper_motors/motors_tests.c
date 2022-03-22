#include "motors_tests.h"
#include "stepper_motors.h"
#include <zephyr.h>

#include <app_config.h>
#include <logging/log.h>
#include <random/rand32.h>
LOG_MODULE_REGISTER(motors_test);

K_THREAD_STACK_DEFINE(motors_test_thread_stack, 1024);
static struct k_thread test_thread_data;

static void
test_routine()
{
    ret_code_t err_code;

    // wait for motors to initialize themselves
    k_msleep(15000);

    while (1) {
        motors_auto_homing(MOTOR_HORIZONTAL, NULL);
        motors_auto_homing(MOTOR_VERTICAL, NULL);

        k_msleep(10000);

        // set to random position before restarting auto-homing
        int angle_vertical = (sys_rand32_get() % 40000) - 20000;
        int angle_horizontal = (sys_rand32_get() % 40000) + 25000;
        err_code = motors_angle_vertical(angle_vertical);
        err_code = motors_angle_horizontal(angle_horizontal);
        k_msleep(1000);
    }
}

void
motors_tests_init(void)
{
    k_tid_t tid = k_thread_create(
        &test_thread_data, motors_test_thread_stack,
        K_THREAD_STACK_SIZEOF(motors_test_thread_stack), test_routine, NULL,
        NULL, NULL, THREAD_PRIORITY_TESTS, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning test_thread thread");
    }
}
