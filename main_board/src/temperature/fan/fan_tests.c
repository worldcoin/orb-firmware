#include "fan.h"
#include "fan_tach.h"
#include <orb_state.h>
#include <zephyr/kernel.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(fan_test, LOG_LEVEL_INF);

ORB_STATE_REGISTER(fan_tach);

void
fan_tach_self_test(void)
{
    // initial speed
    uint16_t initial_speed_value = fan_get_speed_setting();
    uint32_t main_speed = fan_tach_get_main_speed();
#if defined(CONFIG_BOARD_PEARL_MAIN)
    uint32_t aux_speed = fan_tach_get_aux_speed();
#endif
    // increase speed to double the initial value
    fan_set_speed_by_value(initial_speed_value * 2);

    k_msleep(1000);
    uint32_t main_speed_second = fan_tach_get_main_speed();
#if defined(CONFIG_BOARD_PEARL_MAIN)
    uint32_t aux_speed_second = fan_tach_get_aux_speed();
#endif

    if (main_speed != UINT32_MAX && main_speed_second != UINT32_MAX) {
        if (main_speed_second > main_speed) {
            ORB_STATE_SET_CURRENT(RET_SUCCESS);
        } else {
            ORB_STATE_SET_CURRENT(RET_ERROR_ASSERT_FAILS,
                                  "speed didn't increase");
        }
    } else if (main_speed == 0 || main_speed_second == 0) {
        ORB_STATE_SET_CURRENT(RET_ERROR_ASSERT_FAILS, "fan not running");
    }

#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (aux_speed != UINT32_MAX && aux_speed_second != UINT32_MAX) {
        if (aux_speed_second > aux_speed) {
            ORB_STATE_SET_CURRENT(RET_SUCCESS);
        } else {
            ORB_STATE_SET_CURRENT(RET_ERROR_ASSERT_FAILS,
                                  "aux speed didn't increase");
        }
    } else if (aux_speed == 0 || aux_speed_second == 0) {
        ORB_STATE_SET_CURRENT(RET_ERROR_ASSERT_FAILS, "fan not running");
    }
#endif

    // reset speed to initial value
    fan_set_speed_by_value(initial_speed_value);
}

#if defined(CONFIG_ZTEST)
#include "system/version/version.h"
#include <zephyr/ztest.h>

ZTEST(hil, test_fan_set_speed)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_FAN);

    uint32_t fan_speed_value;

    // check that value get = value set
    fan_set_speed_by_percentage(5);

    fan_speed_value = fan_get_speed_setting();
    fan_set_speed_by_value(fan_speed_value);
    zassert_equal(fan_get_speed_setting(), fan_speed_value);

    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT);
}

ZTEST(hil, test_fan_tachometer)
{
    // "fast" speed, then revert to initial speed
    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT + 5);
    k_msleep(5000);
    uint32_t fan_aux_speed = 0;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    fan_aux_speed = fan_tach_get_aux_speed();
#endif
    uint32_t fan_main_speed = fan_tach_get_main_speed();

    LOG_INF("fan aux speed: %d, fan main speed: %d", fan_aux_speed,
            fan_main_speed);

    // check that either one or the other fan is spinning
    // there is only one fan enabled at a time
    zassert_true(fan_aux_speed != 0 || fan_main_speed != 0);

    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT);
    k_msleep(5000);
    uint32_t fan_aux_speed_after = 0;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    fan_tach_get_aux_speed();
#endif
    uint32_t fan_main_speed_after = fan_tach_get_main_speed();

    LOG_INF("new measured fan aux speed: %d, fan main speed: %d",
            fan_aux_speed_after, fan_main_speed_after);

    // check that speed decreases after setting it back to initial speed
    if (fan_aux_speed != 0) {
        zassert_true(fan_aux_speed_after < fan_aux_speed);
    } else if (fan_main_speed != 0) {
        zassert_true(fan_main_speed_after < fan_main_speed);
    } else {
        zassert_false(true, "No fan was spinning");
    }
}

#endif // CONFIG_ZTEST
