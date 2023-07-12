#pragma once

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
 *
 * @return error code
 */
int
optics_init(void);
