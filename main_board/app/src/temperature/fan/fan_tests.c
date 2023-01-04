#include "fan.h"
#include "system/version/version.h"
#include <zephyr/ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fan_test);

ZTEST(runtime_tests_1, fan)
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
