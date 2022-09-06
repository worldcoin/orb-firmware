/*******************************************************************************
 Copyright (C) 2016, STMicroelectronics International N.V.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of STMicroelectronics nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
 IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/**
 * @file   vl53l1_platform.c
 * @brief  Code function definitions for EwokPlus25 Platform Layer
 *         RANGING SENSOR VERSION
 *
 */

#include "vl53l1_platform.h"
#include <drivers/i2c.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(VL53L1X, CONFIG_SENSOR_LOG_LEVEL);

VL53L1_Error
VL53L1_WriteMulti(VL53L1_Dev_t *pdev, uint16_t index, uint8_t *pdata,
                  uint32_t count)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    int32_t status_int = 0;

    uint8_t buffer[count + 2];

    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;

    memcpy(&buffer[2], pdata, count);

    status_int =
        i2c_write(pdev->i2c, buffer, count + 2, pdev->i2c_slave_address);
    if (status_int < 0) {
        LOG_ERR("i2c_write failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    }

    return status;
}

VL53L1_Error
VL53L1_WrByte(VL53L1_Dev_t *pdev, uint16_t index, uint8_t data)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    int32_t status_int;
    uint8_t buffer[3];

    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;

    buffer[2] = data;

    status_int =
        i2c_write(pdev->i2c, buffer, sizeof buffer, pdev->i2c_slave_address);
    if (status_int < 0) {
        LOG_ERR("i2c_write failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    }

    return status;
}

VL53L1_Error
VL53L1_WrWord(VL53L1_Dev_t *pdev, uint16_t index, uint16_t data)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    int32_t status_int;
    uint8_t buffer[4];

    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;

    buffer[2] = data >> 8;
    buffer[3] = data & 0x00FF;

    status_int =
        i2c_write(pdev->i2c, buffer, sizeof buffer, pdev->i2c_slave_address);
    if (status_int < 0) {
        LOG_ERR("i2c_write failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    }

    return status;
}

VL53L1_Error
VL53L1_WrDWord(VL53L1_Dev_t *pdev, uint16_t index, uint32_t data)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    int32_t status_int;
    uint8_t buffer[6];

    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;

    buffer[2] = (data >> 24) & 0xFF;
    buffer[3] = (data >> 16) & 0xFF;
    buffer[4] = (data >> 8) & 0xFF;
    buffer[5] = (data >> 0) & 0xFF;

    status_int =
        i2c_write(pdev->i2c, buffer, sizeof buffer, pdev->i2c_slave_address);
    if (status_int < 0) {
        LOG_ERR("i2c_write failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    }

    return status;
}

VL53L1_Error
VL53L1_ReadMulti(VL53L1_Dev_t *pdev, uint16_t index, uint8_t *pdata,
                 uint32_t count)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    int32_t status_int;

    uint8_t buffer[2];
    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;

    status_int = i2c_write_read(pdev->i2c, pdev->i2c_slave_address, buffer, 2,
                                pdata, count);
    if (status_int < 0) {
        LOG_ERR("i2c_write_read failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    }

    return status;
}

VL53L1_Error
VL53L1_RdByte(VL53L1_Dev_t *pdev, uint16_t index, uint8_t *pdata)
{
    return VL53L1_ReadMulti(pdev, index, pdata, 1);
}

VL53L1_Error
VL53L1_RdWord(VL53L1_Dev_t *pdev, uint16_t index, uint16_t *pdata)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    uint8_t buffer[4];

    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;
    int32_t status_int = i2c_write_read(pdev->i2c, pdev->i2c_slave_address,
                                        &buffer, 2, &buffer[2], 2);
    if (status_int < 0) {
        LOG_ERR("i2c_write_read failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    } else {
        *pdata = (uint16_t)(((uint16_t)(buffer[2]) << 8) + (uint16_t)buffer[3]);
    }

    return status;
}

VL53L1_Error
VL53L1_RdDWord(VL53L1_Dev_t *pdev, uint16_t index, uint32_t *pdata)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;
    uint8_t buffer[6];

    buffer[0] = (uint8_t)(index >> 8) & 0xFF;
    buffer[1] = (uint8_t)index & 0xFF;
    int32_t status_int = i2c_write_read(pdev->i2c, pdev->i2c_slave_address,
                                        &buffer, 2, &buffer[2], 4);
    if (status_int < 0) {
        LOG_ERR("i2c_write_read failed (%d)", status_int);
        status = VL53L1_ERROR_CONTROL_INTERFACE;
    } else {
        *pdata = ((uint32_t)buffer[0] << 24) + ((uint32_t)buffer[1] << 16) +
                 ((uint32_t)buffer[2] << 8) + (uint32_t)buffer[3];
    }

    return status;
}

VL53L1_Error
VL53L1_WaitUs(VL53L1_Dev_t *pdev, int32_t wait_us)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;

    k_usleep(wait_us);

    return status;
}

VL53L1_Error
VL53L1_WaitMs(VL53L1_Dev_t *pdev, int32_t wait_ms)
{
    return VL53L1_WaitUs(pdev, wait_ms * 1000);
}

VL53L1_Error
VL53L1_GetTickCount(uint32_t *ptick_count_ms)
{
    VL53L1_Error status = VL53L1_ERROR_NONE;

    *ptick_count_ms = k_uptime_get_32();

    LOG_DBG("%5u ms;", *ptick_count_ms);

    return status;
}

VL53L1_Error
VL53L1_WaitValueMaskEx(VL53L1_Dev_t *pdev, uint32_t timeout_ms, uint16_t index,
                       uint8_t value, uint8_t mask, uint32_t poll_delay_ms)
{
    SUPPRESS_UNUSED_WARNING(poll_delay_ms);

    VL53L1_Error status = VL53L1_ERROR_NONE;
    uint32_t start_time_ms = 0;
    uint32_t current_time_ms = 0;
    uint8_t byte_value = 0;
    uint8_t found = 0;

    /* calculate time limit in absolute time */

    VL53L1_GetTickCount(&start_time_ms);
    pdev->new_data_ready_poll_duration_ms = 0;

    /* wait until value is found, timeout reached on error occurred */

    while ((status == VL53L1_ERROR_NONE) &&
           (pdev->new_data_ready_poll_duration_ms < timeout_ms) &&
           (found == 0)) {
        status = VL53L1_RdByte(pdev, index, &byte_value);

        if ((byte_value & mask) == value) {
            found = 1;
        }

        /* Update polling time (Compare difference rather than absolute to
        negate 32bit wrap around issue) */
        VL53L1_GetTickCount(&current_time_ms);
        pdev->new_data_ready_poll_duration_ms = current_time_ms - start_time_ms;
    }

    if (found == 0 && status == VL53L1_ERROR_NONE) {
        status = VL53L1_ERROR_TIME_OUT;
    }

    return status;
}
