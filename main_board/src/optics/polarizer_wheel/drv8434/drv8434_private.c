/******************************************************************************
 * @file drv8434_private.c
 * @brief Private source file for Texas Instruments DRV8434 stepper motor driver
 *
 * This file defines the register read/write functions, and the lowest
 * level functions used to interface with the chip via the given SPI driver
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
 * @brief DRV8434 Register Operation Validity Check
 *
 * @param address Register Address to Read/Write
 * @param write   Write flag (true if write operation)
 *
 * @return True if allowed operation, false if not allowed
 *
 *
 */
static bool
validate_register_operation(uint8_t address, bool write)
{
    switch (address) {
    case DRV8434_REG_CTRL1_ADDR:
    case DRV8434_REG_CTRL2_ADDR:
    case DRV8434_REG_CTRL3_ADDR:
    case DRV8434_REG_CTRL4_ADDR:
    case DRV8434_REG_CTRL5_ADDR:
    case DRV8434_REG_CTRL6_ADDR:
    case DRV8434_REG_CTRL7_ADDR:
        return true;
        break;
    case DRV8434_REG_FAULT_ADDR:
    case DRV8434_REG_DIAGSTATUS1_ADDR:
    case DRV8434_REG_DIAGSTATUS2_ADDR:
    case DRV8434_REG_CTRL8_ADDR:
    case DRV8434_REG_CTRL9_ADDR:
        if (write) {
            return false;
        } else {
            return true;
        }
        break;
    default:
        return false;
        break;
    }
}

/**
 * @brief DRV8434 Shadow Register Population
 *
 * @param address Register Address to Read/Write
 * @param write   Write flag (true if write operation)
 *
 * @return True if allowed operation, false if not allowed
 *
 * @note No address or instance validation necessary here as that is done in the
 * caller
 */
static void
populate_shadow_register(uint8_t address, uint8_t data,
                         DRV8434_Instance_t *instance)
{
    switch (address) {
    case DRV8434_REG_CTRL1_ADDR:
        instance->registers.ctrl1.raw = data;
        break;
    case DRV8434_REG_CTRL2_ADDR:
        instance->registers.ctrl2.raw = data;
        break;
    case DRV8434_REG_CTRL3_ADDR:
        instance->registers.ctrl3.raw = data;
        break;
    case DRV8434_REG_CTRL4_ADDR:
        instance->registers.ctrl4.raw = data;
        break;
    case DRV8434_REG_CTRL5_ADDR:
        instance->registers.ctrl5.raw = data;
        break;
    case DRV8434_REG_CTRL6_ADDR:
        instance->registers.ctrl6.raw = data;
        break;
    case DRV8434_REG_CTRL7_ADDR:
        instance->registers.ctrl7.raw = data;
        break;
    case DRV8434_REG_CTRL8_ADDR:
        instance->registers.ctrl8.raw = data;
        break;
    case DRV8434_REG_CTRL9_ADDR:
        instance->registers.ctrl9.raw = data;
        break;
    case DRV8434_REG_FAULT_ADDR:
        instance->registers.fault.raw = data;
        break;
    case DRV8434_REG_DIAGSTATUS1_ADDR:
        instance->registers.diag_status1.raw = data;
        break;
    case DRV8434_REG_DIAGSTATUS2_ADDR:
        instance->registers.diag_status2.raw = data;
        break;
    default:
        break;
    }
    return;
}

ret_code_t
drv8434_private_reg_read(uint8_t address, DRV8434_Instance_t *instance)
{
    if (instance == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (!validate_register_operation(address, false)) {
        return RET_ERROR_INVALID_ADDR;
    }

    uint16_t tx_word = 0;

    // Load register address into tx word
    tx_word =
        tx_word | ((uint16_t)(address) << DRV8434_SPI_TX_ADDRESS_START_POS);

    // Load r/w bit into tx word
    tx_word = tx_word | ((uint16_t)(DRV8434_SPI_TX_RW_BIT_READ)
                         << DRV8434_SPI_TX_RW_START_POS);

    // Wipe TX and RX buffers before each operation
    memset(instance->spi.rx_buffer, 0, sizeof(instance->spi.rx_buffer));
    memset(instance->spi.tx_buffer, 0, sizeof(instance->spi.tx_buffer));

    instance->spi.tx_buffer[0u] = (tx_word >> 8) & DRV8434_SPI_TX_LSB_MASK;
    instance->spi.tx_buffer[1u] = tx_word & DRV8434_SPI_TX_LSB_MASK;

    gpio_pin_set_dt(instance->driver_cfg.spi_cs_gpio, 1);

    int ret = spi_transceive(instance->driver_cfg.spi.bus,
                             &instance->driver_cfg.spi.config,
                             &instance->spi.tx_bufs, &instance->spi.rx_bufs);

    gpio_pin_set_dt(instance->driver_cfg.spi_cs_gpio, 0);

    if (ret) {
        return RET_ERROR_BUSY;
    }

    instance->registers.fault.raw = instance->spi.rx_buffer[0u];

    populate_shadow_register(address, instance->spi.rx_buffer[1u], instance);

    return RET_SUCCESS;
}

ret_code_t
drv8434_private_reg_write(uint8_t address, uint8_t data,
                          DRV8434_Instance_t *instance)
{
    if (instance == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (!validate_register_operation(address, true)) {
        return RET_ERROR_INVALID_ADDR;
    }

    uint16_t tx_word = 0;

    // Load register address into tx word
    tx_word =
        tx_word | ((uint16_t)(address) << DRV8434_SPI_TX_ADDRESS_START_POS);

    // Load data into tx word
    tx_word = tx_word | (uint16_t)(data);

    // Wipe TX and RX buffers before each operation
    memset(instance->spi.rx_buffer, 0, sizeof(instance->spi.rx_buffer));
    memset(instance->spi.tx_buffer, 0, sizeof(instance->spi.tx_buffer));

    instance->spi.tx_buffer[0u] = (tx_word >> 8) & DRV8434_SPI_TX_LSB_MASK;
    instance->spi.tx_buffer[1u] = tx_word & DRV8434_SPI_TX_LSB_MASK;

    gpio_pin_set_dt(instance->driver_cfg.spi_cs_gpio, 1);

    int ret = spi_transceive(instance->driver_cfg.spi.bus,
                             &instance->driver_cfg.spi.config,
                             &instance->spi.tx_bufs, &instance->spi.rx_bufs);

    gpio_pin_set_dt(instance->driver_cfg.spi_cs_gpio, 0);

    if (ret) {
        return RET_ERROR_BUSY;
    }

    // Only care about fault status bits reported back
    instance->registers.fault.raw = instance->spi.rx_buffer[0u];

    return RET_SUCCESS;
}
