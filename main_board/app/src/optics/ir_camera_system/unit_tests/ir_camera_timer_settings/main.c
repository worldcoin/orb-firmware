#include <ir_camera_timer_settings.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(timer_settings_on_time, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(timer_settings_fps, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(ir_740nm_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(timer_settings_on_time, test_on_time_set_0us_with_0_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us;
    ret_code_t ret;

    // Try 0µs
    on_time_us = 0;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_on_time, test_on_time_set_under_max_with_0_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us;
    ret_code_t ret;

    on_time_us = IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US / 2;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_on_time, test_on_time_set_at_max_with_0_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us;
    ret_code_t ret;

    on_time_us = IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_on_time, test_on_time_over_max_with_0_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us;
    ret_code_t ret;

    on_time_us = IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US + 1;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    memset(&on_time_us, 0xff, sizeof on_time_us);
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
}

ZTEST(ir_740nm_tests, test_on_time_within_45_percent_duty_cycle_740nm)
{
    struct ir_camera_timer_settings settings = {0};
    uint16_t fps = 0;

    // Turn off settings
    timer_settings_from_fps(fps, &settings, &settings);

    // FPS = 0 so no CCR calculation
    timer_740nm_ccr_from_on_time_us(12, &settings, &settings);
    zassert_equal(0, settings.ccr_740nm, "must be 0, actual %u",
                  settings.ccr_740nm);
    zassert_equal(settings.ccr_740nm, 0,
                  "ccr_740nm should be 0 when FPS = 0, but it was %u",
                  settings.ccr_740nm);
    zassert_equal(settings.on_time_in_us_740nm, 12,
                  "on_time_in_us_740nm should be set no matter what, in this "
                  "case to 12, but it was %u",
                  settings.on_time_in_us_740nm);

    fps = 1;
    timer_settings_from_fps(fps, &settings, &settings);

    // under limit
    timer_740nm_ccr_from_on_time_us(100000, &settings, &settings);
    zassert_equal((uint16_t)(settings.arr / 2 * 0.2), settings.ccr_740nm,
                  "expected %u, but got %u", (uint16_t)(settings.arr / 2 * 0.2),
                  settings.ccr_740nm);

    // at limit
    timer_740nm_ccr_from_on_time_us(225000, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.45), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.45),
                   settings.ccr_740nm);

    // over limit
    timer_740nm_ccr_from_on_time_us(300000, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.45), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.45),
                   settings.ccr_740nm);

    fps = 30;
    timer_settings_from_fps(fps, &settings, &settings);

    // under limit
    timer_740nm_ccr_from_on_time_us(5000, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.3), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.2),
                   settings.ccr_740nm);

    // at limit
    timer_740nm_ccr_from_on_time_us(7500, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.45), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.45),
                   settings.ccr_740nm);

    // over limit
    timer_740nm_ccr_from_on_time_us(10000, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.45), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.45),
                   settings.ccr_740nm);

    fps = 60;
    timer_settings_from_fps(fps, &settings, &settings);

    // under limit
    timer_740nm_ccr_from_on_time_us(500, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.06), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.06),
                   settings.ccr_740nm);

    // at limit
    timer_740nm_ccr_from_on_time_us(3750, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.45), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.45),
                   settings.ccr_740nm);

    // over limit
    timer_740nm_ccr_from_on_time_us(5000, &settings, &settings);
    zassert_within((uint16_t)(settings.arr / 2 * 0.45), settings.ccr_740nm, 1,
                   "expected %u, but got %u", (uint16_t)(settings.arr * 0.45),
                   settings.ccr_740nm);
}

ZTEST(ir_740nm_tests, test_on_time_740nm_when_on_time_is_not_zero)
{
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, on_time_us_740nm, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = 1000;
    on_time_us_740nm = 500;
    // write a test to reach line 147 in ir_camera_timer_settings.c
    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    ret = timer_740nm_ccr_from_on_time_us(on_time_us_740nm, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us_740nm, on_time_us_740nm,
                  "must be on_time_us_740nm");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.on_time_in_us_740nm, on_time_us_740nm,
                  "must be on_time_us_740nm");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);
    zassert_not_equal(0, ts.ccr_740nm, "must not be 0, actual %u",
                      ts.ccr_740nm);
}

ZTEST(timer_settings_on_time, test_on_time_with_corresponding_max_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 59;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    duty_cycle = 0.15; // 15%
#else
    duty_cycle = 0.25; // 25%
#endif
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_on_time, test_on_time_with_corresponding_max_fps_plus_1)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 59;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    duty_cycle = 0.15; // 15%
#else
    duty_cycle = 0.25; // 25%
#endif
    on_time_us = (1000000.0 / fps) * duty_cycle;
    fps++; // too high

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, 0, "must be %u, actual %u", 0, ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_on_time,
      test_on_time_set_valid_then_set_fps_to_zero_on_time_should_be_zeroed)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = ((1000000.0 / fps) * 0.05); // 5%

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);

    // check that the on-time is zeroed and FPS-related settings are cleared
    fps = 0;
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    fps = 30;
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_on_time, test_on_time_set_valid_then_lower_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = ((1000000.0 / fps) * 0.025); // 2.5%

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);

    on_time_us /= 2;
    settings = ts;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.psc, ts.psc,
                  "must be unchanged, changed from %u to %u", settings.psc,
                  ts.psc);
    zassert_equal(settings.arr, ts.arr,
                  "must be unchanged, changed from %u to %u", settings.arr,
                  ts.arr);
    zassert_equal(settings.ccr / 2, ts.ccr,
                  "must be 1/2 of original, changed from %u to %u",
                  settings.ccr, ts.ccr);
}

ZTEST(timer_settings_on_time,
      test_on_time_set_valid_then_increase_to_another_valid_value)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = ((1000000.0 / fps) * 0.05); // 5%

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us,
                  "ts.on_time_in_us (%u) should equal on_time_us (%u)",
                  ts.on_time_in_us, on_time_us);
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);

    on_time_us = ((1000000.0 / fps) * 0.06); // 6%
    settings = ts;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.psc, ts.psc,
                  "must be unchanged, changed from %u to %u", settings.psc,
                  ts.psc);
    zassert_equal(settings.arr, ts.arr,
                  "must be unchanged, changed from %u to %u", settings.arr,
                  ts.arr);
    zassert_true(ts.ccr > settings.ccr,
                 "ccr must increase, changed from %u to %u", settings.ccr,
                 ts.ccr);
}

ZTEST(timer_settings_on_time,
      test_on_time_set_valid_then_increase_to_an_invalid_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    duty_cycle = 0.15; // 15%
#else
    duty_cycle = 0.25; // 25%
#endif
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    // 60 fps is the minimum FPS which is valid for an on-time of 2500 (Pearl),
    // 4166 (Diamond)
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be 0, actual %u", ts.ccr);

    // This should be invalid and all settings should be preserved
    on_time_us++;
    settings = ts;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, settings.on_time_in_us,
                  "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.psc, ts.psc,
                  "must be unchanged, changed from %u to %u", settings.psc,
                  ts.psc);
    zassert_equal(settings.arr, ts.arr,
                  "must be unchanged, changed from %u to %u", settings.arr,
                  ts.arr);
    zassert_equal(settings.ccr, ts.ccr,
                  "must be 120%% of original, changed from %u to %u",
                  settings.ccr, ts.ccr);
}

ZTEST(timer_settings_on_time, test_on_time_set_very_low_when_fps_is_at_minimum)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 1;
    on_time_us = 10;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    // the calculated ccr would be 0 but it should be capped to 1
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(1, ts.ccr, "must be 1, actual %u", ts.ccr);

    // same should apply for setting the on-time at 1 fps
    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(1, ts.ccr, "must be 1, actual %u", ts.ccr);
}

ZTEST(timer_settings_fps, test_fps_under_max_fps_0_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t fps;
    ret_code_t ret;

    fps = IR_CAMERA_SYSTEM_MAX_FPS / 2;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.fps, fps, "must be fps");
    zassert_equal(0, ts.on_time_in_us, "must be 0, actual %u",
                  ts.on_time_in_us);
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);
}

ZTEST(timer_settings_fps, test_fps_at_max_0_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t fps;
    ret_code_t ret;

    fps = IR_CAMERA_SYSTEM_MAX_FPS;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.fps, fps, "must be fps");
    zassert_equal(0, ts.on_time_in_us, "must be 0, actual %u",
                  ts.on_time_in_us);
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);
}

ZTEST(timer_settings_fps, test_fps_over_max_0_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t fps;
    ret_code_t ret;

    fps = IR_CAMERA_SYSTEM_MAX_FPS + 1;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.fps, 0, "must be 0");
    zassert_equal(0, ts.on_time_in_us, "must be 0, actual %u",
                  ts.on_time_in_us);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
}

ZTEST(timer_settings_fps, test_fps_set_valid_then_increase_to_an_invalid_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    duty_cycle = 0.15; // 15%
#else
    duty_cycle = 0.25; // 25%
#endif
    on_time_us = (1000000.0 / fps) * duty_cycle;

    // 60 fps is the minimum FPS which is valid for an on-time of 2500 (Pearl),
    // 4166 (Diamond)
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(fps, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_not_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_not_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    // This should be invalid and all settings should be preserved
    fps++;
    settings = ts;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, settings.on_time_in_us,
                  "must be on_time_us");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(fps - 1, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.psc, ts.psc,
                  "must be unchanged, changed from %u to %u", settings.psc,
                  ts.psc);
    zassert_equal(settings.arr, ts.arr,
                  "must be unchanged, changed from %u to %u", settings.arr,
                  ts.arr);
    zassert_equal(settings.ccr, ts.ccr,
                  "must be 120%% of original, changed from %u to %u",
                  settings.ccr, ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);
}

ZTEST(timer_settings_fps,
      test_fps_set_valid_then_increase_to_another_valid_value)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 30;
    on_time_us = (1000000.0 / fps) * 0.07; // 7%

    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(fps, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_not_equal(0, ts.psc, "must not be zero", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be zero", ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be zero", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    fps++;
    settings = ts;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_true(settings.psc >= ts.psc, "must be >=, changed from %u to %u",
                 settings.psc, ts.psc);
    zassert_not_equal(0, ts.arr, "must not be zero");
    zassert_not_equal(0, ts.ccr, "must not be zero");
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);
}

ZTEST(timer_settings_fps, test_fps_set_valid_then_lower_fps)
{

    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    duty_cycle = 0.15; // 15%
#else
    duty_cycle = 0.25; // 25%
#endif
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    settings = ts;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(settings.psc, ts.psc, "must not have changed. Was %u, now %u",
                  settings.psc, ts.psc);
    zassert_equal(settings.arr, ts.arr, "must not have changed. Was %u, now %u",
                  settings.arr, ts.arr);
    zassert_not_equal(0, ts.ccr, "must not be zero", ts.ccr);

    fps /= 2;
    settings = ts;

    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_true(settings.psc <= ts.psc, "must be <=, changed from %u to %u",
                 settings.psc, ts.psc);
    zassert_not_equal(0, ts.arr, "must not be zero");
    zassert_not_equal(0, ts.ccr, "must not be zero");
}

ZTEST(timer_settings_fps, test_fps_set_valid_then_invalid_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
#if defined(CONFIG_BOARD_PEARL_MAIN)
    duty_cycle = 0.16; // 16%
#else
    duty_cycle = 0.26; // 26%
#endif
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_fps(60, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.psc, "must not be 0, actual %u", ts.psc);
    zassert_not_equal(0, ts.arr, "must not be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);

    settings = ts;

    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(0, ts.on_time_in_us_740nm, "must be 0, actual %u",
                  ts.on_time_in_us_740nm);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.psc, ts.psc, "must not have changed. Was %u, now %u",
                  settings.psc, ts.psc);
    zassert_equal(settings.arr, ts.arr, "must not have changed. Was %u, now %u",
                  settings.arr, ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);
    zassert_equal(0, ts.ccr_740nm, "must be 0, actual %u", ts.ccr_740nm);
}
