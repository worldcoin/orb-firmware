#include "fan_tests.h"
#include "app_config.h"
#include "fan.h"
#include "version/version.h"
#include <zephyr.h>

#include <app_assert.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(fan_test);

K_THREAD_STACK_DEFINE(fan_test_thread_stack, 3000);
static struct k_thread test_thread_fan;

static void
test_fan()
{
    uint32_t fan_speed_value;

    // check that value get = value set
    fan_set_speed_by_percentage(1);
    fan_speed_value = fan_get_speed_setting();
    fan_set_speed_by_value(fan_speed_value);
    ASSERT_SOFT_BOOL(fan_get_speed_setting() == fan_speed_value);

    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT);
}

void
fan_tests_init(void)
{
    LOG_INF("Creating fan test thread");

    k_thread_create(&test_thread_fan, fan_test_thread_stack,
                    K_THREAD_STACK_SIZEOF(fan_test_thread_stack), test_fan,
                    NULL, NULL, NULL, THREAD_PRIORITY_TESTS, 0, K_NO_WAIT);
    k_thread_name_set(&test_thread_fan, "fan_test");
}
