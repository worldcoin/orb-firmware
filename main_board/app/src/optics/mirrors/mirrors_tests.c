#include "mirrors_tests.h"
#include "mirrors.h"
#include <app_config.h>
#include <zephyr/kernel.h>
#include <zephyr/random/rand32.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mirrors_test);

static K_THREAD_STACK_DEFINE(mirrors_test_thread_stack, 1024);
static struct k_thread test_thread_data;

static void
test_routine()
{
    // wait for motors to initialize themselves
    k_msleep(15000);

    while (1) {
        mirrors_auto_homing_one_end(MIRROR_HORIZONTAL_ANGLE, NULL);
        mirrors_auto_homing_one_end(MIRROR_VERTICAL_ANGLE, NULL);

        k_msleep(10000);

        // set to random position before restarting auto-homing
        int angle_vertical = (sys_rand32_get() % 40000) - 20000;
        int angle_horizontal = (sys_rand32_get() % 40000) + 25000;
        mirrors_angle_vertical(angle_vertical);
        mirrors_angle_horizontal(angle_horizontal);
        k_msleep(1000);

        struct k_thread *horiz = NULL, *vert = NULL;
        mirrors_auto_homing_stall_detection(MIRROR_VERTICAL_ANGLE, &horiz);
        mirrors_auto_homing_stall_detection(MIRROR_HORIZONTAL_ANGLE, &vert);
        k_msleep(10000);
    }
}

void
mirrors_tests_init(void)
{
    k_tid_t tid = k_thread_create(
        &test_thread_data, mirrors_test_thread_stack,
        K_THREAD_STACK_SIZEOF(mirrors_test_thread_stack), test_routine, NULL,
        NULL, NULL, THREAD_PRIORITY_TESTS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "mirrors_test");
}
