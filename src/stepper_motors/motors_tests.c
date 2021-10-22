//
// Created by Cyril on 22/10/2021.
//

#include "motors_tests.h"
#include "stepper_motors.h"
#include <zephyr.h>

#include <logging/log.h>
#include <random/rand32.h>
#include <app_config.h>
LOG_MODULE_REGISTER(motors_test);

K_THREAD_STACK_DEFINE(motors_test_thread_stack, 1024);
static struct k_thread test_thread_data;

/// This function allows the test of the full CAN bus data pipe using two boards
/// Below, we test the TX thread while a remote Orb will receive data in its RX thread
/// \return never
static _Noreturn void
test_routine()
{
    while(1)
    {
        k_msleep(10000);

        int8_t random_angle = ((int32_t) sys_rand32_get())  % 100;

        motors_angle_horizontal((int8_t) random_angle);
    }
}

void
motors_tests_init(void)
{
    k_tid_t tid = k_thread_create(&test_thread_data, motors_test_thread_stack,
                                  K_THREAD_STACK_SIZEOF(motors_test_thread_stack),
                                  test_routine, NULL, NULL, NULL,
                                  THREAD_PRIORITY_TESTS, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning test_thread thread");
    }
}

