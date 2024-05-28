#include "ir_camera_timer_settings.h"
#ifdef CONFIG_ZTEST
#include <zephyr/logging/log.h>
#else
#include "orb_logs.h"
#endif
#include <utils.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ir_camera_timer_settings,
                    CONFIG_IR_CAMERA_SYSTEM_LOG_LEVEL);

ret_code_t
timer_settings_from_on_time_us(
    uint16_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings)
{
    ret_code_t ret;
    struct ir_camera_timer_settings ts = *current_settings;

    if (on_time_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if ((current_settings->fps != 0) &&
               ((1e6 / current_settings->fps *
                 IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE) < on_time_us)) {
        ret = RET_ERROR_INVALID_PARAM;
        LOG_ERR(
            "On-time duration must not exceed %uµs for the current FPS setting",
            (uint16_t)(1e6 / current_settings->fps *
                       IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE));
    } else {
        ret = RET_SUCCESS;
        ts.on_time_in_us = on_time_us;
    }

    if (ret == RET_SUCCESS) {
        // make copy operation atomic
        CRITICAL_SECTION_ENTER(k);
        *new_settings = ts;
        CRITICAL_SECTION_EXIT(k);
    }

    return ret;
}

ret_code_t
timer_settings_from_fps(uint16_t fps,
                        const struct ir_camera_timer_settings *current_settings,
                        struct ir_camera_timer_settings *new_settings)
{
    ret_code_t ret;
    struct ir_camera_timer_settings ts = {0};

    if (fps == 0) {
        // Call timer settings depend on PSC, which depends on the FPS.
        // So if the FPS is set to zero, all timer settings are invalidated and
        // we set them to zero.
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.fps = 0;
        ts.master_psc = 0;
        ts.master_arr = 0;
    } else if (fps > IR_CAMERA_SYSTEM_MAX_FPS) {
        // Do nothing if we have an invalid FPS parameter.
        ret = RET_ERROR_INVALID_PARAM;
    } else {
        // Now we know that we have a valid FPS greater than zero.
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.fps = fps;
        ts.master_psc =
            TIMER_CLOCK_FREQ_HZ / ((1 << TIMER_COUNTER_WIDTH_BITS) * ts.fps);
        ts.master_arr = TIMER_CLOCK_FREQ_HZ / ((ts.master_psc + 1) * ts.fps);

        if (current_settings->on_time_in_us != 0) {
            uint16_t max_on_time_us_for_this_fps =
                MIN(1e6 / ts.fps * IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE,
                    IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US);

            // We reject the new passed FPS if maximum on-time duration is
            // shorter than the current duration
            if (max_on_time_us_for_this_fps < current_settings->on_time_in_us) {
                LOG_ERR(
                    "New FPS value the violates safety constraints given the "
                    "current on-time settings. The maximum on-time for the "
                    "requested new FPS of %u is %uµs, but the current on-time "
                    "setting is %uµs",
                    fps, max_on_time_us_for_this_fps,
                    current_settings->on_time_in_us);
                ret = RET_ERROR_INVALID_PARAM;
            }
        }
    }

    if (ret == RET_SUCCESS) {
        // make copy operation atomic
        CRITICAL_SECTION_ENTER(k);
        *new_settings = ts;
        CRITICAL_SECTION_EXIT(k);
    }
    return ret;
}

void
timer_settings_print(const struct ir_camera_timer_settings *settings)
{
    LOG_DBG("fps                 = %5u", settings->fps);
    LOG_DBG("master_psc          = %5u", settings->master_psc);
    LOG_DBG("master_arr          = %5u", settings->master_arr);
    LOG_DBG("on_time_in_us       = %5u", settings->on_time_in_us);
}
