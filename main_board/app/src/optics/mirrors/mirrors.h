#ifndef MIRRORS_H
#define MIRRORS_H

#include <errors.h>
#include <zephyr/kernel.h>

typedef enum {
    MIRROR_VERTICAL_ANGLE = 0,
    MIRROR_HORIZONTAL_ANGLE,
    MIRRORS_COUNT
} mirror_t;

#define MIRRORS_ANGLE_HORIZONTAL_MIN (26000)
#define MIRRORS_ANGLE_HORIZONTAL_MAX (64000)
#define MIRRORS_ANGLE_HORIZONTAL_RANGE                                         \
    (MIRRORS_ANGLE_HORIZONTAL_MAX - MIRRORS_ANGLE_HORIZONTAL_MIN)

#define MIRRORS_ANGLE_VERTICAL_MIN (-35000)
#define MIRRORS_ANGLE_VERTICAL_MAX (35000)
#define MIRRORS_ANGLE_VERTICAL_RANGE                                           \
    (MIRRORS_ANGLE_VERTICAL_MAX - MIRRORS_ANGLE_VERTICAL_MIN)

#define MOTOR_DRV_STATUS_STALLGUARD (1 << 24)
#define MOTOR_DRV_STATUS_STANDSTILL (1 << 31)

#define MOTOR_DRV_SW_MODE_SG_STOP (1 << 10)

/**
 * Set horizontal angle
 * @param angle_millidegrees milli-degrees, accepted range is [25000;65000]
 * @return
 *  * RET_SUCESS mirror successfully set to passed angle \c angle_millidegrees
 *  * RET_ERROR_INVALID_PARAM invalid value for \c angle_millidegrees
 *  * RET_ERROR_NOT_INITIALIZED mirror control is not fully initialized
 *  * RET_ERROR_INVALID_STATE critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
mirrors_angle_horizontal(int32_t angle_millidegrees);

/**
 * Set vertical angle
 * @param angle_millidegrees milli-degrees, accepted range is [20000;40000]
 * @return
 *  * RET_SUCESS mirror successfully set to passed angle \c angle_millidegrees
 *  * RET_ERROR_INVALID_PARAM invalid value for \c angle_millidegrees
 *  * RET_ERROR_NOT_INITIALIZED mirror control is not fully initialized
 *  * RET_ERROR_INVALID_STATE critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
mirrors_angle_vertical(int32_t angle_millidegrees);

/**
 * Set horizontal angle relative to current position
 * @param angle_millidegrees
 * @return
 */
ret_code_t
mirrors_angle_horizontal_relative(int32_t angle_millidegrees);

/**
 * Set vertical angle relative to current position
 * @param angle_millidegrees
 * @return
 */
ret_code_t
mirrors_angle_vertical_relative(int32_t angle_millidegrees);

/**
 * Perform auto-homing using motors' stall detection to detect both ends and go
 * to the center based on measured range.
 * @param motor MOTOR_VERTICAL or MOTOR_HORIZONTAL
 * @param thread_ret optional, return a pointer to the thread info about thew
 * spawned auto-homing thread. This intended to be used for waiting on
 * auto-homing to finish
 * @return
 * * RET_SUCCESS auto-homing has started
 * * RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * * RET_ERROR_FORBIDDEN auto-homing already in progress
 */
ret_code_t
mirrors_auto_homing_stall_detection(mirror_t mirror,
                                    struct k_thread **thread_ret);

/**
 * Perform auto-homing by going to one end using the maximum number of steps
 * in the available mechanical range, then to center using half the range
 * This method does not allow for blockers detection
 * @param motor MOTOR_VERTICAL or MOTOR_HORIZONTAL
 * @param thread_ret optional, return a pointer to the thread info about thew
 * spawned auto-homing thread. This intended to be used for waiting on
 * auto-homing to finish
 * @return
 * * RET_SUCCESS auto-homing has started
 * * RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * * RET_ERROR_FORBIDDEN auto-homing already in progress
 */
ret_code_t
mirrors_auto_homing_one_end(mirror_t mirror, struct k_thread **thread_ret);

/**
 * Check that auto-homing is in progress for at least one mirror
 * @return true if in progress, false otherwise
 */
bool
mirrors_auto_homing_in_progress();

/**
 * Initiliazing mirrors controlling system
 * @return
 * * RET_SUCCESS communication with motor controller is working. Spawned threads
 * to perform auto-homing procedure.
 * * RET_ERROR_INVALID_STATE SPI peripheral not ready
 * * RET_ERROR_OFFLINE cannot communicate with motor controller
 * * RET_ERROR_INTERNAL cannot initialize semaphores needed for auto-homing
 */
ret_code_t
mirrors_init(void);

bool
mirrors_homed_successfully(void);

#endif // MIRRORS_H
