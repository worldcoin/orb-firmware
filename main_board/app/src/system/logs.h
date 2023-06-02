#pragma once

/**
 * @brief Initialize the logging modules
 *
 * This function initializes the logging modules that are used by the
 *     mcu. One custom backend is used: the CAN bus
 *
 * @return RET_SUCCESS
 */
int
logs_init(void);
