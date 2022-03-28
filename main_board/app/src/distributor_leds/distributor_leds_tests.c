//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#include "distributor_leds_tests.h"
#include "distributor_leds.h"
#include <app_config.h>
#include <logging/log.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(distributor_leds_test);

static K_THREAD_STACK_DEFINE(distributor_leds_test_thread_stack, 1024);
static struct k_thread test_thread_data;

/// Test all patterns with 2 brightness levels
static void
distributor_leds_test_thread()
{
    uint8_t brightness[2] = {0x10, 0x80};
    uint8_t idx = 0;
    while (1) {
        distributor_leds_set_brightness(brightness[idx]);
        idx = (1 - idx);

        for (int i = DistributorLEDsPattern_DistributorRgbLedPattern_OFF;
             i <= DistributorLEDsPattern_DistributorRgbLedPattern_ALL_BLUE;
             ++i) {
            distributor_leds_set_pattern(i);
            k_msleep(1000);
        }
    }
}

void
distributor_leds_tests_init(void)
{
    k_tid_t tid = k_thread_create(
        &test_thread_data, distributor_leds_test_thread_stack,
        K_THREAD_STACK_SIZEOF(distributor_leds_test_thread_stack),
        distributor_leds_test_thread, NULL, NULL, NULL, THREAD_PRIORITY_TESTS,
        0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning test_thread thread");
    }
}
