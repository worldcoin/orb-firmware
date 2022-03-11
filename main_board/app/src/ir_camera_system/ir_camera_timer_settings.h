//
// Created by Cyril on 22/12/2021.
//

#ifndef FIRMWARE_TIMER_SETTINGS_H
#define FIRMWARE_TIMER_SETTINGS_H

#include <errors.h>
#include <stdint.h>

#define IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US 5000
#define IR_CAMERA_SYSTEM_MAX_FPS               60
#define ASSUMED_TIMER_CLOCK_FREQ               170000000

struct ir_camera_timer_settings {
    uint16_t fps;
    uint16_t psc;
    uint16_t arr; // full period to trigger the camera (1/FPS), in timer unit
                  // (FREQ/(PSC+1))
    uint16_t ccr; // on-time in timer unit (FREQ/(PSC+1))
    uint16_t on_time_in_us;
};

ret_code_t
timer_settings_from_on_time_us(
    uint16_t on_time_us,
    const struct ir_camera_timer_settings *current_settings,
    struct ir_camera_timer_settings *new_settings);

ret_code_t
timer_settings_from_fps(uint16_t fps,
                        const struct ir_camera_timer_settings *current_settings,
                        struct ir_camera_timer_settings *new_settings);

#endif // FIRMWARE_TIMER_SETTINGS_H
