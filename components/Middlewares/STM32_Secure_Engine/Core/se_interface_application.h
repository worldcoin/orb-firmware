/**
  ******************************************************************************
  * @file    se_interface_application.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Engine Interface module
  *          functionalities. These services are used by the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SE_INTERFACE_APPLICATION_H
#define SE_INTERFACE_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include "se_def.h"
#include "se_user_application.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_INTERFACE Secure Engine Interface
  * @{
  */

/** @addtogroup  SE_INTERFACE_APPLICATION
  * @{
  */

/** @addtogroup SE_INTERFACE_APPLICATION_Exported_Constant
  * @brief These are the constant to be used by the User Application when calling SE services.
  * @{
  */

#ifdef ENABLE_IMAGE_STATE_HANDLING
#define VALID_ALL_SLOTS 255U  /* All slots are validated with a single validation request */
#endif /* ENABLE_IMAGE_STATE_HANDLING */

/**
  * @}
  */


/** @addtogroup SE_INTERFACE_APPLICATION_Exported_Functions
  * @brief These are the functions the User Application can call to use the SE services.
  * @{
  */

/*
 * The functions below are demonstrating how the user application can use some SE services.
 */

SE_ErrorStatus SE_APP_GetActiveFwInfo(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                      SE_APP_ActiveFwInfo_t *pFwInfo);
#ifdef ENABLE_IMAGE_STATE_HANDLING
SE_ErrorStatus SE_APP_ValidateFw(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber);
SE_ErrorStatus SE_APP_GetActiveFwState(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber, SE_FwStateTypeDef *pFwState);
#endif /* ENABLE_IMAGE_STATE_HANDLING */
#ifdef SFU_ISOLATE_SE_WITH_MPU
void SE_APP_SVC_Handler(uint32_t *args);
#if defined(UPDATE_IRQ_SERVICE)
SE_ErrorStatus SE_SYS_SaveDisableIrq(SE_StatusTypeDef *peSE_Status, uint32_t *pIrqState, uint32_t IrqStateNb);
SE_ErrorStatus SE_SYS_RestoreEnableIrq(SE_StatusTypeDef *peSE_Status, uint32_t *pIrqState, uint32_t IrqStateNb);
#endif /* defined(UPDATE_IRQ_SERVICE) */
#endif /* SFU_ISOLATE_SE_WITH_MPU */

/**
  * @}
  */

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

#endif /* SE_INTERFACE_APPLICATION_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

