#pragma once

/******************************************************************************
 * @file polarizer_wheel_defines.h
 * @brief Defines for Polarizer Wheel Mechanical Setup, Diamond EVT
 *
 * This file contains various macro definitions for the Polarizer Wheel
 * mechanical setup, Diamond EVT, and other relevant parameters
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

// Maximum number of notches needed to be encountered to finish homing
#define POLARIZER_WHEEL_ENCODER_MAX_HOMING_NOTCHES 5
// Minimum number of notches needed to be encountered to start homing
#define POLARIZER_WHEEL_ENCODER_HOMING_NOTCHES_COMPARE_MIN 2

// TODO get this correctly from the device tree
#define POLARIZER_STEP_CHANNEL 2

/* Following is a calculation how to arrive at different PWM frequencies and
 * step counts for the polarizer wheel application. Motor Used: 26M048B1B,
 * datasheet found here:
 * https://www.digikey.de/en/products/detail/portescap/26M048B1B/417812 Motor
 * Driver used: DRV8434 Motor Step Angle: 7.5 degrees Microstepping configured
 * in DRV8434: 1/128 Calculation: (360 degrees / 7.5 degrees per step) * 128
 * (microsteps) = 6144 steps/rev 6144 steps/rev / 3 = 2048 steps per filter
 * wheel position For Auto-homing, we can move slow, To move 120 degrees in 1.5
 * seconds, PWM frequency = 2048 / 1.5 = 1365.33 Hz
 */
#define POLARIZER_WHEEL_SPIN_PWN_FREQUENCY_AUTO_HOMING 1366

// Number of steps to move during auto-homing when attempting to find notches
// This is intentionally set slightly higher than the max steps needed to move
// 120 degrees to account for any potential errors in the step count
#define POLARIZER_WHEEL_STEPS_AUTOHOMING 2200

// Number of correction steps to issue after homing is complete and last notch
// is found
// TODO: Get this information from the ME team
#define POLARIZER_WHEEL_STEPS_CORRECTION 100

// Number of steps required to move the wheel from one filter to the next
// This is 120 degrees
#define POLARIZER_WHEEL_STEPS_PER_1_FILTER_WHEEL_POSITION 2048

// Number of steps required to move the wheel by 2 filter positions
// This is 240 degrees
#define POLARIZER_WHEEL_STEPS_PER_2_FILTER_WHEEL_POSITION 4096
