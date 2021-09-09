/**
  ******************************************************************************
  * @file    tkms.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for trusted Key Management
  *          Services (tKMS) module of the PKCS11 APIs when called from the
  *          secure enclave or without any enclave
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"
#if defined(KMS_ENABLED)
#include "tkms.h"
#include "kms_entry.h"

/** @addtogroup tKMS User interface to Key Management Services (tKMS)
  * @{
  */

/** @addtogroup KMS_TKMS_UTILS tKMS Utilities
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_TKMS_UTILS_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is used to return the caller cluster number
  * @note   Declared as 'weak' it can be overloaded by caller code
  * @retval Caller cluster number
  */
#if defined (__ICCARM__)
__weak uint32_t tKMS_GetCluster(void)
#elif defined(__ARMCC_VERSION) || defined(__GNUC__)
__attribute__((weak)) uint32_t tKMS_GetCluster(void)
#endif
{
  return KMS_CLUST_SECX;
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


#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
