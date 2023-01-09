#include "front_leds.h"
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(user_leds_test);

ZTEST(hil, test_front_leds_patterns)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_USER_LEDS);

    front_leds_set_brightness(0x10);
    int ret_code;

    RgbColor custom = {60, 60, 0};
    // test all patterns
    for (int i = UserLEDsPattern_UserRgbLedPattern_OFF;
         i <= UserLEDsPattern_UserRgbLedPattern_BOOT_ANIMATION; ++i) {
        for (int j = 0; j <= 360; j = j + 90) {
            ret_code = front_leds_set_pattern(i, 90, j, &custom, 0, 0);
            zassert_equal(ret_code, 0);

            // time for visual inspection
            k_msleep(200);
        }
    }

    // verify that we don't have: color * pulsing_scale > 255
    ret_code =
        front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern_PULSING_RGB,
                               90, 180, &custom, 1000, 6);
    zassert_equal(ret_code, RET_ERROR_INVALID_PARAM);
}
