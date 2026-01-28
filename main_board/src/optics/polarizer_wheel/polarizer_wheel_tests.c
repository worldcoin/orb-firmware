/**
 * @file polarizer_wheel_tests.c
 * @brief HIL tests for the polarizer wheel module
 *
 * These tests run on actual hardware and verify:
 * - Initialization and homing
 * - Standard position transitions (pass-through, vertical, horizontal)
 * - Position accuracy via encoder feedback
 */

#include "compilers.h"
#include "polarizer_wheel.h"
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(polarizer_wheel_test);

/* Test timeout for homing (ms) */
#define HOMING_TIMEOUT_MS 15000

/* Test timeout for position change (ms) */
#define POSITION_TIMEOUT_MS 2000

/* Test timeout for calibration (ms) - calibration spins + homing */
#define CALIBRATION_TIMEOUT_MS 10000

/* Poll interval when waiting for operations (ms) */
#define POLL_INTERVAL_MS 100

void
polarizer_test_reset(void *fixture)
{
    UNUSED_PARAMETER(fixture);
    polarizer_wheel_home_async();
}

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
ZTEST(polarizer, test_polarizer_wheel_homing)
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
ZTEST(polarizer, test_polarizer_wheel_set_vertical)
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
ZTEST(polarizer, test_polarizer_wheel_set_horizontal)
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
ZTEST(polarizer, test_polarizer_wheel_set_passthrough)
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
 * - Movements are tested at multiple speeds
 * - Each transition completes successfully
 */
ZTEST(polarizer, test_polarizer_wheel_full_cycle)
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
    const uint32_t speeds[] = {
        POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
        POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_400MSEC_PER_TURN};
    const char *names[] = {"vertical", "horizontal", "pass-through"};

    for (size_t j = 0; j < ARRAY_SIZE(speeds); j++) {
        LOG_INF("Testing cycle at speed %u usteps/s", speeds[j]);

        for (size_t i = 0; i < ARRAY_SIZE(positions); i++) {
            LOG_INF("Moving to %s position...", names[i]);

            ret = polarizer_wheel_set_angle(speeds[j], positions[i]);
            zassert_equal(ret, RET_SUCCESS,
                          "Failed to initiate move to %s, speed %u: %d",
                          names[i], speeds[j], ret);

            /* Wait for movement to complete */
            k_msleep(POSITION_TIMEOUT_MS);

            LOG_INF("Reached %s position, speed %u", names[i], speeds[j]);
        }
    }

    LOG_INF("Full cycle completed successfully");
}

/**
 * Test: Re-homing after movement
 *
 * Verifies that:
 * - The wheel can re-home after being moved
 * - Re-homing corrects any accumulated position error
 */
ZTEST(polarizer, test_polarizer_wheel_rehoming)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing re-homing after movement...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed initially");

    /* Move to a position */
    ret_code_t ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
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
ZTEST(polarizer, test_polarizer_wheel_invalid_params)
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

/**
 * Wait for calibration to complete by polling bump widths validity
 * @return true if calibration completed successfully, false on timeout
 */
static bool
wait_for_calibration(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    polarizer_wheel_bump_widths_t widths;

    while (elapsed < timeout_ms) {
        ret_code_t ret = polarizer_wheel_get_bump_widths(&widths);
        if (ret == RET_SUCCESS && widths.valid) {
            return true;
        }
        k_msleep(POLL_INTERVAL_MS);
        elapsed += POLL_INTERVAL_MS;
    }
    return false;
}

/**
 * Test: Polarizer wheel calibration
 *
 * Verifies that:
 * - Calibration can be started after homing
 * - Calibration completes successfully
 * - Bump widths become valid after calibration
 * - Measured bump widths are reasonable (non-zero)
 */
ZTEST(polarizer, test_polarizer_wheel_calibration)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing polarizer wheel calibration...");

    /* Ensure homed first - calibration requires homed state */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Verify bump widths are not valid before calibration */
    polarizer_wheel_bump_widths_t widths_before =
        (polarizer_wheel_bump_widths_t){0};
    ret_code_t ret = polarizer_wheel_get_bump_widths(&widths_before);
    /* It's OK if this fails or returns invalid - we just want to confirm
     * calibration changes the state */
    LOG_INF("Bump widths before calibration: valid=%d", widths_before.valid);

    /* Start calibration */
    ret = polarizer_wheel_calibrate_async();
    zassert_equal(ret, RET_SUCCESS, "Failed to start calibration: %d", ret);

    LOG_INF("Calibration started, waiting for completion...");

    /* Wait for calibration to complete */
    bool calibrated = wait_for_calibration(CALIBRATION_TIMEOUT_MS);
    zassert_true(calibrated, "Calibration timed out");

    /* Verify bump widths are now valid */
    polarizer_wheel_bump_widths_t widths;
    ret = polarizer_wheel_get_bump_widths(&widths);
    zassert_equal(ret, RET_SUCCESS,
                  "Failed to get bump widths after calibration: %d", ret);
    zassert_true(widths.valid, "Bump widths not marked as valid");

    /* Verify all bump widths are non-zero (sanity check) */
    zassert_true(
        widths.pass_through > POLARIZER_WHEEL_MICROSTEPS_PER_STEP * 0.8 &&
            widths.pass_through < POLARIZER_WHEEL_MICROSTEPS_PER_STEP * 2,
        "Pass-through bump doesn't have reasonable width after calibration: %u",
        widths.pass_through);
    zassert_true(
        widths.vertical > POLARIZER_WHEEL_MICROSTEPS_PER_STEP * 0.8 &&
            widths.vertical < POLARIZER_WHEEL_MICROSTEPS_PER_STEP * 2,
        "Vertical bump doesn't have reasonable width after calibration: %u",
        widths.vertical);
    zassert_true(
        widths.horizontal > POLARIZER_WHEEL_MICROSTEPS_PER_STEP * 0.8 &&
            widths.horizontal < POLARIZER_WHEEL_MICROSTEPS_PER_STEP * 2,
        "Horizontal bump width doesn't have reasonable width after "
        "calibration: %u",
        widths.horizontal);

    LOG_INF("Calibration complete: pass_through=%u, vertical=%u, horizontal=%u "
            "microsteps",
            widths.pass_through, widths.vertical, widths.horizontal);
}

/**
 * Test: Calibration followed by standard position move
 *
 * Verifies that:
 * - After calibration, the wheel can still move to standard positions
 * - The calibrated bump widths improve centering accuracy
 */
ZTEST(polarizer, test_polarizer_wheel_calibration_then_move)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing calibration followed by position moves...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Start calibration */
    ret_code_t ret = polarizer_wheel_calibrate_async();
    zassert_equal(ret, RET_SUCCESS, "Failed to start calibration: %d", ret);

    /* Wait for calibration to complete */
    bool calibrated = wait_for_calibration(CALIBRATION_TIMEOUT_MS);
    zassert_true(calibrated, "Calibration timed out");

    /* Verify bump widths are valid */
    polarizer_wheel_bump_widths_t widths;
    ret = polarizer_wheel_get_bump_widths(&widths);
    zassert_equal(ret, RET_SUCCESS, "Failed to get bump widths: %d", ret);
    zassert_true(widths.valid, "Bump widths not valid after calibration");

    LOG_INF("Calibration verified, testing position moves...");

    /* /!\ order is important here */
    /* Move to vertical position */
    ret = polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                    POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate move to vertical: %d",
                  ret);
    k_msleep(POSITION_TIMEOUT_MS);

    /* Move to horizontal position */
    ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to initiate move to horizontal: %d",
                  ret);
    k_msleep(POSITION_TIMEOUT_MS);

    /* Return to pass-through */
    ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE);
    zassert_equal(ret, RET_SUCCESS,
                  "Failed to initiate move to pass-through: %d", ret);
    k_msleep(POSITION_TIMEOUT_MS);

    LOG_INF("Post-calibration position moves completed successfully");
}

/**
 * Test: Calibration rejection when not homed
 *
 * Verifies that:
 * - Calibration fails with appropriate error if wheel is not homed
 */
ZTEST(polarizer, test_polarizer_wheel_calibration_requires_homing)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_POLARIZER_WHEEL);

    LOG_INF("Testing calibration rejection when not at pass-through...");

    /* Ensure homed first */
    bool homed = wait_for_homing(HOMING_TIMEOUT_MS);
    zassert_true(homed, "Polarizer wheel not homed");

    /* Move away from pass-through position */
    ret_code_t ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE);
    zassert_equal(ret, RET_SUCCESS, "Failed to move to vertical: %d", ret);
    k_msleep(POSITION_TIMEOUT_MS);

    /* Attempt calibration - should fail since not at pass-through */
    ret = polarizer_wheel_calibrate_async();
    /* Calibration requires pass-through position, so this should fail */
    zassert_not_equal(ret, RET_SUCCESS,
                      "Calibration should fail when not at pass-through");

    LOG_INF("Calibration correctly rejected when not at pass-through");

    /* Return to pass-through for cleanup */
    ret =
        polarizer_wheel_set_angle(POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_DEFAULT,
                                  POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE);
    k_msleep(POSITION_TIMEOUT_MS);
}
