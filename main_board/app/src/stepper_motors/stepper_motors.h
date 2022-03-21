#ifndef STEPPER_MOTORS_H
#define STEPPER_MOTORS_H

#include <errors.h>
#include <zephyr.h>

typedef enum { MOTOR_VERTICAL = 0, MOTOR_HORIZONTAL, MOTOR_COUNT } motor_t;

#define MOTORS_ANGLE_HORIZONTAL_MIN (25000)
#define MOTORS_ANGLE_HORIZONTAL_MAX (65000)

#define MOTORS_ANGLE_VERTICAL_MIN (-20000)
#define MOTORS_ANGLE_VERTICAL_MAX (20000)

#define MOTOR_DRV_STATUS_STALLGUARD (1 << 24)
#define MOTOR_DRV_STATUS_STANDSTILL (1 << 31)

#define MOTOR_DRV_SW_MODE_SG_STOP (1 << 10)

/**
 * Set horizontal angle
 * @param angle_millidegrees millidegrees, accepted range is [25000;65000]
 * @return
 *  * RET_SUCESS motor successfully set to passed angle \c angle_millidegrees
 *  * RET_ERROR_INVALID_PARAM invalid value for \c angle_millidegrees
 *  * RET_ERROR_NOT_INITIALIZED motor is not fully initialized
 *  * RET_ERROR_INVALID_STATE motor critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
motors_angle_horizontal(int32_t angle_millidegrees);

/**
 * Set vertical angle
 * @param angle_millidegrees milli-degrees, accepted range is [20000;40000]
 * @return
 *  * RET_SUCESS motor successfully set to passed angle \c angle_millidegrees
 *  * RET_ERROR_INVALID_PARAM invalid value for \c angle_millidegrees
 *  * RET_ERROR_NOT_INITIALIZED motor is not fully initialized
 *  * RET_ERROR_INVALID_STATE motor critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
motors_angle_vertical(int32_t angle_millidegrees);

/**
 * Perform auto-homing
 * @param motor (0 or 1)
 * @param thread_ret return a pointer to the thread info about thew spawned
 * auto-homing thread. This intended to be used for waiting on auto-homing to
 * finish
 * @return
 * * RET_SUCCESS auto-homing has started
 * * RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * * RET_ERROR_FORBIDDEN auto-homing already in progress
 */
ret_code_t
motors_auto_homing(motor_t motor, struct k_thread **thread_ret);

/**
 * Initiliazing motors
 * @return
 * * RET_SUCCESS communication with motor controller is working. Spawned threads
 * to perform auto-homing procedure.
 * * RET_ERROR_BUSY SPI peripheral not ready
 * * RET_ERROR_INVALID_STATE cannot communicate with motor controller
 */
ret_code_t
motors_init(void);

bool
motors_homed_successfully(void);

#endif // STEPPER_MOTORS_H
