#ifndef STEPPER_MOTORS_H
#define STEPPER_MOTORS_H

#include <errors.h>

#define READ 0
#define WRITE (1 << 7)

#define TMC5041_IC_VERSION 0x10

#define TMC5041_REG_GCONF       0x00
#define REG_INPUT               0x04

/**
 * Set horizontal angle
 * @param x
 * @return
 *  * RET_SUCESS motor successfully set to passed angle \c x
 *  * RET_ERROR_NOT_INITIALIZED motor is not fully initialized
 *  * RET_ERROR_INVALID_STATE motor critical error detected during auto-homing: positioning not available
 */
ret_code_t motors_angle_horizontal(int8_t x);

/**
 * Set vertical angle
 * @param x
 * @return
 *  * RET_SUCESS motor successfully set to passed angle \c x
 *  * RET_ERROR_NOT_INITIALIZED motor is not fully initialized
 *  * RET_ERROR_INVALID_STATE motor critical error detected during auto-homing: positioning not available
 */
ret_code_t motors_angle_vertical(int8_t x);

/**
 * Perform auto-homing
 * @param motor (0 or 1)
 * @return
 * * RET_SUCCESS auto-homing has started
 * * RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * * RET_ERROR_FORBIDDEN auto-homing already in progress
 */
ret_code_t motors_auto_homing(uint8_t motor);

/**
 * Initiliazing motors
 * @return
 * * RET_SUCCESS communication with motor controller is working. Spawned threads to perform auto-homing procedure.
 * * RET_ERROR_BUSY SPI peripheral not ready
 * * RET_ERROR_INVALID_STATE cannot communicate with motor controller
 */
ret_code_t motors_init(void);

#endif // STEPPER_MOTORS_H
