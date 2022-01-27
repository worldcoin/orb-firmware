//
// Created by Cyril on 22/12/2021.
//

#include "ir_camera_timer_settings.h"
#include <logging/log.h>
#include <sys/util.h>
LOG_MODULE_REGISTER(ir_camera_timer_settings);

#define MAX_PSC_DIV                  65536U
#define ASSUMED_TIMER_CLOCK_FREQ_MHZ 170

struct ir_camera_timer_settings
timer_settings_from_on_time_us(
    uint16_t on_time_us,
    const struct ir_camera_timer_settings *current_settings)
{
    struct ir_camera_timer_settings ts;

    if (on_time_us == 0) {
        // turn off IR/Cameras
        ts = (struct ir_camera_timer_settings){0};
    } else {
        // on-time duration must not be longer than
        // IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US
        ts.on_time_in_us =
            MIN(IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US, on_time_us);

        // find the FPS that matches the on-time duration
        ts.fps = 100000.0 / ts.on_time_in_us;

        ts.psc = ASSUMED_TIMER_CLOCK_FREQ / (MAX_PSC_DIV * ts.fps);
        ts.arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);
        ts.ccr =
            (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);
    }

    // if the FPS matching the on-time duration is lower than the current one
    // it means the on-time duration increased so that we need to change the FPS
    // to keep IR on-time duration < 5ms and 10% duty-cycle.
    // use computed settings if turning on the IR/Cameras system
    if (ts.fps < current_settings->fps || current_settings->fps == 0) {
        LOG_INF("FPS modified based on on-time duration: fps: was %u, now %u; "
                "for new on-time duration: %uus",
                current_settings->fps, ts.fps, ts.on_time_in_us);
    } else {
        // let's keep the previous FPS config
        ts.fps = current_settings->fps;

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
            uint16_t fps = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.arr);

            LOG_ERR(
                "Current PSC doesn't allow for FPS=%u, must change FPS to %u",
                ts.fps, fps);
            ts.fps = fps;
        } else {
            ts.arr = (uint16_t)arr;
        }

        uint32_t accuracy_us = (ts.psc + 1) / ASSUMED_TIMER_CLOCK_FREQ_MHZ;
        if (accuracy_us > 1) {
            LOG_WRN("on-time duration accuracy: %uus", accuracy_us);
        }
        ts.ccr =
            (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);
        if (ts.ccr == 0) {
            // at least `accuracy_us` in the worst case scenario where PSC=65535
            // only possible if on-time duration < accuracy
            // in this specific case: always < 10% duty cycle
            ts.ccr = 1;
        }
    }

    return ts;
}

struct ir_camera_timer_settings
timer_settings_from_fps(uint16_t fps,
                        const struct ir_camera_timer_settings *current_settings)
{
    struct ir_camera_timer_settings ts;

    if (fps == 0) {
        ts = (struct ir_camera_timer_settings){0};
    } else {
        ts.fps = MIN(fps, 1000);
        ts.on_time_in_us =
            MIN(100000.0 / ts.fps, IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US);
        ts.psc = ASSUMED_TIMER_CLOCK_FREQ / (MAX_PSC_DIV * ts.fps);
        ts.arr = ASSUMED_TIMER_CLOCK_FREQ / ((ts.psc + 1) * ts.fps);
        ts.ccr =
            (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);
    }

    // we use settings from passed fps if maximum on-time duration is shorter
    // than the current duration
    if (ts.on_time_in_us < current_settings->on_time_in_us ||
        current_settings->on_time_in_us == 0) {
        LOG_INF("On-time duration modified based on FPS: was %u, now %uus; for "
                "new fps: %u",
                current_settings->on_time_in_us, ts.on_time_in_us, ts.fps);
    } else if (current_settings->psc != ts.psc) {
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
        ts.ccr =
            (ASSUMED_TIMER_CLOCK_FREQ_MHZ * ts.on_time_in_us) / (ts.psc + 1);
        if (ts.ccr == 0) {
            // at least `accuracy_us` in the worst case scenario where PSC=65535
            // only possible if on-time duration < accuracy
            // in this specific case: always < 10% duty cycle
            ts.ccr = 1;
        }
    }

    return ts;
}
