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
#include "polarizer_wheel_defines.h"

/**
 * @brief Initialize the Polarizer Wheel runtime context
 *
 * @return Indicates successful initialization of runtime context
 *
 * @details This functon is responsible for initializing the Polarizer wheel
 *          run-time context.
 */

ret_code_t
polarizer_wheel_init(void);

/**
 * @brief Configure the Polarizer Wheel Motor and surrounding peripherals
 *
 * @return Indicates successful configuration of polarizer wheel
 *
 * @details This functon is responsible for configuring the Polarizer wheel
 *          operating parameters and surrounding peripherals.
 */

ret_code_t
polarizer_wheel_configure(void);

/**
 * @brief Get current position of the Polarizer Wheel
 *
 *
 * @return Returns positional enum
 *
 *
 */

Polarizer_Wheel_Position_t
polarizer_wheel_get_position(void);

/**
 * @brief Set current position of the Polarizer Wheel
 *
 *
 * @return Returns positional enum
 *
 *
 */

ret_code_t
polarizer_wheel_set_position(Polarizer_Wheel_Position_t position);
