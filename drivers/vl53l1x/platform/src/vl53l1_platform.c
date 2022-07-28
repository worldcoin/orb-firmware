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
#include <windows.h>

#include <stdio.h>      // sprintf(), vsnprintf(), printf()
#include <stdint.h>
#include <string.h>     // strncpy(), strnlen()

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#include "vl53l1_platform.h"
#include "vl53l1_platform_log.h"

#ifdef PAL_EXTENDED
	#include "vl53l1_register_strings.h"
#else
	#define VL53L1_get_register_name(a,b)
#endif

#include "ranging_sensor_comms.h"
#include "power_board_defs.h"

// flag to indicate if power board has been accessed
const uint32_t _power_board_in_use = 0;

// flag to indicate if we can use the extended voltage ranges (not laser safe!)
uint32_t _power_board_extended = 0;

// cache of the comms type flag
uint8_t global_comms_type = 0;

#define  VL53L1_COMMS_CHUNK_SIZE  56
#define  VL53L1_COMMS_BUFFER_SIZE 64

#define GPIO_INTERRUPT          RS_GPIO62
#define GPIO_POWER_ENABLE       RS_GPIO60
#define GPIO_XSHUTDOWN          RS_GPIO61
#define GPIO_SPI_CHIP_SELECT    RS_GPIO51

/*!
 *  The intent of this Abstraction layer is to provide the same API
 *  to the underlying SystemVerilog tasks as the C driver will have
 *  to ST Comms DLL's for the talking to Ewok via the USB + STM32
 *  or if the C-driver is implemented directly on the STM32
 */

#define trace_print(level, ...) \
	_LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_PLATFORM, \
	level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)

#define trace_i2c(...) \
	_LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_NONE, \
	VL53L1_TRACE_LEVEL_NONE, VL53L1_TRACE_FUNCTION_I2C, ##__VA_ARGS__)


VL53L1_Error VL53L1_CommsInitialise(
	VL53L1_Dev_t *pdev,
	uint8_t       comms_type,
	uint16_t      comms_speed_khz)
{
	VL53L1_Error status = VL53L1_ERROR_NONE;
	char comms_error_string[ERROR_TEXT_LENGTH];

	SUPPRESS_UNUSED_WARNING(pdev);
	SUPPRESS_UNUSED_WARNING(comms_speed_khz);

	global_comms_type = comms_type;

	if(global_comms_type == VL53L1_I2C)
	{
		if ((CP_STATUS)RANGING_SENSOR_COMMS_Init_CCI(0, 0, 0) != CP_STATUS_OK)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_CommsInitialise: RANGING_SENSOR_COMMS_Init_CCI() failed\n");
			trace_i2c(comms_error_string);
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}
	}
	else if(global_comms_type == VL53L1_SPI)
	{
		if ((CP_STATUS)RANGING_SENSOR_COMMS_Init_SPI_V2W8(0, 0, 0) != CP_STATUS_OK)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_CommsInitialise: RANGING_SENSOR_COMMS_Init_SPI_V2W8() failed\n");
			trace_i2c(comms_error_string);
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}
	}
	else
	{
		trace_i2c("VL53L1_CommsInitialise: Comms must be one of VL53L1_I2C or VL53L1_SPI\n");
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}

	return status;
}


VL53L1_Error VL53L1_CommsClose(
	VL53L1_Dev_t *pdev)
{
	VL53L1_Error status = VL53L1_ERROR_NONE;
	char comms_error_string[ERROR_TEXT_LENGTH];

	SUPPRESS_UNUSED_WARNING(pdev);

	if(global_comms_type == VL53L1_I2C)
	{
		if((CP_STATUS)RANGING_SENSOR_COMMS_Fini_CCI() != CP_STATUS_OK)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_CommsClose: RANGING_SENSOR_COMMS_Fini_CCI() failed\n");
			trace_i2c(comms_error_string);
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}
	}
	else if(global_comms_type == VL53L1_SPI)
	{
		if((CP_STATUS)RANGING_SENSOR_COMMS_Fini_SPI_V2W8() != CP_STATUS_OK)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_CommsClose: RANGING_SENSOR_COMMS_Fini_SPI_V2W8() failed\n");
			trace_i2c(comms_error_string);
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}
	}
	else
	{
		trace_i2c("VL53L1_CommsClose: Comms must be one of VL53L1_I2C or VL53L1_SPI\n");
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}

	return status;
}

/*
 * ----------------- COMMS FUNCTIONS -----------------
 */

VL53L1_Error VL53L1_WriteMulti(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint8_t      *pdata,
	uint32_t      count)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint32_t     position       = 0;
	uint32_t     data_size      = 0;

	char   comms_error_string[ERROR_TEXT_LENGTH];

	_LOG_STRING_BUFFER(register_name);
	_LOG_STRING_BUFFER(value_as_str);

	if(global_comms_type == VL53L1_I2C)
	{
		for(position=0; position<count; position+=VL53L1_COMMS_CHUNK_SIZE)
		{
			if (count > VL53L1_COMMS_CHUNK_SIZE)
			{
				if((position + VL53L1_COMMS_CHUNK_SIZE) > count)
				{
					data_size = count - position;
				}
				else
				{
					data_size = VL53L1_COMMS_CHUNK_SIZE;
				}
			}
			else
			{
				data_size = count;
			}

			if (status == VL53L1_ERROR_NONE)
			{
				if( RANGING_SENSOR_COMMS_Write_CCI(
								pdev->i2c_slave_address,
								0,
								index+position,
								pdata+position,
								data_size) != 0 )
				{
					status = VL53L1_ERROR_CONTROL_INTERFACE;
				}
			}

#ifdef VL53L1_LOG_ENABLE
			if (status == VL53L1_ERROR_NONE) {
				char* pvalue_as_str;
				uint32_t i;

				/*lint --e{661} Suppress out of bounds walkthrough warning */
				/*lint --e{662} Suppress out of bounds walkthrough warning */

				// Build  value as string;
				pvalue_as_str =  value_as_str;
				for(i = 0 ; i < data_size ; i++)
				{
						sprintf(pvalue_as_str, ", 0x%02X", *(pdata+position+i));
						pvalue_as_str = pvalue_as_str + 6;
				}

				register_name[0] = 0;
				VL53L1_get_register_name(
						index+(uint16_t)position,
						register_name);

				trace_i2c(
						/* "WriteAutoIncrement(0x%04X%s); // %3u bytes\n",
						index+(uint16_t)position, */
						"WriteAutoIncrement(%s%s); // %3u bytes\n",
						register_name,
						value_as_str,
						data_size);
			}
#endif // VL53L1_LOG_ENABLE
		}

		if(status != VL53L1_ERROR_NONE)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_WriteMulti RANGING_SENSOR_COMMS_Write_CCI() failed\n");
			trace_i2c(comms_error_string);
		}
	}
	else if(global_comms_type == VL53L1_SPI)
	{
		if((CP_STATUS)RANGING_SENSOR_COMMS_Write_SPI_16I(0, 0, index, pdata, count) != CP_STATUS_OK)
		{
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}

		if(status != VL53L1_ERROR_NONE)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_WriteMulti RANGING_SENSOR_COMMS_Write_SPI_16I() failed\n");
			trace_i2c(comms_error_string);
		}
	}
	else
	{
		trace_i2c("VL53L1_WriteMulti: Comms must be one of VL53L1_I2C or VL53L1_SPI\n");
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}

	return status;
}


VL53L1_Error VL53L1_ReadMulti(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint8_t      *pdata,
	uint32_t      count)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint32_t     position       = 0;
	uint32_t     data_size      = 0;

	char   comms_error_string[ERROR_TEXT_LENGTH];

	_LOG_STRING_BUFFER(register_name);
	_LOG_STRING_BUFFER(value_as_str);

	if(global_comms_type == VL53L1_I2C)
	{
		for(position=0; position<count; position+=VL53L1_COMMS_CHUNK_SIZE)
		{
			if(count > VL53L1_COMMS_CHUNK_SIZE)
			{
				if((position + VL53L1_COMMS_CHUNK_SIZE) > count)
				{
					data_size = count - position;
				}
				else
				{
					data_size = VL53L1_COMMS_CHUNK_SIZE;
				}
			}
			else
				data_size = count;

			if(status == VL53L1_ERROR_NONE)
			{
				if( RANGING_SENSOR_COMMS_Read_CCI(
								pdev->i2c_slave_address,
								0,
								index+position,
								pdata+position,
								data_size) != 0 )
				{
					status = VL53L1_ERROR_CONTROL_INTERFACE;
				}
			}
#ifdef VL53L1_LOG_ENABLE
			if(status == VL53L1_ERROR_NONE)
			{
				char* pvalue_as_str;
				uint32_t i;

				/*lint --e{661} Suppress out of bounds walkthrough warning */
				/*lint --e{662} Suppress out of bounds walkthrough warning */
				pvalue_as_str =  value_as_str;
				for(i = 0 ; i < data_size ; i++) {
					sprintf(pvalue_as_str, "0x%02X", *(pdata+position+i));
					if (i == 0) {
						pvalue_as_str = pvalue_as_str + 4;
					}
					else {
						pvalue_as_str = pvalue_as_str + 6;
					}
				}

				register_name[0] = 0;
				VL53L1_get_register_name(
						index+(uint16_t)position,
						register_name);

				trace_i2c(
						/* "ReadAutoIncrement(0x%04X,%3u); // = (%s)\n",
						index+(uint16_t)position, */
						"ReadAutoIncrement(%s,%3u); // = (%s)\n",
						register_name,
						data_size,
						value_as_str);
			}
#endif // VL53L1_LOG_ENABLE
		}

		if(status != VL53L1_ERROR_NONE)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_ReadMulti: RANGING_SENSOR_COMMS_Read_CCI() failed\n");
			trace_i2c(comms_error_string);
		}
	}
	else if(global_comms_type == VL53L1_SPI)
	{
		if((CP_STATUS)RANGING_SENSOR_COMMS_Read_SPI_16I(0, 0, index, pdata, count) != CP_STATUS_OK)
		{
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}

		if(status != VL53L1_ERROR_NONE)
		{
			RANGING_SENSOR_COMMS_Get_Error_Text(comms_error_string); /*lint !e534 ignoring return value */
			trace_i2c("VL53L1_ReadMulti: RANGING_SENSOR_COMMS_Read_SPI_16I() failed\n");
			trace_i2c(comms_error_string);
		}
	}
	else
	{
		trace_i2c("VL53L1_ReadMulti: Comms must be one of VL53L1_I2C or VL53L1_SPI\n");
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}

	return status;
}


VL53L1_Error VL53L1_WrByte(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint8_t       data)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint8_t  buffer[1];

	// Split 16-bit word into MS and LS uint8_t
	buffer[0] = (uint8_t)(data);

	status = VL53L1_WriteMulti(pdev, index, buffer, 1);

	return status;
}


VL53L1_Error VL53L1_WrWord(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint16_t      data)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint8_t  buffer[2];

	// Split 16-bit word into MS and LS uint8_t
	buffer[0] = (uint8_t)(data >> 8);
	buffer[1] = (uint8_t)(data &  0x00FF);

	status = VL53L1_WriteMulti(pdev, index, buffer, VL53L1_BYTES_PER_WORD);

	return status;
}


VL53L1_Error VL53L1_WrDWord(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint32_t      data)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint8_t  buffer[4];

	// Split 32-bit word into MS ... LS bytes
	buffer[0] = (uint8_t) (data >> 24);
	buffer[1] = (uint8_t)((data &  0x00FF0000) >> 16);
	buffer[2] = (uint8_t)((data &  0x0000FF00) >> 8);
	buffer[3] = (uint8_t) (data &  0x000000FF);

	status = VL53L1_WriteMulti(pdev, index, buffer, VL53L1_BYTES_PER_DWORD);

	return status;
}


VL53L1_Error VL53L1_RdByte(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint8_t      *pdata)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint8_t  buffer[1];

	status = VL53L1_ReadMulti(pdev, index, buffer, 1);

	*pdata = buffer[0];

	return status;
}


VL53L1_Error VL53L1_RdWord(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint16_t     *pdata)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint8_t  buffer[2];

	status = VL53L1_ReadMulti(
					pdev,
					index,
					buffer,
					VL53L1_BYTES_PER_WORD);

	*pdata = (uint16_t)(((uint16_t)(buffer[0])<<8) + (uint16_t)buffer[1]);

	return status;
}


VL53L1_Error VL53L1_RdDWord(
	VL53L1_Dev_t *pdev,
	uint16_t      index,
	uint32_t     *pdata)
{
	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t  buffer[4];

	status = VL53L1_ReadMulti(
					pdev,
					index,
					buffer,
					VL53L1_BYTES_PER_DWORD);

	*pdata = ((uint32_t)buffer[0]<<24) + ((uint32_t)buffer[1]<<16) + ((uint32_t)buffer[2]<<8) + (uint32_t)buffer[3];

	return status;
}

/*
 * ----------------- HOST TIMING FUNCTIONS -----------------
 */

VL53L1_Error VL53L1_WaitUs(
	VL53L1_Dev_t *pdev,
	int32_t       wait_us)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	float wait_ms = (float)wait_us/1000.0f; /*lint !e586 float in windows platform*/
	HANDLE hEvent = CreateEvent(0, TRUE, FALSE, 0);

	SUPPRESS_UNUSED_WARNING(pdev);

	/*
	 * Use windows event handling to perform non-blocking wait.
	 */
	WaitForSingleObject(hEvent, (DWORD)(wait_ms + 0.5f)); /*lint !e534 ignoring return value */

	trace_i2c("WaitUs(%6d);\n", wait_us);

	return status;
}


VL53L1_Error VL53L1_WaitMs(
	VL53L1_Dev_t *pdev,
	int32_t       wait_ms)
{
	return VL53L1_WaitUs(pdev, wait_ms * 1000);
}

/*
 * ----------------- DEVICE TIMING FUNCTIONS -----------------
 */

VL53L1_Error VL53L1_GetTimerFrequency(int32_t *ptimer_freq_hz)
{
	*ptimer_freq_hz = 0;

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GetTimerFrequency: Freq : %dHz\n", *ptimer_freq_hz);
	return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_GetTimerValue(int32_t *ptimer_count)
{
	*ptimer_count = 0;

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GetTimerValue: Freq : %dHz\n", *ptimer_count);
	return VL53L1_ERROR_NONE;
}


/*
 * ----------------- GPIO FUNCTIONS -----------------
 */

VL53L1_Error VL53L1_GpioSetMode(uint8_t pin, uint8_t mode)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Mode((RS_GPIO_Pin)pin, (RS_GPIO_Mode)mode) != CP_STATUS_OK)
	{
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GpioSetMode: Status %d. Pin %d, Mode %d\n", status, pin, mode);
	return status;
}


VL53L1_Error  VL53L1_GpioSetValue(uint8_t pin, uint8_t value)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value((RS_GPIO_Pin)pin, value) != CP_STATUS_OK)
	{
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GpioSetValue: Status %d. Pin %d, Mode %d\n", status, pin, value);
	return status;

}


VL53L1_Error  VL53L1_GpioGetValue(uint8_t pin, uint8_t *pvalue)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	DWORD value = 0;

	if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Get_Value((RS_GPIO_Pin)pin, &value) != CP_STATUS_OK)
	{
		status = VL53L1_ERROR_CONTROL_INTERFACE;
	}
	else
	{
		*pvalue = (uint8_t)value;
	}

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GpioGetValue: Status %d. Pin %d, Mode %d\n", status, pin, *pvalue);
	return status;
}

/*
 * ----------------- HARDWARE STATE FUNCTIONS -----------------
 */

VL53L1_Error  VL53L1_GpioXshutdown(uint8_t value)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	if(status == VL53L1_ERROR_NONE) /*lint !e774 always true */
	{
		status = VL53L1_GpioSetMode((uint8_t)GPIO_XSHUTDOWN, (uint8_t)GPIO_OutputPP);
	}

	if(status == VL53L1_ERROR_NONE)
	{
		if(value)
		{
			if ((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value(GPIO_XSHUTDOWN, (DWORD)Pin_State_High) != CP_STATUS_OK)
			{
				status = VL53L1_ERROR_CONTROL_INTERFACE;
			}
		}
		else
		{
			if ((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value(GPIO_XSHUTDOWN, (DWORD)Pin_State_Low) != CP_STATUS_OK)
			{
				status = VL53L1_ERROR_CONTROL_INTERFACE;
			}
		}
	}

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GpioXShutdown: Status %d. Value %d\n", status, value);
	return status;
}


VL53L1_Error  VL53L1_GpioCommsSelect(uint8_t value)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	if(status == VL53L1_ERROR_NONE) /*lint !e774 always true */
	{
		status = VL53L1_GpioSetMode((uint8_t)GPIO_SPI_CHIP_SELECT, (uint8_t)GPIO_OutputPP);
	}

	if(status == VL53L1_ERROR_NONE)
	{
		if(value)
		{
			if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value(GPIO_SPI_CHIP_SELECT, (DWORD)Pin_State_High) != CP_STATUS_OK)
			{
				status = VL53L1_ERROR_CONTROL_INTERFACE;
			}
		}
		else
		{
			if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value(GPIO_SPI_CHIP_SELECT, (DWORD)Pin_State_Low) != CP_STATUS_OK)
			{
				status = VL53L1_ERROR_CONTROL_INTERFACE;
			}
		}
	}

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GpioCommsSelect: Status %d. Value %d\n", status, value);
	return status;
}


VL53L1_Error  VL53L1_GpioPowerEnable(uint8_t value)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;
	POWER_BOARD_CMD power_cmd;

	if(status == VL53L1_ERROR_NONE) /*lint !e774 always true */
	{
		status = VL53L1_GpioSetMode((uint8_t)GPIO_POWER_ENABLE, (uint8_t)GPIO_OutputPP);
	}

	if(status == VL53L1_ERROR_NONE)
	{
		if(value)
		{
			if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value(GPIO_POWER_ENABLE, (DWORD)Pin_State_High) != CP_STATUS_OK)
			{
				status = VL53L1_ERROR_CONTROL_INTERFACE;
			}
		}
		else
		{
			if((CP_STATUS)RANGING_SENSOR_COMMS_GPIO_Set_Value(GPIO_POWER_ENABLE, (DWORD)Pin_State_Low) != CP_STATUS_OK)
			{
				status = VL53L1_ERROR_CONTROL_INTERFACE;
			}
		}
	}

	if(status == VL53L1_ERROR_NONE && _power_board_in_use == 1 && value) /*lint !e506 !e845 !e774*/
	{
		memset(&power_cmd, 0, sizeof(POWER_BOARD_CMD));
		power_cmd.command = ENABLE_DUT_POWER;

		if((CP_STATUS)RANGING_SENSOR_COMMS_Write_System_I2C(
			POWER_BOARD_I2C_ADDRESS, sizeof(POWER_BOARD_CMD), (uint8_t*)&power_cmd) != CP_STATUS_OK)
		{
			status = VL53L1_ERROR_CONTROL_INTERFACE;
		}
	}

	trace_print(VL53L1_TRACE_LEVEL_INFO, "VL53L1_GpioPowerEnable: Status %d. Value %d\n", status, value);
	return status;
}


VL53L1_Error  VL53L1_GpioInterruptEnable(void (*function)(void), uint8_t edge_type)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	SUPPRESS_UNUSED_WARNING(function);
	SUPPRESS_UNUSED_WARNING(edge_type);

	return status;
}


VL53L1_Error  VL53L1_GpioInterruptDisable(void)
{
	VL53L1_Error status         = VL53L1_ERROR_NONE;

	return status;
}


VL53L1_Error VL53L1_GetTickCount(
	uint32_t *ptick_count_ms)
{

	/* Returns current tick count in [ms] */

	VL53L1_Error status  = VL53L1_ERROR_NONE;

	*ptick_count_ms = timeGetTime();

	trace_print(
	VL53L1_TRACE_LEVEL_DEBUG,
	"VL53L1_GetTickCount() = %5u ms;\n",
	*ptick_count_ms);

	return status;

}


VL53L1_Error VL53L1_WaitValueMaskEx(
	VL53L1_Dev_t *pdev,
	uint32_t      timeout_ms,
	uint16_t      index,
	uint8_t       value,
	uint8_t       mask,
	uint32_t      poll_delay_ms)
{
	/*
	 * Platform implementation of WaitValueMaskEx V2WReg script command
	 *
	 * WaitValueMaskEx(
	 *          duration_ms,
	 *          index,
	 *          value,
	 *          mask,
	 *          poll_delay_ms);
	 */

	VL53L1_Error status         = VL53L1_ERROR_NONE;
	uint32_t     start_time_ms   = 0;
	uint32_t     current_time_ms = 0;
	uint8_t      byte_value      = 0;
	uint8_t      found           = 0;
#ifdef VL53L1_LOG_ENABLE
	uint32_t     trace_functions = 0;
#endif

	_LOG_STRING_BUFFER(register_name);

	SUPPRESS_UNUSED_WARNING(poll_delay_ms);

#ifdef VL53L1_LOG_ENABLE
	/* look up register name */
	VL53L1_get_register_name(
			index,
			register_name);

	/* Output to I2C logger for FMT/DFT  */
	trace_i2c("WaitValueMaskEx(%5d, %s, 0x%02X, 0x%02X, %5d);\n",
		timeout_ms, register_name, value, mask, poll_delay_ms);
#endif // VL53L1_LOG_ENABLE

	/* calculate time limit in absolute time */

	VL53L1_GetTickCount(&start_time_ms);
	pdev->new_data_ready_poll_duration_ms = 0;

	/* remember current trace functions and temporarily disable
	 * function logging
	 */

#ifdef VL53L1_LOG_ENABLE
	trace_functions = _LOG_GET_TRACE_FUNCTIONS();
#endif
	_LOG_SET_TRACE_FUNCTIONS(VL53L1_TRACE_FUNCTION_NONE);

	/* wait until value is found, timeout reached on error occurred */

	while ((status == VL53L1_ERROR_NONE) &&
		   (pdev->new_data_ready_poll_duration_ms < timeout_ms) &&
		   (found == 0))
	{
		status = VL53L1_RdByte(
						pdev,
						index,
						&byte_value);

		if ((byte_value & mask) == value)
		{
			found = 1;
		}

		/*if (status == VL53L1_ERROR_NONE  &&
			found == 0 &&
			poll_delay_ms > 0)
			status = VL53L1_WaitMs(
							pdev,
							poll_delay_ms);
		*/

		/* Update polling time (Compare difference rather than absolute to
		negate 32bit wrap around issue) */
		VL53L1_GetTickCount(&current_time_ms);
		pdev->new_data_ready_poll_duration_ms = current_time_ms - start_time_ms;
	}

	/* Restore function logging */
	_LOG_SET_TRACE_FUNCTIONS(trace_functions);

	if (found == 0 && status == VL53L1_ERROR_NONE)
		status = VL53L1_ERROR_TIME_OUT;

	return status;
}

