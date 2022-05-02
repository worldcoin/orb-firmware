#include "front_leds_tests.h"
#include "front_leds.h"
#include <app_config.h>
#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(fu_rgb_leds_test);

static K_THREAD_STACK_DEFINE(fu_rgb_leds_test_thread_stack, 1024);
static struct k_thread test_thread_data;

static void
fu_rgb_leds_test_thread()
{
    front_leds_set_brightness(0x10);

    RgbColor custom = {60, 60, 0};
    while (1) {
        // test all patterns
        for (int i = UserLEDsPattern_UserRgbLedPattern_OFF;
             i <= UserLEDsPattern_UserRgbLedPattern_RGB; ++i) {
            for (int j = 0; j <= 360; j = j + 90) {
                front_leds_set_pattern(i, 90, j, &custom);
                k_msleep(1000);
            }
            for (int j = 360; j >= 0; j = j - 90) {
                front_leds_set_pattern(i, 90, j, &custom);
                k_msleep(1000);
            }
        }
    }
}

void
front_unit_rdb_leds_tests_init(void)
{
    k_thread_create(&test_thread_data, fu_rgb_leds_test_thread_stack,
                    K_THREAD_STACK_SIZEOF(fu_rgb_leds_test_thread_stack),
                    fu_rgb_leds_test_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_TESTS, 0, K_NO_WAIT);
}
