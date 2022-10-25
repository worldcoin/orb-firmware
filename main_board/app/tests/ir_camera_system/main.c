#include <assert.h>
#include <ir_camera_timer_settings.h>
#include <ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_ir_camera_system);

static void
test_on_time_set_0us_with_0_fps(void)
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

static void
test_on_time_set_under_max_with_0_fps(void)
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

static void
test_on_time_set_at_max_with_0_fps(void)
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

static void
test_on_time_over_max_with_0_fps(void)
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

static void
test_on_time_within_45_percent_duty_cycle_740nm(void)
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

static void
test_on_time_with_corresponding_max_fps(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 59;
    on_time_us = ((1000000.0 / fps) * 0.1); // 10%

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

static void
test_on_time_with_corresponding_max_fps_plus_1(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 59;
    on_time_us = ((1000000.0 / fps) * 0.1); // 10%
    fps++;                                  // too high

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

static void
test_on_time_set_valid_then_set_fps_to_zero_on_time_should_be_zeroed(void)
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

static void
test_on_time_set_valid_then_lower_on_time(void)
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

static void
test_on_time_set_valid_then_increase_to_another_valid_value(void)
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

static void
test_on_time_set_valid_then_increase_to_an_invalid_on_time(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = (1000000.0 / fps) * 0.10; // 10%

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.psc, "must be 0, actual %u", ts.psc);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    // 60 fps is the minimum FPS which is valid for an on-time of 1666
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

static void
test_fps_under_max_fps_0_on_time(void)
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

static void
test_fps_at_max_0_on_time(void)
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

static void
test_fps_over_max_0_on_time(void)
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

static void
test_fps_set_valid_then_increase_to_an_invalid_fps(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = (1000000.0 / fps) * 0.10; // 10%

    // 60 fps is the minimum FPS which is valid for an on-time of 1666
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

static void
test_fps_set_valid_then_increase_to_another_valid_value(void)
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

static void
test_fps_set_valid_then_lower_fps(void)
{

    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = (1000000.0 / fps) * 0.10; // 10%

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

static void
test_fps_set_valid_then_invalid_on_time(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;

    fps = 60;
    on_time_us = (1000000.0 / fps) * 0.11; // 11%

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

void
test_main(void)
{
    ztest_test_suite(
        ir_camera_test_timer_settings_on_time,
        ztest_unit_test(
            test_on_time_set_valid_then_increase_to_an_invalid_on_time),
        ztest_unit_test(
            test_on_time_set_valid_then_increase_to_another_valid_value),
        ztest_unit_test(test_on_time_set_valid_then_lower_on_time),
        ztest_unit_test(
            test_on_time_set_valid_then_set_fps_to_zero_on_time_should_be_zeroed),
        ztest_unit_test(test_on_time_with_corresponding_max_fps_plus_1),
        ztest_unit_test(test_on_time_with_corresponding_max_fps),
        ztest_unit_test(test_on_time_set_0us_with_0_fps),
        ztest_unit_test(test_on_time_set_under_max_with_0_fps),
        ztest_unit_test(test_on_time_set_at_max_with_0_fps),
        ztest_unit_test(test_on_time_over_max_with_0_fps));

    ztest_test_suite(
        ir_camera_test_timer_settings_fps,
        ztest_unit_test(test_fps_set_valid_then_lower_fps),
        ztest_unit_test(test_fps_over_max_0_on_time),
        ztest_unit_test(test_fps_at_max_0_on_time),
        ztest_unit_test(test_fps_set_valid_then_increase_to_an_invalid_fps),
        ztest_unit_test(
            test_fps_set_valid_then_increase_to_another_valid_value),
        ztest_unit_test(test_fps_under_max_fps_0_on_time),
        ztest_unit_test(test_fps_set_valid_then_invalid_on_time));

    ztest_test_suite(
        ir_740nm_tests,
        ztest_unit_test(test_on_time_within_45_percent_duty_cycle_740nm));

    ztest_run_test_suite(ir_camera_test_timer_settings_on_time);
    ztest_run_test_suite(ir_camera_test_timer_settings_fps);
    ztest_run_test_suite(ir_740nm_tests);
}
