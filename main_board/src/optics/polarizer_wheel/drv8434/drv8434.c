/******************************************************************************
 * @file drv8434.c
 * @brief Source file for Texas Instruments DRV8434 stepper motor driver
 *
 * This file defines the application level interface functions for the DRV8434
 * such as initialize, configure, and control
 *
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434 datasheet SLOSE70 – DECEMBER 2020
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include "drv8434.h"

static DRV8434_Instance_t g_drv8434_instance;

ret_code_t
drv8434_init(const DRV8434_DriverCfg_t *cfg)
{
    // Wipe instance
    memset(&g_drv8434_instance, 0, sizeof(DRV8434_Instance_t));

    // Copy driver config over to runtime context
    memcpy(&g_drv8434_instance.driver_cfg, cfg, sizeof(DRV8434_DriverCfg_t));

    g_drv8434_instance.spi.rx_bufs.buffers = &g_drv8434_instance.spi.rx;
    g_drv8434_instance.spi.rx_bufs.count = 1;
    g_drv8434_instance.spi.tx_bufs.buffers = &g_drv8434_instance.spi.tx;
    g_drv8434_instance.spi.tx_bufs.count = 1;

    // Set internal SPI buffer pointers
    g_drv8434_instance.spi.rx.buf = g_drv8434_instance.spi.rx_buffer;
    g_drv8434_instance.spi.tx.buf = g_drv8434_instance.spi.tx_buffer;
    g_drv8434_instance.spi.rx.len =
        2; // sizeof(g_drv8434_instance.spi.rx_buffer);
    g_drv8434_instance.spi.tx.len =
        2; // sizeof(g_drv8434_instance.spi.tx_buffer);

    return RET_SUCCESS;
}

ret_code_t
drv8434_disable(void)
{
    DRV8434_CTRL2_REG_t ctrl2 = g_drv8434_instance.registers.ctrl2;
    ctrl2.EN_OUT = false;
    return drv8434_private_reg_write(DRV8434_REG_CTRL2_ADDR, ctrl2.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_enable(void)
{
    DRV8434_CTRL2_REG_t ctrl2 = g_drv8434_instance.registers.ctrl2;
    ctrl2.EN_OUT = true;
    return drv8434_private_reg_write(DRV8434_REG_CTRL2_ADDR, ctrl2.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_clear_fault(void)
{
    DRV8434_CTRL4_REG_t ctrl4 = g_drv8434_instance.registers.ctrl4;
    ctrl4.CLR_FLT = true;
    return drv8434_private_reg_write(DRV8434_REG_CTRL4_ADDR, ctrl4.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_unlock_control_registers(void)
{
    DRV8434_CTRL4_REG_t ctrl4 = g_drv8434_instance.registers.ctrl4;
    ctrl4.LOCK = DRV8434_REG_CTRL4_VAL_UNLOCK;
    return drv8434_private_reg_write(DRV8434_REG_CTRL4_ADDR, ctrl4.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_lock_control_registers(void)
{
    DRV8434_CTRL4_REG_t ctrl4 = g_drv8434_instance.registers.ctrl4;
    ctrl4.LOCK = DRV8434_REG_CTRL4_VAL_LOCK;
    return drv8434_private_reg_write(DRV8434_REG_CTRL4_ADDR, ctrl4.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_write_config(DRV8434_DeviceCfg_t const *const cfg)
{
    // Copy over desired device config to runtime context
    memcpy(&g_drv8434_instance.device_cfg, cfg, sizeof(DRV8434_DeviceCfg_t));

    // Proceed to write all config registers
    ret_code_t ret_val = RET_SUCCESS;

    ret_val = drv8434_private_reg_write(DRV8434_REG_CTRL4_ADDR, cfg->ctrl4.raw,
                                        &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    ret_val = drv8434_private_reg_write(DRV8434_REG_CTRL2_ADDR, cfg->ctrl2.raw,
                                        &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    ret_val = drv8434_private_reg_write(DRV8434_REG_CTRL3_ADDR, cfg->ctrl3.raw,
                                        &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    ret_val = drv8434_private_reg_write(DRV8434_REG_CTRL7_ADDR, cfg->ctrl7.raw,
                                        &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    return RET_SUCCESS;
}

ret_code_t
drv8434_read_config(void)
{
    // Proceed to write all config registers
    ret_code_t ret_val = RET_SUCCESS;
    ret_val =
        drv8434_private_reg_read(DRV8434_REG_CTRL2_ADDR, &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    ret_val =
        drv8434_private_reg_read(DRV8434_REG_CTRL3_ADDR, &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    ret_val =
        drv8434_private_reg_read(DRV8434_REG_CTRL4_ADDR, &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    ret_val =
        drv8434_private_reg_read(DRV8434_REG_CTRL7_ADDR, &g_drv8434_instance);
    if (ret_val) {
        return ret_val;
    }

    return RET_SUCCESS;
}

ret_code_t
drv8434_verify_config(void)
{
    // Proceed to write all config registers
    ret_code_t ret_val = RET_SUCCESS;

    if (g_drv8434_instance.device_cfg.ctrl2.raw !=
        g_drv8434_instance.registers.ctrl2.raw) {
        ret_val = RET_ERROR_INTERNAL;
    }

    if (g_drv8434_instance.device_cfg.ctrl3.raw !=
        g_drv8434_instance.registers.ctrl3.raw) {
        ret_val = RET_ERROR_INTERNAL;
    }

    if (g_drv8434_instance.device_cfg.ctrl4.raw !=
        g_drv8434_instance.registers.ctrl4.raw) {
        ret_val = RET_ERROR_INTERNAL;
    }

    if (g_drv8434_instance.device_cfg.ctrl7.raw !=
        g_drv8434_instance.registers.ctrl7.raw) {
        ret_val = RET_ERROR_INTERNAL;
    }

    return ret_val;
}

ret_code_t
drv8434_enable_stall_guard(void)
{
    DRV8434_CTRL5_REG_t ctrl5 = g_drv8434_instance.registers.ctrl5;
    ctrl5.EN_STL = true;
    return drv8434_private_reg_write(DRV8434_REG_CTRL5_ADDR, ctrl5.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_scale_current(enum DRV8434_TRQ_DAC_Val current)
{
    DRV8434_CTRL1_REG_t ctrl1 = g_drv8434_instance.registers.ctrl1;
    ctrl1.TRQ_DAC = current;
    return drv8434_private_reg_write(DRV8434_REG_CTRL1_ADDR, ctrl1.raw,
                                     &g_drv8434_instance);
}

ret_code_t
drv8434_get_register_data(DRV8434_Registers_t *reg)
{
    memcpy(reg, &g_drv8434_instance.registers, sizeof(DRV8434_Registers_t));
    return RET_SUCCESS;
}
