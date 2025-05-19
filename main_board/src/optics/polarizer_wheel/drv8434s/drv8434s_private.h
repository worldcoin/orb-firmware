/******************************************************************************
 * @file drv8434s_private.h
 * @brief Private header file for Texas Instruments DRV8434 stepper motor driver
 *
 * This file declares the register read/write functions, and the lowest
 * level functions used to interface with the chip via the given SPI driver
 *
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434 datasheet SLOSE70 – DECEMBER 2020
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include "errors.h"

#include "drv8434s_datatypes.h"

/**
 * @brief DRV8434 Register read function
 *
 * @param address Register Address to Read
 * @param drv8434 instance Description of second parameter
 *
 * @return Description of return value
 *
 * @details
 * This functon is responsible for a reading a register
 * on the chip using a BLOCKING SPI transfer function
 *
 */

ret_code_t
drv8434s_private_reg_read(uint8_t address, DRV8434S_Instance_t *instance);

/**
 * @brief DRV8434 Register write function
 *
 * @param address Register Address to Write
 * @param data    Data to write
 * @param drv8434 instance Description of second parameter
 *
 * @return Description of return value
 *
 * @details
 * This functon is responsible for a writing a register
 * on the chip using a BLOCKING SPI transfer function
 *
 */

ret_code_t
drv8434s_private_reg_write(uint8_t address, uint8_t data,
                           DRV8434S_Instance_t *instance);
