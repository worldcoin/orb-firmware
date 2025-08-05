#pragma once

#include "mcu.pb.h"
#include <errors.h>
#include <stdint.h>

int
version_fw_send(uint32_t remote);

struct image_version
version_fw_get(void);

int
version_hw_send(uint32_t remote);

orb_mcu_Hardware
version_get(void);

/**
 * @brief Get the front unit hardware version
 * @return The front unit hardware version
 */
orb_mcu_Hardware_FrontUnitVersion
version_get_front_unit_rev(void);

/**
 * @brief Get the power board hardware version
 * @return The power board hardware version
 */
orb_mcu_Hardware_PowerBoardVersion
version_get_power_board_rev(void);

/**
 * @brief Get the reset board hardware version
 * @return The reset board hardware version
 */
orb_mcu_Hardware_ResetBoardVersion
version_get_reset_board_rev(void);

/**
 * @brief Fetch hardware version by reading voltage and guessing mounted
 * resistors
 *
 * @return error code
 */
int
version_init(void);
