/**
  ******************************************************************************
  * @file    nvms_low_level.c
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module NVM Low Level access to physical storage (Flash...)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "nvms_low_level.h"

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_NVMLL NVM Low Level access
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_NVMLL_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief   Flash low level driver initialization.
  */
void NVMS_LL_Init(void)
{

}

/**
  * @brief   Determines if a flash block is in erased state.
  *
  * @param   block         the block identifier
  * @return                  The block state.
  * @retval false            if the block is not in erased state.
  * @retval true             if the block is in erased state.
  */

/* Template version of the function */
bool NVMS_LL_IsBlockErased(nvms_block_t block)
{

  return false;
}



/**
  * @brief   Erases a block.
  * @note    The erase operation is verified internally.
  *
  * @param   block         the block identifier
  * @return                  The operation status.
  * @retval false            if the operation is successful.
  * @retval true             if the erase operation failed.
  */
/* Template version of the function */
bool NVMS_LL_BlockErase(nvms_block_t block)
{

  return true;
}

/**
  * @brief   Writes data.
  * @note    The write operation is verified internally.
  * @note    If the write operation partially writes flash pages then the
  *          unwritten bytes are left to the filler value.
  *
  * @param   source        pointer to the data to be written
  * @param   destination   pointer to the flash position
  * @param   size          size of data
  * @return                  The operation status.
  * @retval false            if the operation is successful.
  * @retval true             if the write operation failed.
  */
/* Template version of the function */
bool NVMS_LL_Write(const uint8_t *source, uint8_t *destination, size_t size)
{
  return true;
}


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

