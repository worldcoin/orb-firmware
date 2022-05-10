#include "ir_camera_timer_settings.h"
#include "kernel.h"
#include <logging/log.h>
#include <sys/util.h>
#include <utils.h>
LOG_MODULE_REGISTER(ir_camera_timer_settings);

#define MAX_PSC_DIV                  65536U
#define ASSUMED_TIMER_CLOCK_FREQ_MHZ 170

ret_code_t
timer_settings_from_on_time_us(
    uint16_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings)
{
    ret_code_t ret;
    struct ir_camera_timer_settings ts;

    if (on_time_us == 0) {
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.ccr = 0;
        ts.on_time_in_us = 0;
    } else if (on_time_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if (current_settings->fps == 0) {
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.on_time_in_us = on_time_us;
    } else {
        // on-time duration must not be longer than
        // IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US
        ts.on_time_in_us = on_time_us;

        // find the FPS that matches the on-time duration
        ts.fps = 100000.0 / ts.on_time_in_us;

        ts.psc = ASSUMED_TIMER_CLOCK_FREQ / (MAX_PSC_DIV * ts.fps);
        ts.arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);
        ts.ccr =
            (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);

        // if computed max FPS is lower than current FPS settings, then refuse
        // to compute new settings
        if (ts.fps < current_settings->fps) {
            LOG_ERR(
                "New on-time value violates the safety constraints given the "
                "current FPS setting. The maximum FPS for the requested new "
                "on-time of %uµs is %u, but the current FPS setting is %u",
                on_time_us, ts.fps, current_settings->fps);
            ret = RET_ERROR_INVALID_PARAM;
        } else {
            ret = RET_SUCCESS;
            // let's keep the previous FPS config
            ts.fps = current_settings->fps;

            if (current_settings->psc == 0) {
                ts.psc = ASSUMED_TIMER_CLOCK_FREQ / (MAX_PSC_DIV * ts.fps);
            }

            // take the higher prescaler to make sure ARR doesn't overflow
            // we might lose accuracy!
            // truncating ARR and CCR values below always make sure we don't
            // go over 10% duty cycle
            ts.psc =
                ts.psc > current_settings->psc ? ts.psc : current_settings->psc;

            // compute new ARR & CCR based on common PSC
            uint32_t arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);
            if (arr > 0xffff) {
                // not possible to keep the current FPS with new PSC
                ts.arr = 0xffff;
                uint16_t fps =
                    ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.arr);

                LOG_ERR("Current PSC doesn't allow for FPS=%u, must change FPS "
                        "to %u",
                        ts.fps, fps);
                ts.fps = fps;
                ret = RET_ERROR_INTERNAL;
            } else {
                ts.arr = (uint16_t)arr;
            }

            uint32_t accuracy_us = (ts.psc + 1) / ASSUMED_TIMER_CLOCK_FREQ_MHZ;
            if (accuracy_us > 1) {
                LOG_WRN("on-time duration accuracy: %uus", accuracy_us);
            }
            ts.ccr = (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) /
                     (ts.psc + 1);
            if (ts.ccr == 0) {
                // at least `accuracy_us` in the worst case scenario where
                // PSC=65535 only possible if on-time duration < accuracy in
                // this specific case: always < 10% duty cycle
                ts.ccr = 1;
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

ret_code_t
timer_settings_from_fps(uint16_t fps,
                        const struct ir_camera_timer_settings *current_settings,
                        struct ir_camera_timer_settings *new_settings)
{
    ret_code_t ret;
    struct ir_camera_timer_settings ts;

    if (fps == 0) {
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.fps = 0;
        ts.psc = 0;
        ts.arr = 0;
        ts.ccr = 0;
    } else if (fps > IR_CAMERA_SYSTEM_MAX_FPS) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if (current_settings->on_time_in_us == 0) {
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.fps = fps;
    } else {
        ts.fps = fps;
        ts.on_time_in_us =
            MIN(100000.0 / ts.fps, IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US);
        ts.psc = ASSUMED_TIMER_CLOCK_FREQ / (MAX_PSC_DIV * ts.fps);
        ts.arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);
        ts.ccr =
            (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);

        // We reject the new passed FPS if maximum on-time duration is shorter
        // than the current duration
        if (ts.on_time_in_us < current_settings->on_time_in_us) {
            LOG_ERR("New FPS value the violates safety constraints given the "
                    "current on-time settings. The maximum on-time for the "
                    "requested new FPS of %u is %uµs, but the current on-time "
                    "setting is %uµs",
                    fps, ts.on_time_in_us, current_settings->on_time_in_us);
            ret = RET_ERROR_INVALID_PARAM;
        } else if (current_settings->psc != ts.psc) {
            ret = RET_SUCCESS;
            ts.on_time_in_us = current_settings->on_time_in_us;

            // take the higher prescaler to make sure ARR doesn't overflow
            // we might lose accuracy!
            // truncating ARR and CCR values below always make sure we don't
            // go over 10% duty cycle
            ts.psc =
                ts.psc > current_settings->psc ? ts.psc : current_settings->psc;

            ts.arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);

            uint32_t accuracy_us = (ts.psc + 1) / ASSUMED_TIMER_CLOCK_FREQ_MHZ;
            if (accuracy_us > 1) {
                LOG_WRN("on-time duration accuracy: %uus", accuracy_us);
            }
            ts.ccr = (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) /
                     (ts.psc + 1);
            if (ts.ccr == 0) {
                // at least `accuracy_us` in the worst case scenario where
                // PSC=65535 only possible if on-time duration < accuracy in
                // this specific case: always < 10% duty cycle
                ts.ccr = 1;
            }
        } else {
            ret = RET_SUCCESS;
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
    LOG_DBG("fps           = %5u", settings->fps);
    LOG_DBG("psc           = %5u", settings->psc);
    LOG_DBG("arr           = %5u", settings->arr);
    LOG_DBG("ccr           = %5u", settings->ccr);
    LOG_DBG("on_time_in_us = %5u", settings->on_time_in_us);
}

ret_code_t
timer_740nm_ccr_from_on_time_us(
    uint32_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings)
{
    // can't compute new settings if FPS is not set
    if (current_settings->fps == 0) {
        new_settings->ccr_740nm = 0;
        return RET_ERROR_INVALID_STATE;
    }

    // All we need to verify is that the on time does not imply
    // a duty cycle > 45%.
    // If a duty cycle of > 45% would occur, then clamp it at 45%

    // would be 1_000_000 / fps to get us, but we run the leds at 2x frequency
    // of fps, so 1/2 it = 500000.0
    uint32_t forty_five_percent_of_current_period_us =
        (uint32_t)(500000.0 / current_settings->fps * 0.45);

    // apply psc and arr for current FPS
    struct ir_camera_timer_settings ts;
    ts = *current_settings;
    ts.psc = ASSUMED_TIMER_CLOCK_FREQ / (MAX_PSC_DIV * ts.fps);
    ts.arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);
    ts.ccr_740nm = (ASSUMED_TIMER_CLOCK_FREQ_MHZ *
                    MIN(forty_five_percent_of_current_period_us, on_time_us)) /
                   (ts.psc + 1);

    // make copy operation atomic
    CRITICAL_SECTION_ENTER(k);
    *new_settings = ts;
    CRITICAL_SECTION_EXIT(k);

    return RET_SUCCESS;
}
