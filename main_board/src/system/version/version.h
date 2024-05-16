#pragma once

#include "mcu_messaging_main.pb.h"
#include <errors.h>
#include <stdint.h>

int
version_fw_send(uint32_t remote);

int
version_hw_send(uint32_t remote);

Hardware_OrbVersion
version_get_hardware_rev(void);

/**
 * @brief Get the front unit hardware version
 * @return The front unit hardware version
 */
Hardware_FrontUnitVersion
version_get_front_unit_rev(void);

/**
 * @brief Get the power board hardware version
 * @return The power board hardware version
 */
Hardware_PowerBoardVersion
version_get_power_board_rev(void);

/**
 * @brief Fetch hardware version by reading voltage and guessing mounted
 * resistors
 *
 * @return error code
 */
int
version_init(void);
