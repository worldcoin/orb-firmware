#include <ir_camera_timer_settings.h>
#include <ztest.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_ir_camera_system);

static void
print_config(char *name, const struct ir_camera_timer_settings *settings)
{
    LOG_INF("%s: on_time_us: %u, fps: %u, psc: %u, ccr: %u, arr: %u", name,
            settings->on_time_in_us, settings->fps, settings->psc,
            settings->ccr, settings->arr);
}

static void
test_on_time_turn_on_off(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts;
    uint32_t on_time_us = 0;

    // Turn off settings
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);

    // turn on with normal settings
    // 1ms on-time <=> fps = 100 at 10% duty cycle
    on_time_us = 1000;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(100, ts.fps, "must be 100, actual %u", ts.fps);
    zassert_true(ts.arr != 0, "must be != 0, actual %u", ts.arr);
    zassert_true(ts.ccr != 0, "must be != 0, actual %u", ts.ccr);
    settings = ts;

    // change to max settings
    // 10ms on-time: must block at 5ms maximum <=> 20 fps
    on_time_us = 10000;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US,
                  "must be IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US, actual %u",
                  ts.on_time_in_us);
    zassert_equal(ts.fps, 20, "must be 20, actual %u", ts.fps);

    settings = ts;

    // Turn off with previously on
    on_time_us = 0;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, on_time_us, "must be on_time_us");
    zassert_equal(0, ts.fps, "must be 0, actual %u", ts.fps);
    zassert_equal(0, ts.arr, "must be 0, actual %u", ts.arr);
    zassert_equal(0, ts.ccr, "must be 0, actual %u", ts.ccr);
}

static void
test_on_time_modify(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts;
    uint32_t on_time_us;

    LOG_INF("Modifying on-time duration");

    // Turn on with basic settings
    // store in settings
    on_time_us = 1000;
    settings = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(100, settings.fps, "must be 100, actual %u", settings.fps);

    print_config("current", &settings);

    // double on-time duration, still < 5ms
    // must divide FPS by 2 to keep duty cycle <= 10%
    // new settings based on on-time duration are applied
    on_time_us = 2000;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(on_time_us, ts.on_time_in_us, "must be on_time_us, actual %d",
                  ts.on_time_in_us);
    zassert_equal(50, ts.fps, "must be 50, actual %u", ts.fps);

    settings = ts;
    print_config("current", &settings);

    // cut on-time duration by 2
    // must keep FPS
    // prescaler must decrease to allow shorter on-time periods
    on_time_us = settings.on_time_in_us / 2;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(on_time_us, ts.on_time_in_us, "must be on_time_us, actual %d",
                  ts.on_time_in_us);
    zassert_equal(settings.fps, ts.fps, "fps must not change");
    zassert_true(ts.psc <= settings.psc, "prescaler must decrease");

    settings = ts;
    print_config("current", &settings);

    // increase on-time duration by 1.5
    // FPS settings allows for on-time up to 2000us
    // so FPS must not change
    on_time_us = settings.on_time_in_us * 1.5;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(on_time_us, ts.on_time_in_us, "must be on_time_us, actual %d",
                  ts.on_time_in_us);
    zassert_equal(settings.fps, ts.fps, "fps must not change");

    settings = ts;
    print_config("current", &settings);

    // check that FPS is still 50 before next tests
    zassert_equal(settings.fps, 50, "FPS must be 50 at this stage");

    // on-time higher than allowed
    // will set FPS to 20 (maximum possible value with on-time = 5000)
    on_time_us = 6000;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, 5000, "must be 5000, actual %d",
                  ts.on_time_in_us);
    zassert_equal(ts.fps, 20, "fps must be 20, actual %u", ts.fps);
    zassert_equal(ts.psc, 129, "fps must be 129, actual %u", ts.psc);

    settings = ts;
    print_config("current", &settings);

    // lowest possible value
    // FPS must not change
    on_time_us = 1;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, 1, "must be 5000, actual %d",
                  ts.on_time_in_us);
    zassert_equal(ts.fps, 20, "fps must be 20, actual %u", ts.fps);
    zassert_equal(ts.psc, settings.psc, "psc must not change: %u, actual %u",
                  settings.psc, ts.psc);

    print_config("current", &settings);
}

static void
test_fps_turn_on_off(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts;
    uint16_t fps = 0;

    // Turn off settings
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(0, ts.fps, "must be 0");
    zassert_equal(0, ts.on_time_in_us, "must be 0");
    zassert_equal(0, ts.arr, "must be 0");
    zassert_equal(0, ts.ccr, "must be 0");

    settings = ts;
    print_config("turned off:", &settings);

    // Turn on
    fps = 50;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_true(ts.on_time_in_us != 0, "must be != 0");
    zassert_true(ts.arr != 0, "must be != 0");
    zassert_true(ts.ccr != 0, "must be != 0");

    settings = ts;
    print_config("turned on:", &settings);

    // Change to max settings
    fps = 1200;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(ts.fps, 1000, "Must be 1000, actual %u", ts.fps);

    settings = ts;
    print_config("max settings (fps set to 1200):", &settings);

    // turn off
    ts = timer_settings_from_fps(0, &settings);
    zassert_equal(0, ts.fps, "must be 0");
    zassert_equal(0, ts.on_time_in_us, "must be 0");
    zassert_equal(0, ts.arr, "must be 0");
    zassert_equal(0, ts.ccr, "must be 0");

    settings = ts;
    print_config("turned off:", &settings);
}

static void
test_fps_modify(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts;
    uint16_t fps = 0;

    // Turn on
    fps = 50;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);

    settings = ts;
    print_config("current", &settings);

    // increase FPS
    // on-time must decrease to keep 10% duty cycle
    fps = 60;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_true(settings.on_time_in_us > ts.on_time_in_us, "must decrease");

    settings = ts;
    print_config("current", &settings);

    // decrease FPS
    // on-time shouldn't change
    fps = 20;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(ts.on_time_in_us, settings.on_time_in_us,
                  "must not change: %u, actual %u", settings.on_time_in_us,
                  ts.on_time_in_us);

    settings = ts;
    print_config("current", &settings);
}

static void
test_both(void)
{
    struct ir_camera_timer_settings settings = {0};
    struct ir_camera_timer_settings ts;
    uint16_t fps = 0;
    uint16_t on_time_us = 0;

    fps = 1;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(ts.on_time_in_us, 5000, "Must be 5000, actual %u",
                  ts.on_time_in_us);

    settings = ts;
    print_config("current", &settings);

    // Error case
    // Current PSC doesn't allow for on-time duration below < 15us
    on_time_us = 5;
    ts = timer_settings_from_on_time_us(on_time_us, &settings);
    zassert_equal(ts.on_time_in_us, 5, "Must be 5, actual %u",
                  ts.on_time_in_us);
    zassert_true(ts.ccr != 0, "CCR must not be 0");

    settings = ts;
    print_config("current", &settings);

    // change to FPS = 2
    // on-time duration still lower than minimum time with prescaler for FPS=2
    fps = 2;
    ts = timer_settings_from_fps(fps, &settings);
    zassert_equal(fps, ts.fps, "must be %u, actual %u", fps, ts.fps);
    zassert_equal(ts.on_time_in_us, 5, "Must be 5, actual %u",
                  ts.on_time_in_us);
    zassert_true(ts.ccr != 0, "CCR must not be 0");

    settings = ts;
    print_config("current", &settings);
}

void
test_main(void)
{
    ztest_test_suite(ir_camera_test_timer_settings_on_time,
                     ztest_unit_test(test_on_time_turn_on_off),
                     ztest_unit_test(test_on_time_modify));

    ztest_test_suite(ir_camera_test_timer_settings_fps,
                     ztest_unit_test(test_fps_turn_on_off),
                     ztest_unit_test(test_fps_modify));

    ztest_test_suite(ir_camera_test_timer_settings_both,
                     ztest_unit_test(test_both));

    ztest_run_test_suite(ir_camera_test_timer_settings_on_time);
    ztest_run_test_suite(ir_camera_test_timer_settings_fps);
    ztest_run_test_suite(ir_camera_test_timer_settings_both);
}
