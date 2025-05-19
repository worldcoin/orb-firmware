#pragma once

/******************************************************************************
 * @file polarizer_wheel.h
 * @brief Header file for Polarizer wheel functions
 *
 * This file declares the interface layer between the polarizer wheel
 * application and the motor of choice
 *
 ******************************************************************************/

#include <errors.h>
#include <stdint.h>

/*
 * - Motor Used: 26M048B1B, datasheet found here:
 *   https://www.digikey.de/en/products/detail/portescap/26M048B1B/417812
 * - Motor Driver used: DRV8434s
 * - Motor Step Angle: 7.5 degrees
 *   Microstepping configured (128 microsteps / step)
 *
 * (360 degrees / 7.5 degrees per step) * 128 (microsteps)
 *   = 6144 steps/revolution
 */
#define POLARIZER_WHEEL_DEGREES_PER_STEP    7.5
#define POLARIZER_WHEEL_MICROSTEPS_PER_STEP 128
#define POLARIZER_WHEEL_STEPS_PER_TURN                                         \
    (int)(360.0 / POLARIZER_WHEEL_DEGREES_PER_STEP)

#define POLARIZER_WHEEL_MICROSTEPS_360_DEGREES                                 \
    (POLARIZER_WHEEL_STEPS_PER_TURN * POLARIZER_WHEEL_MICROSTEPS_PER_STEP)
#define POLARIZER_WHEEL_MICROSTEPS_120_DEGREES                                 \
    (POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 3)

#define POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_3SEC_PER_TURN                       \
    (POLARIZER_WHEEL_MICROSTEPS_360_DEGREES / 3)
#define POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_1SEC_PER_TURN                       \
    (POLARIZER_WHEEL_MICROSTEPS_360_DEGREES)

#define POLARIZER_MICROSTEPS_PER_SECOND(ms)                                    \
    (POLARIZER_WHEEL_MICROSTEPS_360_DEGREES * 1000 / ms)

// from notch edge to notch center
#define POLARIZER_WHEEL_MICROSTEPS_NOTCH_EDGE_TO_CENTER 100

enum polarizer_wheel_angle_e {
    POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE = 0,
    POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE = 1200,
    POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE = 2400,
};

/**
 * Set angle to polarizer wheel
 * 0º being the passthrough glass once homing is completed
 *
 * @param frequency microsteps/sec, maximum is
 * POLARIZER_WHEEL_SPIN_PWM_FREQUENCY_1SEC_PER_TURN
 * @param angle [0, 3600], use `polarizer_wheel_angle_e` for predefined angles
 * @return RET_SUCCESS on succes, RET_ERROR_INVALID_PARAM if out of range
 */
ret_code_t
polarizer_wheel_set_angle(uint32_t frequency, uint32_t angle);

/**
 * Spawn homing thread
 *
 * Below is a representation of the notches on the wheel (encoder):
 * |--->--|---->-|----|--| (and back)
 * 0------1------2----3--0 notch number
 *
 * The goal is to detect the 2-dash long number of steps between 3 and 0 and to
 * stop at notch #0, with the wheel spinning from left to right.
 *
 * Homing procedure:
 * 1. The wheel spins up to a first notch, that can be anywhere, very close or
 * not, since the initial wheel position is unknown.
 * 2. Once that notch is detected, the wheel spins until a short number of steps
 * between two notches is detected (2-dash segment above), meaning the home
 * position (0) is reached.
 * 3. The **edge** of notch #0 is reached but the wheel needs to be centered on
 * that notch, so a few more steps are performed to go home.
 *
 * The maximum number of steps perform is if the initial position is between
 * notch #3 and #0 (more than one entire revolution)
 *
 * @retval RET_ERROR_BUSY if already in progress
 * @retval RET_ERROR_NOT_INITIALIZED if module not initialized or no wheel
 * detected
 * @retval RET_SUCCESS spawned successfully
 */
ret_code_t
polarizer_wheel_home_async(void);

/**
 * @brief Initialize the polarizer wheel
 * Spawn homing thread once completed
 *
 * @return Indicates successful initialization
 */
ret_code_t
polarizer_wheel_init(void);
