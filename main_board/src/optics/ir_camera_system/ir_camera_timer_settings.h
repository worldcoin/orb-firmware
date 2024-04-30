#pragma once

#include <errors.h>
#include <stdint.h>

#define IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US 5000
#define IR_CAMERA_SYSTEM_MAX_FPS               60
#define ASSUMED_TIMER_CLOCK_FREQ_MHZ           170
#define ASSUMED_TIMER_CLOCK_FREQ               (ASSUMED_TIMER_CLOCK_FREQ_MHZ * 1000000)

struct ir_camera_timer_settings {
    uint16_t fps;
    uint16_t psc;
    uint16_t arr; // full period to trigger the camera (1/FPS), in timer unit
                  // (FREQ/(PSC+1))
    uint16_t ccr; // on-time in timer unit (FREQ/(PSC+1)), 940nm & 850nm LEDs
    uint16_t ccr_740nm; // 740nm LEDs w/ different duty cycle constraints
    uint16_t on_time_in_us;
    uint32_t on_time_in_us_740nm;
};

void
timer_settings_print(const struct ir_camera_timer_settings *settings);

ret_code_t
timer_settings_from_on_time_us(
    uint16_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings);

ret_code_t
timer_settings_from_fps(uint16_t fps,
                        const struct ir_camera_timer_settings *current_settings,
                        struct ir_camera_timer_settings *new_settings);

/**
 * Compute CCR to apply on 740nm LEDs based on current settings
 * If on_time_us > 45% duty cycle, on_time_us is truncated
 * ⚠️ FPS must be set
 * @param on_time_us 740nm LED on time value in microseconds
 * @param current_settings Current setting with FPS value
 * @param new_settings Settings to use
 * @return err code
 */
ret_code_t
timer_740nm_ccr_from_on_time_us(
    uint32_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings);
