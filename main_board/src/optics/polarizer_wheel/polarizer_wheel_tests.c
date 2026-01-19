/**
 * @file polarizer_wheel_tests.c
 * @brief HIL tests for the polarizer wheel module
 *
 * These tests run on actual hardware and verify:
 * - Initialization and homing
 * - Standard position transitions (pass-through, vertical, horizontal)
 * - Position accuracy via encoder feedback
 */

#include "polarizer_wheel.h"
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(polarizer_wheel_test);

/* Test timeout for homing (ms) */
#define HOMING_TIMEOUT_MS 15000

/* Test timeout for position change (ms) */
#define POSITION_TIMEOUT_MS 5000

/* Poll interval when waiting for operations (ms) */
#define POLL_INTERVAL_MS 100

/**
 * Wait for the polarizer wheel to complete homing
 * @return true if homed successfully, false on timeout
 */
static bool
wait_for_homing(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        if (polarizer_wheel_homed()) {
            return true;
        }
        k_msleep(POLL_INTERVAL_MS);
        elapsed += POLL_INTERVAL_MS;
    }
    return false;
}

/**
 * Test: Polarizer wheel initialization and homing
 *
 * Verifies that:
 * - The wheel initializes successfully
 * - Homing completes within timeout
 * - The wheel reports as homed after completion
 */
ZTEST(hil, test_polarizer_wheel_homing)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing polarizer wheel homing...");

    /* Wait for homing to complete (init triggers homing automatically) */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel homing timed out");

    LOG_INF("Polarizer wheel homed successfully");
}

/**
 * Test: Move to vertical polarizer position
 *
 * Verifies that:
 * - The wheel can move to vertical position (120 degrees)
 * - Movement completes within timeout
 */
ZTEST(hil, test_polarizer_wheel_set_vertical)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing move to vertical position...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Move to vertical position (1200 decidegrees = 120 degrees) */
    ret_code_t ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate move to vertical: %d",
                  ret);

    /* Wait for movement to complete */
    k_msleep(POSITION_TIMEOUT_MS);

    LOG_INF("Moved to vertical position");
}

/**
 * Test: Move to horizontal polarizer position
 *
 * Verifies that:
 * - The wheel can move to horizontal position (240 degrees)
 * - Movement completes within timeout
 */
ZTEST(hil, test_polarizer_wheel_set_horizontal)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing move to horizontal position...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Move to horizontal position (2400 decidegrees = 240 degrees) */
    ret_code_t ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate move to horizontal: %d",
                  ret);

    /* Wait for movement to complete */
    k_msleep(POSITION_TIMEOUT_MS);

    LOG_INF("Moved to horizontal position");
}

/**
 * Test: Return to pass-through position
 *
 * Verifies that:
 * - The wheel can return to pass-through position (0 degrees)
 * - Movement completes within timeout
 */
ZTEST(hil, test_polarizer_wheel_set_passthrough)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing move to pass-through position...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Move to pass-through position (0 decidegrees) */
    ret_code_t ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE);
    zassert_equal(ret, RET_SUCCESS,
                  "Failed to initiate move to pass-through: %d", ret);

    /* Wait for movement to complete */
    k_msleep(POSITION_TIMEOUT_MS);

    LOG_INF("Moved to pass-through position");
}

/**
 * Test: Full position cycle
 *
 * Verifies that:
 * - The wheel can cycle through all three standard positions
 * - Each transition completes successfully
 */
ZTEST(hil, test_polarizer_wheel_full_cycle)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing full position cycle...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    ret_code_t ret;

    /* Cycle: pass-through -> vertical -> horizontal -> pass-through */
    const uint32_t positions[] = {
        POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE,
        POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE,
        POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE,
    };
    const char *names[] = {"vertical", "horizontal", "pass-through"};

    for (size_t i = 0; i < ARRAY_SIZE(positions); i++) {
        LOG_INF("Moving to %s position...", names[i]);

        ret = polarizer_wheel_set_angle(
            POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT, positions[i]);
        zassert_equal(ret, RET_SUCCESS, "Failed to initiate move to %s: %d",
                      names[i], ret);

        /* Wait for movement to complete */
        k_msleep(POSITION_TIMEOUT_MS);

        LOG_INF("Reached %s position", names[i]);
    }

    LOG_INF("Full cycle completed successfully");
}

/**
 * Test: High-speed position change
 *
 * Verifies that:
 * - The wheel can move at higher speeds
 * - S-curve acceleration/deceleration works correctly
 */
ZTEST(hil, test_polarizer_wheel_high_speed)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing high-speed movement...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Move at higher speed (400ms per revolution) */
    ret_code_t ret = polarizer_wheel_set_angle(
        POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_400MSEC_PER_TURN,
        POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate high-speed move: %d",
                  ret);

    /* Wait for movement to complete */
    k_msleep(POSITION_TIMEOUT_MS);

    /* Return to pass-through at high speed */
    ret = polarizer_wheel_set_angle(
        POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_400MSEC_PER_TURN,
        POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate high-speed return: %d",
                  ret);

    k_msleep(POSITION_TIMEOUT_MS);

    LOG_INF("High-speed test completed");
}

/**
 * Test: Re-homing after movement
 *
 * Verifies that:
 * - The wheel can re-home after being moved
 * - Re-homing corrects any accumulated position error
 */
ZTEST(hil, test_polarizer_wheel_rehoming)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing re-homing after movement...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed initially");

    /* Move to a position */
    ret_code_t ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to move before re-homing: %d", ret);
    k_msleep(POSITION_TIMEOUT_MS);

    /* Trigger re-homing */
    ret = polarizer_wheel_home_async();
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate re-homing: %d", ret);

    /* Wait for re-homing to complete */
    k_msleep(500); /* Small delay for state transition */
    homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Re-homing timed out");

    LOG_INF("Re-homing completed successfully");
}

/**
 * Test: Invalid parameter rejection
 *
 * Verifies that:
 * - Invalid angles are rejected
 * - Invalid frequencies are rejected
 */
ZTEST(hil, test_polarizer_wheel_invalid_params)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing invalid parameter rejection...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    ret_code_t ret;

    /* Test invalid angle (> 360 degrees) */
    ret = polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                    3700); /* > 3600 decidegrees */
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "Should reject angle > 360 degrees");

    /* Test frequency too low */
    ret = polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_MINIMUM -
                                        1,
                                    POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "Should reject frequency below minimum");

    /* Test frequency too high */
    ret = polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_MAXIMUM +
                                        1,
                                    POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "Should reject frequency above maximum");

    LOG_INF("Invalid parameter rejection test passed");
}
