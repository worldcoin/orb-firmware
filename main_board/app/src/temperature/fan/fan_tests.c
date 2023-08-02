#include "fan.h"
#include "fan_tach.h"
#include "system/version/version.h"
#include <zephyr/ztest.h>

#include "logs_can.h"
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
    uint32_t fan_aux_speed = fan_tach_get_aux_speed();
    uint32_t fan_main_speed = fan_tach_get_main_speed();

    // check that either one or the other fan is spinning
    zassert_true(fan_aux_speed != 0 || fan_main_speed != 0);
}
