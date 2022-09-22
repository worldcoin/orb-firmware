#ifndef ORB_MCU_MAIN_APP_VERSION_H
#define ORB_MCU_MAIN_APP_VERSION_H

#include <errors.h>
#include <stdint.h>

enum hw_version_e {
    HW_VERSION_MAINBOARD_EV1 = 31,
    HW_VERSION_MAINBOARD_EV2 = 32,
    HW_VERSION_MAINBOARD_EV3 = 33,
};

int
version_send(uint32_t remote);

/// Get hardware revision
/// \return RET_SUCCESS if successful, error code otherwise
ret_code_t
version_get_hardware_rev(enum hw_version_e *hw_version);

#endif // ORB_MCU_MAIN_APP_VERSION_H
