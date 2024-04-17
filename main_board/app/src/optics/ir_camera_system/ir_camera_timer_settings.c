#include "ir_camera_timer_settings.h"
#ifdef CONFIG_ZTEST
#include <zephyr/logging/log.h>
#else
#include "logs_can.h"
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

    if (on_time_us == 0) {
        ret = RET_SUCCESS;
        ts.ccr = 0;
        ts.on_time_in_us = 0;
    } else if (on_time_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        ret = RET_ERROR_INVALID_PARAM;
    } else if (current_settings->fps == 0) {
        // save on-time duration for when FPS is set, but dont calculate ccr
        // TODO: when IR LED timers will be independent from
        // CAMERA_TRIGGER_TIMER this case can be handled in the default case
        ret = RET_SUCCESS;
        ts.on_time_in_us = on_time_us;
    } else if ((1e6 / current_settings->fps *
                IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE) < on_time_us) {
        ret = RET_ERROR_INVALID_PARAM;
        LOG_ERR(
            "On-time duration must not exceed %uµs for the current FPS setting",
            (uint16_t)(1e6 / current_settings->fps *
                       IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE));
    } else {
        ret = RET_SUCCESS;
        ts.on_time_in_us = on_time_us;
        uint32_t accuracy_us = (ts.psc + 1) / TIMER_CLOCK_FREQ_MHZ;
        if (accuracy_us > 1) {
            LOG_WRN("on-time duration accuracy: %uus", accuracy_us);
        }
        // calculate the ccr value for the new on-time duration
        ts.ccr = (TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);
        if (ts.ccr == 0) {
            // at least `accuracy_us` in the worst case scenario where
            // PSC=65535 only possible if on-time duration < accuracy in
            // this specific case: always < 10% duty cycle
            ts.ccr = 1;
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

static uint16_t
calc_ccr_740nm(const struct ir_camera_timer_settings *settings)
{
    // All we need to verify is that the on-time does not imply
    // a duty cycle > 45%.
    // If a duty cycle of > 45% would occur, then clamp it at 45%

    // would be 1_000_000 / fps to get us, but we run the LEDs at 2x frequency
    // of fps, so 1/2 it = 500000.0
    uint32_t forty_five_percent_of_current_period_us =
        (uint32_t)(500000.0 / settings->fps * 0.45);

    return (TIMER_CLOCK_FREQ_MHZ * MIN(forty_five_percent_of_current_period_us,
                                       settings->on_time_in_us_740nm)) /
           (settings->psc + 1);
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
        ts.psc = 0;
        ts.arr = 0;
        ts.ccr = 0;
        ts.ccr_740nm = 0;
    } else if (fps > IR_CAMERA_SYSTEM_MAX_FPS) {
        // Do nothing if we have an invalid FPS parameter.
        ret = RET_ERROR_INVALID_PARAM;
    } else {
        // Now we know that we have a valid FPS and either the 850nm/940nm LEDs
        // have an on-time value greater than zero, or we have a 740nm on-time
        // value greater than zero. The 850nm/940nm on-time settings have more
        // stringent constraints on their on-time, so IF `on_time_in_us` is >
        // 0, we ensure that it is valid before trying to compute ccr_740nm. If
        // the 740nm on-time setting only is >0, then we just calculate its CCR.
        // In any case, we need to compute PSC and ARR.
        ret = RET_SUCCESS;
        ts = *current_settings;
        ts.fps = fps;
        ts.psc =
            TIMER_CLOCK_FREQ_HZ / ((1 << TIMER_COUNTER_WITDH_BITS) * ts.fps);
        ts.arr = TIMER_CLOCK_FREQ_HZ / ((ts.psc + 1) * ts.fps);

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
            } else {
                ts.on_time_in_us = current_settings->on_time_in_us;
                ts.ccr =
                    (TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);
                if (ts.on_time_in_us_740nm != 0) {
                    // We don't calculate CCR for 740nm if the on-time for
                    // 850nm/940nm LEDs is > 0 but violates safety constraints.
                    // The Jetson will first have to correct the on-time for the
                    // 850nm/940nm LEDs and then try setting the FPS again.
                    ts.ccr_740nm = calc_ccr_740nm(&ts);
                }
            }
        } else if (ts.on_time_in_us_740nm != 0) {
            ts.ccr_740nm = calc_ccr_740nm(&ts);
        }

        if (ts.ccr == 0 && ts.on_time_in_us) {
            // at least `accuracy_us` in the worst case scenario where
            // PSC=65535 only possible if on-time duration < accuracy in
            // this specific case: always < 10% duty cycle
            ts.ccr = 1;
        }
        if (ts.ccr_740nm == 0 && ts.on_time_in_us_740nm) {
            // at least `accuracy_us` in the worst case scenario where
            // PSC=65535 only possible if on-time duration < accuracy in
            // this specific case: always < 10% duty cycle
            ts.ccr_740nm = 1;
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
    LOG_DBG("psc                 = %5u", settings->psc);
    LOG_DBG("arr                 = %5u", settings->arr);
    LOG_DBG("ccr                 = %5u", settings->ccr);
    LOG_DBG("ccr_740nm           = %5u", settings->ccr_740nm);
    LOG_DBG("on_time_in_us       = %5u", settings->on_time_in_us);
    LOG_DBG("on_time_in_us_740nm = %5u", settings->on_time_in_us_740nm);
}

ret_code_t
timer_740nm_ccr_from_on_time_us(
    uint32_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings)
{
    struct ir_camera_timer_settings ts = {0};

    ts = *current_settings;
    ts.on_time_in_us_740nm = on_time_us;

    // can't compute new settings if FPS is not set
    if (current_settings->fps != 0) {
        // apply psc and arr for current FPS
        // psc and arr only need to be re-computed in case fps was previously 0
        // but it is easier just to recalculate than to remember the last FPS
        // setting
        ts.psc =
            TIMER_CLOCK_FREQ_HZ / ((1 << TIMER_COUNTER_WITDH_BITS) * ts.fps);
        ts.arr = TIMER_CLOCK_FREQ_HZ / ((ts.psc + 1) * ts.fps);
        ts.ccr_740nm = calc_ccr_740nm(&ts);
    }

    // make copy operation atomic
    CRITICAL_SECTION_ENTER(k);
    *new_settings = ts;
    CRITICAL_SECTION_EXIT(k);

    return RET_SUCCESS;
}
