#pragma once

#include "mcu_messaging.pb.h"
#include <errors.h>
#include <stdint.h>

int
version_fw_send(uint32_t remote);

int
version_hw_send(uint32_t remote);

Hardware_OrbVersion
version_get_hardware_rev(void);

/**
 * @brief Fetch hardware version by reading voltage and guessing mounted
 * resistors
 *
 * @return error code
 */
int
version_init(void);
