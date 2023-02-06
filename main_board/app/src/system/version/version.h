#ifndef ORB_MCU_MAIN_APP_VERSION_H
#define ORB_MCU_MAIN_APP_VERSION_H

#include "mcu_messaging.pb.h"
#include <errors.h>
#include <stdint.h>

int
version_fw_send(uint32_t remote);

int
version_hw_send(uint32_t remote);

/// Get hardware revision
/// \return RET_SUCCESS if successful, error code otherwise
ret_code_t
version_get_hardware_rev(Hardware *hw_version);

#endif // ORB_MCU_MAIN_APP_VERSION_H
