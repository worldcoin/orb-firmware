#pragma once

#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * @brief Check if the distance in front is safe using the ToF sensor
 * @return true if the distance is safe, false otherwise
 */
bool
distance_is_safe(void);

/**
 * @brief Initialize 1D ToF sensor
 * @param distance_unsafe_cb Callback to be called when the measured distance
 *                            is unsafe for IR-Leds to be turned on.
 * @param mutex Mutex for sensor communication
 * @return RET_SUCCESS on success, RET_ERROR_INVALID_STATE if the ToF sensor
 * isn't ready
 */
int
tof_1d_init(void (*distance_unsafe_cb)(void), struct k_mutex *mutex);
