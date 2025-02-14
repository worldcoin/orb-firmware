/******************************************************************************
 * @file drv8434.h
 * @brief Header file for Texas Instruments DRV8434 stepper motor driver
 *
 * This file declares the application level interface functions for the DRV8434
 * such as initialize, configure, and control
 *
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434 datasheet SLOSE70 – DECEMBER 2020
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include "drv8434_private.h"

/**
 * @brief Initialize the DRV8434 run-time context
 *
 * @param cfg A reference to a driver configuration struct is passed in
 *
 * @return Indicates successful initialization of runtime context
 *
 * @details This functon is responsible for initializing the DRV8434 run-time
 *          context with the passed in driver configuration
 */

ret_code_t
drv8434_init(DRV8434_DriverCfg_t *cfg);

/**
 * @brief DRV8434 Disable Outputs of Half Bridges
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_disable(void);

/**
 * @brief DRV8434 Enable Outputs of Half Bridges
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_enable(void);

/**
 * @brief DRV8434 Write the ASIC specific device configuration
 *
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_write_config(DRV8434_DeviceCfg_t *cfg);

/**
 * @brief DRV8434 Read back the ASIC specific device configuration
 *
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_read_config(void);

/**
 * @brief DRV8434 Verify the ASIC specific device configuration
 *
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_verify_config(void);

/**
 * @brief DRV8434 Enable Stall Guard
 *
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_enable_stall_guard(void);

/**
 * @brief DRV8434 Enable Stall Guard
 *
 * @param percentage Percentage to scale current
 *
 * @return Indicates successful or unsuccessful operation
 *
 *
 */

ret_code_t
drv8434_scale_current(enum DRV8434_TRQ_DAC_Val current);

/**
 * @brief DRV8434 Get a copy of register data
 *
 * @param reg A reference to a register set struct
 *
 * @return Indicates successful or unsuccessful operation
 *
 * @details This functon is responsible for copying the run-time context's
 *          register set into the passed in reference
 */

ret_code_t
drv8434_get_register_data(DRV8434_Registers_t *reg);
