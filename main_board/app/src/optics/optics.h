#pragma once

#include "mcu_messaging.pb.h"
#include <stdbool.h>

/**
 * @brief Check whether the optics components are usable
 *
 * @details Check if the hardware eye safety circuitry has been tripped in case
 *  IR leds are too heavily used or if the distance of any object in front
 *  isn't too close.
 *
 * @return true if usable, false otherwise
 */
bool
optics_usable(void);

/**
 * @brief Initialize the optics components
 * @param *hw_version Mainboard hardware version
 * @return error code
 */
int
optics_init(const Hardware *hw_version);

/**
 * @brief Test that safety circuitry is responding
 *
 * Turn on IR LED subsets one by one, by driving GPIO pins, to check
 * that all lines are making the eye safety circuitry trip
 *
 * @retval RET_ERROR_ALREADY_INITIALIZED if already performed
 * @retval RET_ERROR_INTERNAL if failed to configure GPIOs
 * @retval RET_SUCCESS if successful
 */
int
optics_self_test(void);
