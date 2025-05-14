#pragma once

/******************************************************************************
 * @file polarizer_wheel.h
 * @brief Header file for Polarizer wheel functions
 *
 * This file declares the interface layer between the polarizer wheel
 *application and the motor of choice
 *
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include "drv8434/drv8434.h"
#include "polarizer_wheel_datatypes.h"

/**
 * @brief Get current position of the Polarizer Wheel
 * @return Returns positional enum
 */
polarizer_wheel_position_t
polarizer_wheel_get_position(void);

/**
 * @brief Set current position of the Polarizer Wheel
 * @return Returns positional enum
 */
ret_code_t
polarizer_wheel_set_position(polarizer_wheel_position_t position);

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
 * @retval RET_ERROR_INVALID_STATE if already in progress
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
