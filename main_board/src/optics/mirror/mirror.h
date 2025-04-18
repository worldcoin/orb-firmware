#pragma once

#include <errors.h>
#include <zephyr/kernel.h>

#include "mirror_private.h"

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
 * @param delay_ms
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_BUSY if the queue is busy
 */
ret_code_t
mirror_set_angle_phi_async(int32_t angle_millidegrees, uint32_t delay_ms);

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
 * @param delay_ms
 * @retval RET_ERROR_INVALID_PARAM invalid value for `angle_millidegrees`
 * @retval RET_ERROR_BUSY if the queue is busy
 */
ret_code_t
mirror_set_angle_theta_async(int32_t angle_millidegrees, uint32_t delay_ms);

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
 * Run autohoming
 *
 * @param motor axis to home, ⚠️ unused on diamond
 * @return
 */
ret_code_t
mirror_autohoming(const motor_t *motor);

/**
 * Reset mirror to home position, given known coordinates
 * @return error code
 */
ret_code_t
mirror_go_home(void);

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
