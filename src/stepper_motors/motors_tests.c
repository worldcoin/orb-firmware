//
// Created by Cyril on 22/10/2021.
//

#include "motors_tests.h"
#include "stepper_motors.h"
#include <zephyr.h>

#include <app_config.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(motors_test);

K_THREAD_STACK_DEFINE(motors_test_thread_stack, 1024);
static struct k_thread test_thread_data;

/// This function allows the test of the full CAN bus data pipe using two boards
/// Below, we test the TX thread while a remote Orb will receive data in its RX
/// thread \return never
static void
test_routine()
{
    int8_t angle_vertical = -20;
    int8_t angle_horizontal = 25;
    bool motor = true;
    ret_code_t err_code;

    // wait for motors to initialize themselves
    k_msleep(10000);

    while (1) {
        k_msleep(500);

        /* switch motor */
        motor = !motor;

        if (motor) {
            err_code = motors_angle_vertical(angle_vertical * 1000);
            angle_vertical++;
        } else {
            err_code = motors_angle_horizontal(angle_horizontal * 1000);
            angle_horizontal++;
        }

        switch (err_code) {
        case RET_SUCCESS: {
            /* Do nothing */
        } break;

        case RET_ERROR_NOT_INITIALIZED: {
            LOG_ERR("Motor %d not initialized", motor);
        } break;

        case RET_ERROR_INVALID_STATE: {
            LOG_ERR("Motor %d invalid state", motor);
        } break;

        case RET_ERROR_INVALID_PARAM: {
            if (angle_vertical > 20 && motor) {
                LOG_INF("Reached vertical end");
            }
            if (angle_horizontal > 65 && !motor) {
                LOG_INF("Reached horizontal end");
            }
        } break;

        default: {
            LOG_WRN("Setting motor %d angle ret: %u", motor, err_code);
        }
        }

        if (angle_vertical > 20 && angle_horizontal > 65) {
            LOG_INF("Ending motor test routine");
            return;
        }
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
