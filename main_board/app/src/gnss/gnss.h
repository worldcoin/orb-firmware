#pragma once

#include "errors.h"

/**
 * @brief Initialize GNSS data parsing
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_INVALID_STATE if the UART device cannot be used
 */
ret_code_t
gnss_init(void);
