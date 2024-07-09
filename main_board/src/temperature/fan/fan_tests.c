#include "fan.h"
#include "fan_tach.h"
#include "system/version/version.h"
#include <zephyr/ztest.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(fan_test);

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
    Z_TEST_SKIP_IFDEF(CONFIG_BOARD_DIAMOND_MAIN);

    // "fast" speed, then revert to initial speed
    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT + 5);
    k_msleep(1000);
    uint32_t fan_aux_speed = fan_tach_get_aux_speed();
    uint32_t fan_main_speed = fan_tach_get_main_speed();

    // check that either one or the other fan is spinning
    // there is only one fan enabled at a time
    zassert_true(fan_aux_speed != 0 || fan_main_speed != 0);

    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT);
    k_msleep(1000);
    uint32_t fan_aux_speed_after = fan_tach_get_aux_speed();
    uint32_t fan_main_speed_after = fan_tach_get_main_speed();

    // check that speed decreases after setting it back to initial speed
    if (fan_aux_speed != 0) {
        zassert_true(fan_aux_speed_after < fan_aux_speed);
    } else if (fan_main_speed != 0) {
        zassert_true(fan_main_speed_after < fan_main_speed);
    } else {
        zassert_false(true, "No fan was spinning");
    }
}
