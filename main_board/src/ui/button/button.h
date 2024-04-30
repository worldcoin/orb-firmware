#pragma once

#include "errors.h"

int
button_uninit(void);

/**
 * @brief Initialize power button
 * Set up interrupt handling
 * @retval RET_SUCCESS button is init
 * @retval RET_ERROR_INVALID_STATE device not ready
 * @retval RET_ERROR_INTERNAL error configuring interrupt on button event
 */
int
button_init(void);
