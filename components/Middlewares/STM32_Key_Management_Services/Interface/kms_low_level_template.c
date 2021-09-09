/**
  ******************************************************************************
  * @file    kms_low_level.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
  *          module Low Level access to Flash, CRC...
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

#include "kms.h"
#include "kms_low_level.h"

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_LL Low Level access
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Read data from flash and store into buffer
  * @note   Used to access encrypted blob in flash.
  * @param  pDestination buffer to store red data
  * @param  pSource flash aread to read from
  * @param  Length amount of data to read from flash
  * @retval Operation status
  */
CK_RV KMS_LL_FLASH_Read(void *pDestination, const void *pSource, uint32_t Length)
{
  return CKR_OK;
}

#ifdef KMS_SE_CHECK_PARAMS

/* Remove compilation optimization */
#if defined(__ICCARM__)
#pragma optimize=none
#elif defined(__CC_ARM)
#pragma O0
#else
__attribute__((optimize("O0")))
#endif /* __ICCARM__ */

/**
  * @brief  Check if given buffer in inside secure enclave (RAM or Flash, NVM_Storgae)
  *              If it is in Enclave area, then generate a RESET.
  * @param  pBuffer Buffer address
  * @param  ulSize  Buffer size
  * @retval void
  */
void KMS_LL_IsBufferInSecureEnclave(void *pBuffer, uint32_t Size)
{

}
#endif /* KMS_SE_CHECK_PARAMS */

/**
  * @brief  An error occurred
  * @note   Used to debug or reset in case of important issue
  * @param  error Error reported by KMS
  */
void KMS_LL_ReportError(uint32_t Error)
{

}

/**
  * @brief  A memory initialization occurred
  */
void KMS_LL_ReportMemInit(void)
{

}

/**
  * @brief  A memory allocation occurred
  * @param  size    Size allocated
  * @param  pMem    Allocated pointer
  */
void KMS_LL_ReportMemAlloc(uint32_t Size, void *pMem)
{

}

/**
  * @brief  A memory free occurred
  * @param  pMem    Freed pointer
  */
void KMS_LL_ReportMemFree(void *pMem)
{

}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
