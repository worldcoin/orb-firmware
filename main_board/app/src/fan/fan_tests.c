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
    uint32_t fan_pulse_witdh_ns;
    uint32_t max_speed_pulse_width_ns = 0;
    uint32_t min_speed_pulse_width_ns = 0;

    enum hw_version_e rev;
    version_get_hardware_rev(&rev);

    if (rev == HW_VERSION_MAINBOARD_EV1 || rev == HW_VERSION_MAINBOARD_EV2) {
        max_speed_pulse_width_ns = 32000;

        // 655 (1% of 65535) *40000 (period) *0.8 (range) / 65535 = 319
        min_speed_pulse_width_ns = 319;
    } else if (rev == HW_VERSION_MAINBOARD_EV3) {
        max_speed_pulse_width_ns = 40000;

        // min is 40% duty cycle = 0.4*40000
        // + 239 (1% of available range of 60%)
        min_speed_pulse_width_ns = 16239;
    } else {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return;
    }

    fan_set_speed_by_percentage(100);
    fan_pulse_witdh_ns = fan_get_speed_setting();
    LOG_INF("100%% => %uns", fan_pulse_witdh_ns);
    ASSERT_SOFT_BOOL(fan_pulse_witdh_ns == max_speed_pulse_width_ns);

    k_msleep(1000);

    fan_set_speed_by_percentage(1);
    fan_pulse_witdh_ns = fan_get_speed_setting();
    LOG_INF("1%% => %uns", fan_pulse_witdh_ns);
    ASSERT_SOFT_BOOL(fan_pulse_witdh_ns == min_speed_pulse_width_ns);

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
