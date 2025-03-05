#pragma once

#include "mirror_private.h"
#include <errors.h>

#if defined(CONFIG_BOARD_PEARL_MAIN)
ret_code_t
mirror_homing_one_end(const motors_refs_t *motor_handle,
                      const motor_t motor_id);
#endif

/**
 * Spawn threads to perform mirror homing
 *
 * @param motors pointers to motor handles
 * @return RET_SUCCESS on success
 */
ret_code_t
mirror_homing_async(motors_refs_t motors[MOTORS_COUNT]);
