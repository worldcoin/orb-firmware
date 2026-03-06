#pragma once

#include <main.pb.h>

/**
 * Initialize the persistent config module.
 *
 * Reads the latest valid configuration from the `config_partition`.
 * If no valid configuration is found, the in-memory config is set to
 * defaults (all zeros, i.e. BOOT_BUTTON).
 *
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_NOT_INITIALIZED unable to open flash area
 * @retval RET_ERROR_INTERNAL unable to erase flash area
 */
int
config_init(void);

/**
 * Get the current reboot behavior setting.
 *
 * @return current reboot behavior (BOOT_BUTTON if no config was persisted)
 */
orb_mcu_main_SetConfig_RebootBehavior
config_get_reboot_behavior(void);

/**
 * Persist a new reboot behavior setting to flash.
 *
 * The previous config record is freed before writing the new one so
 * that at most one record exists in the partition at any time.
 *
 * @param behavior  New reboot behavior to persist
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_NOT_INITIALIZED config module not initialized
 * @retval RET_ERROR_INTERNAL flash write failure
 */
int
config_set_reboot_behavior(orb_mcu_main_SetConfig_RebootBehavior behavior);
