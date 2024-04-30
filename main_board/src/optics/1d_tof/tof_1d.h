#pragma once

#include <stdbool.h>

/**
 * @brief Check if the distance in front is safe using the ToF sensor
 * @return true if the distance is safe, false otherwise
 */
bool
distance_is_safe(void);

/**
 * @brief Initialize 1D ToF sensor
 * @return RET_SUCCESS on success, RET_ERROR_INVALID_STATE if the ToF sensor
 * isn't ready
 */
int
tof_1d_init(void);
