#include <ir_camera_timer_settings.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(timer_settings_on_time, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(timer_settings_fps, NULL, NULL, NULL, NULL, NULL);

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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);
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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);
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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);
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
    zassert_equal(0, ts.on_time_in_us, "must be 0, actual %u", ts.on_time_in_us);
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    memset(&on_time_us, 0xff, sizeof on_time_us);
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(0, ts.on_time_in_us, "must be 0, actual %u", ts.on_time_in_us);
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);
}

ZTEST(timer_settings_on_time, test_on_time_with_corresponding_max_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 59;
    duty_cycle = IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE;
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);
}

ZTEST(timer_settings_on_time, test_on_time_with_corresponding_max_fps_plus_1)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 59;
    duty_cycle = IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE;
    on_time_us = (1000000.0 / fps) * duty_cycle;
    fps++; // too high

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, 0, "must be %u, actual %u", 0, ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);
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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    // check that the on-time is zeroed and FPS-related settings are cleared
    fps = 0;
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    fps = 30;
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);
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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    on_time_us /= 2;
    settings = ts;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.master_psc, ts.master_psc,
                  "must be unchanged, changed from %u to %u",
                  settings.master_psc, ts.master_psc);
    zassert_equal(settings.master_arr, ts.master_arr,
                  "must be unchanged, changed from %u to %u",
                  settings.master_arr, ts.master_arr);
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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us,
                  "ts.on_time_in_us (%u) should equal on_time_us (%u)",
                  ts.on_time_in_us, on_time_us);
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    on_time_us = ((1000000.0 / fps) * 0.06); // 6%
    settings = ts;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.master_psc, ts.master_psc,
                  "must be unchanged, changed from %u to %u",
                  settings.master_psc, ts.master_psc);
    zassert_equal(settings.master_arr, ts.master_arr,
                  "must be unchanged, changed from %u to %u",
                  settings.master_arr, ts.master_arr);
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
    duty_cycle = IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE;
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    // 60 fps is the minimum FPS which is valid for an on-time of 2500 (Pearl),
    // 4166 (Diamond)
    ret = timer_settings_from_fps(fps, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    // This should be invalid and all settings should be preserved
    on_time_us++;
    settings = ts;
    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, settings.on_time_in_us,
                  "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.master_psc, ts.master_psc,
                  "must be unchanged, changed from %u to %u",
                  settings.master_psc, ts.master_psc);
    zassert_equal(settings.master_arr, ts.master_arr,
                  "must be unchanged, changed from %u to %u",
                  settings.master_arr, ts.master_arr);
}

// ZTEST(timer_settings_on_time,
// test_on_time_set_very_low_when_fps_is_at_minimum)
// {
//     struct ir_camera_timer_settings settings = {0};
//     struct ir_camera_timer_settings ts = {0};
//     uint32_t on_time_us, fps;
//     ret_code_t ret;

//     fps = 1;
//     on_time_us = 10;

//     ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
//     zassert_equal(RET_SUCCESS, ret, "");
//     zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
//     zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
//     zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
//     zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

//     // the calculated slave_arr would be 0 but it should be capped to 1
//     ret = timer_settings_from_fps(fps, &ts, &ts);
//     zassert_equal(RET_SUCCESS, ret, "");
//     zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
//     zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
//     zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
//     ts.master_psc); zassert_not_equal(0, ts.master_arr, "must not be 0,
//     actual %u", ts.master_arr);

//     // same should apply for setting the on-time at 1 fps
//     ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
//     zassert_equal(RET_SUCCESS, ret, "");
//     zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
//     zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
//     zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
//     ts.master_psc); zassert_not_equal(0, ts.master_arr, "must not be 0,
//     actual %u", ts.master_arr);
// }

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
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);
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
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);
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
    zassert_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);
}

ZTEST(timer_settings_fps, test_fps_set_valid_then_increase_to_an_invalid_fps)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
    duty_cycle = IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE;
    on_time_us = (1000000.0 / fps) * duty_cycle;

    // 60 fps is the minimum FPS which is valid for an on-time of 2500 (Pearl),
    // 4166 (Diamond)
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_not_equal(0, ts.master_psc, "must be 0, actual %u", ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must be 0, actual %u", ts.master_arr);

    // This should be invalid and all settings should be preserved
    fps++;
    settings = ts;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, settings.on_time_in_us,
                  "must be on_time_us");
    zassert_equal(fps - 1, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.master_psc, ts.master_psc,
                  "must be unchanged, changed from %u to %u",
                  settings.master_psc, ts.master_psc);
    zassert_equal(settings.master_arr, ts.master_arr,
                  "must be unchanged, changed from %u to %u",
                  settings.master_arr, ts.master_arr);
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
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be zero", ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be zero", ts.master_arr);

    fps++;
    settings = ts;
    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_true(settings.master_psc >= ts.master_psc,
                 "must be >=, changed from %u to %u", settings.master_psc,
                 ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be zero");
}

ZTEST(timer_settings_fps, test_fps_set_valid_then_lower_fps)
{

    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
    duty_cycle = IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE;
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    settings = ts;

    ret = timer_settings_from_on_time_us(on_time_us, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(settings.master_psc, ts.master_psc,
                  "must not have changed. Was %u, now %u", settings.master_psc,
                  ts.master_psc);
    zassert_equal(settings.master_arr, ts.master_arr,
                  "must not have changed. Was %u, now %u", settings.master_arr,
                  ts.master_arr);

    fps /= 2;
    settings = ts;

    ret = timer_settings_from_fps(fps, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_true(settings.master_psc <= ts.master_psc,
                 "must be <=, changed from %u to %u", settings.master_psc,
                 ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be zero");
}

ZTEST(timer_settings_fps, test_fps_set_valid_then_invalid_on_time)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts = {0};
    uint32_t on_time_us, fps;
    ret_code_t ret;
    double duty_cycle;

    fps = 60;
    duty_cycle = IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE + 0.01;
    on_time_us = (1000000.0 / fps) * duty_cycle;

    ret = timer_settings_from_fps(60, &settings, &ts);
    zassert_equal(RET_SUCCESS, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(ts.fps, fps, "must be %u, actual %u", fps, ts.fps);
    zassert_not_equal(0, ts.master_psc, "must not be 0, actual %u",
                      ts.master_psc);
    zassert_not_equal(0, ts.master_arr, "must not be 0, actual %u",
                      ts.master_arr);

    settings = ts;

    ret = timer_settings_from_on_time_us(on_time_us, &ts, &ts);
    zassert_equal(RET_ERROR_INVALID_PARAM, ret, "");
    zassert_equal(ts.on_time_in_us, 0, "must be 0");
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(settings.master_psc, ts.master_psc,
                  "must not have changed. Was %u, now %u", settings.master_psc,
                  ts.master_psc);
    zassert_equal(settings.master_arr, ts.master_arr,
                  "must not have changed. Was %u, now %u", settings.master_arr,
                  ts.master_arr);
}
