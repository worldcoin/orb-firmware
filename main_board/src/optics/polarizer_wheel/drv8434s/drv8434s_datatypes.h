#pragma once

/******************************************************************************
 * @file drv8434_datatypes.h
 * @brief Header file for Texas Instruments DRV8434 stepper motor driver
 *datatypes and runtime context
 *
 * This file defines driver runtime context structs as well
 *
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434 datasheet SLOSE70 – DECEMBER 2020
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#include "drv8434s_defines.h"
#include "drv8434s_registers.h"

// This is the DRV8434 Driver Configuration Struct (communication interface
// holder)
typedef struct {
    struct spi_dt_spec spi;
    const struct gpio_dt_spec *spi_cs_gpio;
} DRV8434S_DriverCfg_t;

// This is the subset of DRV8434 registers that are used for configuring the
// chip
typedef struct {
    DRV8434S_CTRL2_REG_t ctrl2;
    DRV8434S_CTRL3_REG_t ctrl3;
    DRV8434S_CTRL4_REG_t ctrl4;
    DRV8434S_CTRL7_REG_t ctrl7;
} DRV8434S_DeviceCfg_t;

// DRV8434 Register set struct
typedef struct {
    DRV8434S_FAULT_REG_t fault;
    DRV8434S_DIAGSTATUS1_REG_t diag_status1;
    DRV8434S_DIAGSTATUS2_REG_t diag_status2;
    DRV8434S_CTRL1_REG_t ctrl1;
    DRV8434S_CTRL2_REG_t ctrl2;
    DRV8434S_CTRL3_REG_t ctrl3;
    DRV8434S_CTRL4_REG_t ctrl4;
    DRV8434S_CTRL5_REG_t ctrl5;
    DRV8434S_CTRL6_REG_t ctrl6;
    DRV8434S_CTRL7_REG_t ctrl7;
    DRV8434S_CTRL8_REG_t ctrl8;
    DRV8434S_CTRL9_REG_t ctrl9;
} DRV8434S_Registers_t;

typedef struct {

    // Driver Config Copy
    DRV8434S_DriverCfg_t driver_cfg;

    // Device Config Copy ** This usually will not change after assignment
    // Can be used to check against in case of inadvertent register writes
    DRV8434S_DeviceCfg_t device_cfg;

    // Shadow register copy
    DRV8434S_Registers_t registers;

    // SPI Buffers
    struct {
        struct spi_buf rx;
        struct spi_buf_set rx_bufs;
        struct spi_buf tx;
        struct spi_buf_set tx_bufs;
        uint8_t rx_buffer[DRV8434S_SPI_BUFFER_SIZE_MAX];
        uint8_t tx_buffer[DRV8434S_SPI_BUFFER_SIZE_MAX];

        bool spi_busy;
    } spi;

    // Error handling
    struct {
        uint16_t spi_error;
        uint32_t general_error;
    } error;

    // Statistics
    struct {
        uint32_t transfers_completed;
        uint32_t spi_tranfer_time;
    } stats;

    // General Information
    struct {
        bool init_done;
    } general;

} DRV8434S_Instance_t;
