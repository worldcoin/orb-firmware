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
#define TIMER_COUNTER_WIDTH_BITS    16
#define IR_CAMERA_SYSTEM_IR_LED_PSC (TIMER_CLOCK_FREQ_MHZ - 1) // 1us per tick

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#define IR_LED_TURN_ON_DELAY_US 50
#else
#define IR_LED_TURN_ON_DELAY_US 0
#endif

#define IR_LED_TIMER_START_DELAY_US         1 // 1 timer tick delay to start PWM pulse
#define CAMERA_TRIGGER_TIMER_START_DELAY_US (IR_LED_TURN_ON_DELAY_US + 1)

#if !defined(CONFIG_ZTEST)
BUILD_ASSERT(CAMERA_TRIGGER_TIMER_START_DELAY_US > 0 &&
                 IR_LED_TIMER_START_DELAY_US > 0,
             "XXX_TIMER_START_DELAY_US must be greater than 0, so that the "
             "output is low in idle state");
#endif

struct ir_camera_timer_settings {
    uint16_t fps;
    uint16_t master_psc;
    uint16_t master_arr; // full period to trigger the camera (1/FPS), in timer
                         // unit (FREQ/(PSC+1))
    uint16_t on_time_in_us;
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
