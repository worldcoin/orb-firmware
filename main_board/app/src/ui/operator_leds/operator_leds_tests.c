#include "operator_leds_tests.h"
#include "operator_leds.h"
#include "ui/rgb_leds.h"
#include <app_config.h>
#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(operator_leds_test);

static K_THREAD_STACK_DEFINE(operator_leds_test_thread_stack, 1024);
static struct k_thread test_thread_data;

/// Test all patterns with 2 brightness levels
static void
operator_leds_test_thread()
{
    uint8_t brightness[2] = {0x08, 0x10};
    uint8_t idx = 0;
    int ret_code;

    RgbColor color = RGB_ORANGE;
    while (1) {
        ret_code = operator_leds_set_brightness(brightness[idx]);
        if (ret_code) {
            LOG_ERR("operator_leds_set_brightness returned: %i", ret_code);
        }

        idx = (1 - idx);

        for (int i = DistributorLEDsPattern_DistributorRgbLedPattern_OFF;
             i <= DistributorLEDsPattern_DistributorRgbLedPattern_RGB; ++i) {
            for (uint32_t j = 0; j <= OPERATOR_LEDS_ALL_MASK; j++) {
                ret_code = operator_leds_set_pattern(i, j, &color);
                if (ret_code) {
                    LOG_ERR("operator_leds_set_pattern returned: %i", ret_code);
                }
                k_msleep(500);
            }
        }
    }
}

void
operator_leds_tests_init(void)
{
    k_tid_t tid =
        k_thread_create(&test_thread_data, operator_leds_test_thread_stack,
                        K_THREAD_STACK_SIZEOF(operator_leds_test_thread_stack),
                        operator_leds_test_thread, NULL, NULL, NULL,
                        THREAD_PRIORITY_TESTS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "operator_leds_test");
}
