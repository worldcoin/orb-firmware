/**
  ******************************************************************************
  * @file    kms_mem_low_level.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
  *          memory management Low Level function to custom memory allocator
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
#include "kms_mem.h"

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_MEM_LL Memory management Low Level access
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
  * @brief  Initialize memory management
  */
void KMS_MemInit(void)
{
  /* Put your own implementation here */
}

/**
  * @brief  Allocate memory
  * @param  session Session requesting the memory
  * @param  size Size of the memory to allocate
  * @retval Allocated pointer if successful to allocate, NULL_PTR if failed
  */
CK_VOID_PTR KMS_Alloc(CK_SESSION_HANDLE session, size_t size)
{
  /* Put your own implementation here */
}

/**
  * @brief  Free memory
  * @param  session Session freeing the memory
  * @param  ptr     Pointer to the memory to free
  * @retval None
  */
void KMS_Free(CK_SESSION_HANDLE session, CK_VOID_PTR ptr)
{
  /* Put your own implementation here */
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
