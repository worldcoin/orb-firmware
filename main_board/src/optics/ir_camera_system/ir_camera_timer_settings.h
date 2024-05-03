#pragma once

#include <errors.h>
#include <stdint.h>
#ifndef CONFIG_ZTEST
#include <zephyr/devicetree.h>
#endif

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US 5000
#define IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE 0.15
#if !defined(CONFIG_ZTEST)
BUILD_ASSERT(IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US == 5000 &&
                 IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE == 0.15,
             "These limits are to ensure that the hardware safty circuit is "
             "not triggered. If you change them please test with multiple orbs "
             "to ensure hardware safty circuit is not triggered.");
#endif // !defined(CONFIG_ZTEST)
#else
#define IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US 8000
#define IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE 0.25
#if !defined(CONFIG_ZTEST)
BUILD_ASSERT(IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US == 8000 &&
                 IR_CAMERA_SYSTEM_MAX_IR_LED_DUTY_CYCLE == 0.25,
             "These limits are to ensure that the hardware safty circuit is "
             "not triggered. If you change them please test with multiple orbs "
             "to ensure hardware safty circuit is not triggered.");
#endif // !defined(CONFIG_ZTEST)
#endif
#define IR_CAMERA_SYSTEM_MAX_FPS 60
#if defined(CONFIG_ZTEST)
#define TIMER_CLOCK_FREQ_MHZ 170L
#define TIMER_CLOCK_FREQ_HZ  (TIMER_CLOCK_FREQ_MHZ * 1000000)
#else
#define TIMER_CLOCK_FREQ_HZ                                                    \
    (DT_PROP(DT_NODELABEL(rcc), clock_frequency) /                             \
     DT_PROP(DT_NODELABEL(rcc), ahb_prescaler) /                               \
     DT_PROP(DT_NODELABEL(rcc), apb1_prescaler))
#define TIMER_CLOCK_FREQ_MHZ (TIMER_CLOCK_FREQ_HZ / 1000000)

BUILD_ASSERT(DT_PROP(DT_NODELABEL(rcc), apb1_prescaler) ==
                 DT_PROP(DT_NODELABEL(rcc), apb2_prescaler),
             "TIM2...7 are on APB1, TIM1,8,15,16,17,20 are on APB2, so they "
             "should have the same prescaler value.\n"
             "If this is not the case, use different macros for APB1 and APB2 "
             "prescaler values.");
#endif
#define TIMER_COUNTER_WIDTH_BITS 16

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
