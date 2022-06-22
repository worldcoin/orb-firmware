#ifndef ORB_MCU_MAIN_APP_VERSION_H
#define ORB_MCU_MAIN_APP_VERSION_H

#include <errors.h>
#include <stdint.h>

int
version_send(void);

/// Get hardware revision
/// \return RET_SUCCESS if successful, error code otherwise
ret_code_t
version_get_hardware_rev(uint16_t *hw_version);

#endif // ORB_MCU_MAIN_APP_VERSION_H
