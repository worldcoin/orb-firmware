#pragma once

#include <common.pb.h>
#include <stdint.h>

/**
 * @brief Send cone present status to remote
 * @param remote remote address
 */
void
ui_cone_present_send(uint32_t remote);

/**
 * @brief Initialize UI: all LEDs subsets
 * @return error code
 */
int
ui_init(const orb_mcu_Hardware *hw_version);
