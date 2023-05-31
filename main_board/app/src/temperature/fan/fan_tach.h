#pragma once

/// Initialize the fan tach subsystem
/// \return RET_SUCCESS if successful, otherwise RET_ERROR_INTERNAL
ret_code_t
fan_tach_init(void);

/// Read the actual speed of the main fan in RPM
/// \return speed in rotations per minute if successful, otherwise UINT32_MAX
uint32_t
fan_tach_get_main_speed(void);

/// Read the actual speed of the aux fan in RPM
/// \return speed in rotations per minute if successful, otherwise UINT32_MAX
uint32_t
fan_tach_get_aux_speed(void);
