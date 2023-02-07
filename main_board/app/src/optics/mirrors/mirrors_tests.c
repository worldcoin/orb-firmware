#include "mirrors.h"
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mirrors_test);

ZTEST(runtime_tests, mirrors_ah_past_the_end)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_MIRRORS);

    ret_code_t err_code;
    struct k_thread *horiz = NULL, *vert = NULL;
    bool ah_progress;
    bool ah_success;

    // wait for motors to initialize themselves
    k_msleep(2000);

    err_code = mirrors_auto_homing_one_end(MIRROR_HORIZONTAL_ANGLE, &horiz);
    zassert_equal(err_code, RET_SUCCESS);
    err_code = mirrors_auto_homing_one_end(MIRROR_VERTICAL_ANGLE, &vert);
    zassert_equal(err_code, RET_SUCCESS);

    ah_progress = mirrors_auto_homing_in_progress();
    zassert_true(ah_progress);

    // wait for completion
    zassert_not_null(horiz);
    k_thread_join(horiz, K_FOREVER);
    zassert_not_null(vert);
    k_thread_join(vert, K_FOREVER);

    ah_progress = mirrors_auto_homing_in_progress();
    zassert_false(ah_progress);

    ah_success = mirrors_homed_successfully();
    zassert_true(ah_success);

    // set to random position before restarting auto-homing
    int angle_vertical =
        (rand() % MIRRORS_ANGLE_VERTICAL_RANGE) + MIRRORS_ANGLE_VERTICAL_MIN;
    int angle_horizontal = (rand() % MIRRORS_ANGLE_HORIZONTAL_RANGE) +
                           MIRRORS_ANGLE_HORIZONTAL_MIN;

    err_code = mirrors_angle_vertical(angle_vertical);
    zassert_equal(err_code, RET_SUCCESS);
    err_code = mirrors_angle_horizontal(angle_horizontal);
    zassert_equal(err_code, RET_SUCCESS);
}

/* Disable auto-homing test with stall detection as we don't use it */
#if 0
ZTEST(runtime_tests, mirrors_ah_stall_detection)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_MIRRORS);

    ret_code_t err_code;
    struct k_thread *horiz = NULL, *vert = NULL;
    bool ah_progress;
    bool ah_success;

    horiz = NULL;
    vert = NULL;
    err_code = mirrors_auto_homing_stall_detection(MIRROR_VERTICAL_ANGLE, &horiz);
    zassert_equal(err_code, RET_SUCCESS);
    err_code = mirrors_auto_homing_stall_detection(MIRROR_HORIZONTAL_ANGLE, &vert);
    zassert_equal(err_code, RET_SUCCESS);

    ah_progress = mirrors_auto_homing_in_progress();
    zassert_true(ah_progress);

    // wait for completion
    zassert_not_null(horiz);
    k_thread_join(horiz, K_FOREVER);
    zassert_not_null(vert);
    k_thread_join(vert, K_FOREVER);

    ah_progress = mirrors_auto_homing_in_progress();
    zassert_false(ah_progress);

    ah_success = mirrors_homed_successfully();
    zassert_true(ah_success);
}
#endif
