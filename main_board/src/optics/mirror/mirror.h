#pragma once

#include <errors.h>
#include <zephyr/kernel.h>

/**
 * MOTOR_THETA_ANGLE:
 * If this motor is driven, then the mirror rotates around its horizontal axis.
 * This means if you look through the optics system you see a up/down movement
 * primarily (you also see a smaller amount of left/right movement, because the
 * movement of the mirror in one axis affects movement of the viewing angle in
 * both axes).
 *
 * MOTOR_PHI_ANGLE
 * If this motor is driven, then the mirror rotates around its vertical axis.
 * This means if you look through the optics system you see a left/right
 * movement. (If the current theta angle is different from 90° you also see a
 * small up/down movement).
 */
#if defined(CONFIG_BOARD_PEARL_MAIN)
typedef enum { MOTOR_THETA_ANGLE = 0, MOTOR_PHI_ANGLE, MOTORS_COUNT } motor_t;
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
typedef enum { MOTOR_PHI_ANGLE = 0, MOTOR_THETA_ANGLE, MOTORS_COUNT } motor_t;
#endif

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MIRROR_ANGLE_PHI_MIN_MILLIDEGREES (45000 - 9500)
#define MIRROR_ANGLE_PHI_MAX_MILLIDEGREES (45000 + 9500)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MIRROR_ANGLE_PHI_MIN_MILLIDEGREES (45000 - 13000)
#define MIRROR_ANGLE_PHI_MAX_MILLIDEGREES (45000 + 13000)
#endif
#define MIRROR_ANGLE_PHI_RANGE_MILLIDEGREES                                    \
    (MIRROR_ANGLE_PHI_MAX_MILLIDEGREES - MIRROR_ANGLE_PHI_MIN_MILLIDEGREES)

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MIRROR_ANGLE_THETA_MIN_MILLIDEGREES (90000 - 17500)
#define MIRROR_ANGLE_THETA_MAX_MILLIDEGREES (90000 + 17500)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MIRROR_ANGLE_THETA_MIN_MILLIDEGREES (90000 - 20000)
#define MIRROR_ANGLE_THETA_MAX_MILLIDEGREES (90000 + 20000)
#endif
#define MIRROR_ANGLE_THETA_RANGE_MILLIDEGREES                                  \
    (MIRROR_ANGLE_THETA_MAX_MILLIDEGREES - MIRROR_ANGLE_THETA_MIN_MILLIDEGREES)

#define MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES                                 \
    ((MIRROR_ANGLE_THETA_MIN_MILLIDEGREES +                                    \
      MIRROR_ANGLE_THETA_MAX_MILLIDEGREES) /                                   \
     2)
#define MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES                                   \
    ((MIRROR_ANGLE_PHI_MIN_MILLIDEGREES + MIRROR_ANGLE_PHI_MAX_MILLIDEGREES) / \
     2)

#define MOTOR_DRV_STATUS_STALLGUARD (1 << 24)
#define MOTOR_DRV_STATUS_STANDSTILL (1 << 31)

#define MOTOR_DRV_SW_MODE_SG_STOP (1 << 10)

/**
 * Set phi angle
 * @param angle_millidegrees
 * @retval RET_SUCCESS mirror successfully set to `angle_millidegrees`
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_NOT_INITIALIZED mirror control is not fully initialized
 * @retval RET_ERROR_INVALID_STATE critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
mirror_set_angle_phi(uint32_t angle_millidegrees);

/**
 * Queue job to call `mirror_set_angle_phi` later
 * @param angle_millidegrees
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_BUSY if the queue is busy
 */
ret_code_t
mirror_set_angle_phi_async(int32_t angle_millidegrees);

/**
 * Set theta angle
 * @param angle_millidegrees
 * @retval RET_SUCCESS mirror successfully set to `angle_millidegrees`
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_NOT_INITIALIZED mirror control is not fully initialized
 * @retval RET_ERROR_INVALID_STATE critical error detected during auto-homing:
 * positioning not available
 */
ret_code_t
mirror_set_angle_theta(uint32_t angle_millidegrees);

/**
 * Queue job to call `mirror_set_angle_theta` later
 * @param angle_millidegrees angle
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_BUSY if the queue is busy
 */
ret_code_t
mirror_set_angle_theta_async(int32_t angle_millidegrees);

/**
 * Set phi angle relative to current position
 * @param angle_millidegrees
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if the mirror cannot be moved to the requested
 * position given its current position
 */
ret_code_t
mirror_set_angle_phi_relative(int32_t angle_millidegrees);

/**
 * Set theta angle relative to current position
 * @param angle_millidegrees
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if the mirror cannot be moved to the requested
 * position given its current position
 */
ret_code_t
mirror_set_angle_theta_relative(int32_t angle_millidegrees);

/**
 * Perform auto-homing using motors' stall detection to detect both ends and go
 * to the center based on measured range.
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @param thread_ret optional, return a pointer to the thread info about thew
 * spawned auto-homing thread. This intended to be used for waiting on
 * auto-homing to finish
 * @retval RET_SUCCESS auto-homing has started
 * @retval RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * @retval RET_ERROR_FORBIDDEN auto-homing already in progress
 */
ret_code_t
mirror_auto_homing_stall_detection(motor_t motor, struct k_thread **thread_ret);

/**
 * Perform auto-homing by going to one end using the maximum number of steps
 * in the available mechanical range, then to center using half the range
 * This method does not allow for blockers detection
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @retval RET_SUCCESS auto-homing has started
 * @retval RET_ERROR_INTERNAL unable to spawn auto-homing thread
 * @retval RET_ERROR_BUSY auto-homing already in progress
 */
ret_code_t
mirror_auto_homing_one_end(motor_t motor);

/**
 * Check that auto-homing is in progress for at least one mirror
 * @return true if in progress, false otherwise
 */
bool
mirror_auto_homing_in_progress();

/**
 * @return current mirror angle theta
 */
uint32_t
mirror_get_theta_angle_millidegrees(void);

/**
 * @return current mirror angle phi
 */
uint32_t
mirror_get_phi_angle_millidegrees(void);

/**
 * @return true if auto-homing has been performed successfully
 */
bool
mirror_homed_successfully(void);

/**
 * Initialize mirror controlling system
 *
 * @retval RET_SUCCESS communication with motor controller is working. Spawned
 * threads to perform auto-homing procedure.
 * @retval RET_ERROR_INVALID_STATE SPI peripheral not ready
 * @retval RET_ERROR_OFFLINE cannot communicate with motor controller
 * @retval RET_ERROR_INTERNAL cannot initialize semaphores needed for
 * auto-homing
 */
ret_code_t
mirror_init(void);
