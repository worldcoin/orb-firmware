#include "mirror.h"
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(mirror_test);

ZTEST(hil, test_motors_ah_past_the_end)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_MIRROR);

    ret_code_t err_code;
    bool ah_progress;
    bool ah_success;

    // wait for motors to initialize themselves
    k_msleep(2000);

#if defined(CONFIG_BOARD_PEARL_MAIN)
    // on pearl, each axis homed individually
    motor_t angle = MOTOR_PHI_ANGLE;
    err_code = mirror_autohoming(&angle);
    zassert_equal(err_code, RET_SUCCESS);

    angle = MOTOR_THETA_ANGLE;
    err_code = mirror_autohoming(&angle);
    zassert_equal(err_code, RET_SUCCESS);
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    err_code = mirror_autohoming(NULL);
    zassert_equal(err_code, RET_SUCCESS);
#endif

    ah_progress = mirror_auto_homing_in_progress();
    zassert_true(ah_progress);

    // wait for completion within 10 seconds
    int count = 0;
    while (ah_progress && count < 10) {
        ah_progress = mirror_auto_homing_in_progress();
        k_msleep(1000);
    }

    zassert_false(ah_progress);

    ah_success = mirror_homed_successfully();
    zassert_true(ah_success);

    // set to random position before restarting auto-homing
    int angle_theta = (rand() % MIRROR_ANGLE_THETA_RANGE_MILLIDEGREES) +
                      MIRROR_ANGLE_THETA_MIN_MILLIDEGREES;
    int angle_phi = (rand() % MIRROR_ANGLE_PHI_RANGE_MILLIDEGREES) +
                    MIRROR_ANGLE_PHI_MIN_MILLIDEGREES;

    err_code = mirror_set_angle_theta(angle_theta);
    zassert_equal(err_code, RET_SUCCESS);
    err_code = mirror_set_angle_phi(angle_phi);
    zassert_equal(err_code, RET_SUCCESS);
}

/* Disable auto-homing test with stall detection as we don't use it */
#if AUTO_HOMING_ENABLED
ZTEST(hardware, test_motors_ah_stall_detection)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_MIRROR);

    ret_code_t err_code;
    struct k_thread *horiz = NULL, *vert = NULL;
    bool ah_progress;
    bool ah_success;

    horiz = NULL;
    vert = NULL;
    err_code =
        mirror_auto_homing_stall_detection(MIRROR_VERTICAL_ANGLE, &horiz);
    zassert_equal(err_code, RET_SUCCESS);
    err_code =
        mirror_auto_homing_stall_detection(MIRROR_HORIZONTAL_ANGLE, &vert);
    zassert_equal(err_code, RET_SUCCESS);

    ah_progress = mirror_auto_homing_in_progress();
    zassert_true(ah_progress);

    // wait for completion
    zassert_not_null(horiz);
    k_thread_join(horiz, K_FOREVER);
    zassert_not_null(vert);
    k_thread_join(vert, K_FOREVER);

    ah_progress = mirror_auto_homing_in_progress();
    zassert_false(ah_progress);

    ah_success = mirror_homed_successfully();
    zassert_true(ah_success);
}
#endif
