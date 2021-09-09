/**
  ******************************************************************************
  * @file    kms_mem.h
  * @author  MCD Application Team
  * @brief   This file contains utilities for the manipulation of memory
  *          of Key Management Services (KMS)
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef KMS_MEM_H
#define KMS_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_MEM Memory utilities
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
/** @addtogroup KMS_MEM_Exported_Functions Exported Functions
  * @{
  */
void KMS_MemInit(void);
CK_VOID_PTR KMS_Alloc(CK_SESSION_HANDLE Session, size_t Size);
void KMS_Free(CK_SESSION_HANDLE Session, CK_VOID_PTR Ptr);

/**
  * @}
  */

/**
  * @}
  */


/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* KMS_MEM_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
