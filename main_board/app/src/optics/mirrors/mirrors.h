#pragma once

#include <errors.h>
#include <zephyr/kernel.h>

typedef enum {
    MIRROR_VERTICAL_ANGLE = 0,
    MIRROR_HORIZONTAL_ANGLE,
    MIRRORS_COUNT
} mirror_t;

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MIRRORS_ANGLE_HORIZONTAL_MIN (26000)
#define MIRRORS_ANGLE_HORIZONTAL_MAX (64000)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MIRRORS_ANGLE_HORIZONTAL_MIN (19000)
#define MIRRORS_ANGLE_HORIZONTAL_MAX (71000)
#endif
#define MIRRORS_ANGLE_HORIZONTAL_RANGE                                         \
    (MIRRORS_ANGLE_HORIZONTAL_MAX - MIRRORS_ANGLE_HORIZONTAL_MIN)

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MIRRORS_ANGLE_VERTICAL_MIN (-35000)
#define MIRRORS_ANGLE_VERTICAL_MAX (35000)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MIRRORS_ANGLE_VERTICAL_MIN (-40000)
#define MIRRORS_ANGLE_VERTICAL_MAX (40000)
#endif
#define MIRRORS_ANGLE_VERTICAL_RANGE                                           \
    (MIRRORS_ANGLE_VERTICAL_MAX - MIRRORS_ANGLE_VERTICAL_MIN)

#define AUTO_HOMING_VERTICAL_ANGLE_RESULT_MILLI_DEGREES                        \
    ((MIRRORS_ANGLE_VERTICAL_MIN + MIRRORS_ANGLE_VERTICAL_MAX) / 2)
#define AUTO_HOMING_HORIZONTAL_ANGLE_RESULT_MILLI_DEGREES                      \
    ((MIRRORS_ANGLE_HORIZONTAL_MIN + MIRRORS_ANGLE_HORIZONTAL_MAX) / 2)

#define MOTOR_DRV_STATUS_STALLGUARD (1 << 24)
#define MOTOR_DRV_STATUS_STANDSTILL (1 << 31)

#define MOTOR_DRV_SW_MODE_SG_STOP (1 << 10)

/**
 * Set horizontal angle
 * @param angle_millidegrees milli-degrees, accepted range is [26000;64000]
 * @retval RET_SUCCESS mirror successfully set to `angle_millidegrees`
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_NOT_INITIALIZED mirror control is not fully initialized
 * @retval RET_ERROR_INVALID_STATE critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
mirrors_angle_horizontal(int32_t angle_millidegrees);

/**
 * Queue job to call `mirrors_angle_horizontal` later
 * @param angle_millidegrees
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_BUSY if the queue is busy
 */
ret_code_t
mirrors_angle_horizontal_async(int32_t angle_millidegrees);

/**
 * Set vertical angle
 * @param angle_millidegrees milli-degrees, accepted range is [20000;40000]
 * @retval RET_SUCCESS mirror successfully set to passed angle
 * angle_millidegrees
 * @retval RET_ERROR_INVALID_PARAM invalid value for angle_millidegrees
 * @retval RET_ERROR_NOT_INITIALIZED mirror control is not fully initialized
 * @retval RET_ERROR_INVALID_STATE critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
mirrors_angle_vertical(int32_t angle_millidegrees);

/**
 * Queue job to call `mirrors_angle_vertical` later
 * @param angle_millidegrees angle
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_BUSY if the queue is busy
 */
ret_code_t
mirrors_angle_vertical_async(int32_t angle_millidegrees);

/**
 * Set horizontal angle relative to current position
 * @param angle_millidegrees angle
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if the mirror cannot be moved to the requested
 * position given its current position
 */
ret_code_t
mirrors_angle_horizontal_relative(int32_t angle_millidegrees);

/**
 * Set vertical angle relative to current position
 * @param angle_millidegrees angle
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if the mirror cannot be moved to the requested
 * position given its current position
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
 * @retval RET_SUCCESS auto-homing has started
 * @retval RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * @retval RET_ERROR_FORBIDDEN auto-homing already in progress
 */
ret_code_t
mirrors_auto_homing_stall_detection(mirror_t mirror,
                                    struct k_thread **thread_ret);

/**
 * Perform auto-homing by going to one end using the maximum number of steps
 * in the available mechanical range, then to center using half the range
 * This method does not allow for blockers detection
 * @param motor MOTOR_VERTICAL or MOTOR_HORIZONTAL
 * @retval RET_SUCCESS auto-homing has started
 * @retval RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * @retval RET_ERROR_BUSY auto-homing already in progress
 */
ret_code_t
mirrors_auto_homing_one_end(mirror_t mirror);

/**
 * Check that auto-homing is in progress for at least one mirror
 * @return true if in progress, false otherwise
 */
bool
mirrors_auto_homing_in_progress();

/**
 * @return mirror vertical position in milli-degrees
 */
int32_t
mirrors_get_vertical_position(void);

/**
 * @return mirror horizontal position in milli-degrees
 */
int32_t
mirrors_get_horizontal_position(void);

/**
 * @return true if auto-homing has been performed successfully
 */
bool
mirrors_homed_successfully(void);

/**
 * Initialize mirrors controlling system
 *
 * @retval RET_SUCCESS communication with motor controller is working. Spawned
 * threads to perform auto-homing procedure.
 * @retval RET_ERROR_INVALID_STATE SPI peripheral not ready
 * @retval RET_ERROR_OFFLINE cannot communicate with motor controller
 * @retval RET_ERROR_INTERNAL cannot initialize semaphores needed for
 * auto-homing
 */
ret_code_t
mirrors_init(void);
