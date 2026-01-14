/******************************************************************************
 * @file drv8434s.h
 * @brief Header file for Texas Instruments DRV8434S stepper motor driver
 *
 * This file declares the application level interface functions for the DRV8434S
 * such as initialize, configure, and control.
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434S datasheet SLOSE70 – DECEMBER 2020
 *
 * @author Srikar Chintapalli
 ******************************************************************************/

#include "drv8434s_private.h"

/**
 * @brief Initialize the DRV8434S run-time context
 *
 * This function is responsible for initializing the DRV8434S run-time
 * context with the passed in driver configuration.
 *
 * @param cfg Pointer to the driver configuration struct
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_init(const DRV8434S_DriverCfg_t *cfg);

/**
 * @brief Disable outputs of half bridges
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_disable(void);

/**
 * @brief Enable outputs of half bridges
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_enable(void);

/**
 * @brief Write the device configuration to the DRV8434S
 *
 * @param cfg Pointer to the device configuration struct
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_write_config(DRV8434S_DeviceCfg_t const *const cfg);

/**
 * @brief Read back the device configuration from the DRV8434S
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_read_config(void);

/**
 * @brief Verify the device configuration matches expected values
 *
 * @return ret_code_t RET_SUCCESS if configuration matches, error code otherwise
 */
ret_code_t
drv8434s_verify_config(void);

/**
 * @brief Enable stall guard detection
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_enable_stall_guard(void);

/**
 * @brief Scale the motor drive current
 *
 * @param current The torque DAC value to set (determines current scaling)
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_scale_current(enum DRV8434S_TRQ_DAC_Val current);

/**
 * @brief Get a copy of the current register data
 *
 * Copies the run-time context's register set into the provided struct.
 *
 * @param reg Pointer to the register struct to populate
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_get_register_data(DRV8434S_Registers_t *reg);

/**
 * @brief Clear any fault conditions
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_clear_fault(void);

/**
 * @brief Unlock the control registers for writing
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_unlock_control_registers(void);

/**
 * @brief Lock the control registers to prevent modification
 *
 * @return ret_code_t RET_SUCCESS on success, error code otherwise
 */
ret_code_t
drv8434s_lock_control_registers(void);
