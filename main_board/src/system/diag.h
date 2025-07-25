#pragma once

#include "mcu.pb.h"
#include <errors.h>

/**
 * @brief Check if a new component status hasn't been sent yet
 * @return true if a new component status hasn't been sent yet
 */
bool
diag_has_data(void);

/**
 * @brief Send all hardware component statuses
 * @param remote Remote to send statuses to
 * @return error code
 */
ret_code_t
diag_sync(uint32_t remote);

/**
 * @brief Initialize the diagnostics system
 * Diag will keep the state of the hardware components set during initialization
 * and send it to the Jetson when requested
 */
void
diag_init(void);
