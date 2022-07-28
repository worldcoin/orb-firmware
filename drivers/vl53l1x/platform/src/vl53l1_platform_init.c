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
 * @file   vl53l1_platform_init.c
 * @brief  EwokPlus25 comms and GPIO init
 *
 */


#include "vl53l1_ll_def.h"
#include "vl53l1_platform.h"
#include "vl53l1_platform_init.h"


VL53L1_Error VL53L1_platform_init(
	VL53L1_Dev_t *pdev,
	uint8_t       i2c_slave_address,
	uint8_t       comms_type,
	uint16_t      comms_speed_khz)
{
	/*
	 * Initialise comms, GPIOs (xshutdown, ncs, EVK power regulator enable)
	 * and reset Device
	 */

	VL53L1_Error status = VL53L1_ERROR_NONE;

	/* remember comms settings */

	pdev->i2c_slave_address = i2c_slave_address;
	pdev->comms_type        = comms_type;
	pdev->comms_speed_khz   = comms_speed_khz;

	if (status == VL53L1_ERROR_NONE) /*lint !e774 always true*/
		status =
			VL53L1_CommsInitialise(
				pdev,
				pdev->comms_type,
				pdev->comms_speed_khz);

	/* Ensure device is in reset */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_GpioXshutdown(0);

	/* disable the platform regulators */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_GpioPowerEnable(0);

	/* set the NCS pin for I2C mode */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_GpioCommsSelect(0);

	/* 1ms Wait to ensure XSHUTD / NCS are in the right state */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WaitUs(pdev, 1000);

	/* enable the platform regulators */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_GpioPowerEnable(1);

	/* 1ms Wait for power regs to settle */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WaitUs(pdev, 1000);

	/* finally, bring the device out of reset */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_GpioXshutdown(1);

	/*  Wait 100us for device to exit reset */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WaitUs(pdev, 100);

	return status;
}


VL53L1_Error VL53L1_platform_terminate(
		VL53L1_Dev_t *pdev)
{
	/*
	 * Puts the device into reset, disables EVK power regulator
	 * and closes comms
	 */

	VL53L1_Error status = VL53L1_ERROR_NONE;

	/* put device in reset */
	if (status == VL53L1_ERROR_NONE) /*lint !e774 always true*/
		status = VL53L1_GpioXshutdown(0);

	/* disable the platform regulators */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_GpioPowerEnable(0);

	/* close the comms interfaces */
	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_CommsClose(pdev);

	return status;
}


