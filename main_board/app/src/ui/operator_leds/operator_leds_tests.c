#include "operator_leds.h"
#include "ui/rgb_leds.h"
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "logs_can.h"
LOG_MODULE_REGISTER(operator_leds_test);

/// Test all patterns with 2 brightness levels
ZTEST(hil, test_operator_leds_patterns)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_OPERATOR_LEDS);

    uint8_t brightness[2] = {0x08, 0x10};
    RgbColor color = RGB_ORANGE;
    int ret_code;

    for (size_t b = 0; b < ARRAY_SIZE(brightness); ++b) {
        ret_code = operator_leds_set_brightness(brightness[b]);
        zassert_equal(ret_code, 0);

        for (int i = DistributorLEDsPattern_DistributorRgbLedPattern_OFF;
             i <= DistributorLEDsPattern_DistributorRgbLedPattern_PULSING_RGB;
             ++i) {
            for (uint32_t j = 1; j <= OPERATOR_LEDS_ALL_MASK; j = j * 2) {
                ret_code = operator_leds_set_pattern(i, j, &color);
                zassert_equal(ret_code, 0);

                // give time for visual inspection
                k_msleep(100);
            }
        }
    }

    ret_code = operator_leds_set_pattern(
        DistributorLEDsPattern_DistributorRgbLedPattern_OFF, 0, NULL);
    zassert_equal(ret_code, 0);
}
