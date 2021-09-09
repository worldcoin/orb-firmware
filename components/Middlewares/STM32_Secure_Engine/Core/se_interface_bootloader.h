/**
  ******************************************************************************
  * @file    se_interface_bootloader.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Engine Reserved Interface
  *          module functionalities. These services are used by the bootloader.
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
#ifndef SE_INTERFACE_BOOTLOADER_H
#define SE_INTERFACE_BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_INTERFACE
  * @{
  */

/** @addtogroup SE_INTERFACE_BOOTLOADER
  * @{
  */

/** @addtogroup SE_INTERFACE_BOOTLOADER_Exported_Functions
  * @{
  */

/* SE Initialization functions */
SE_ErrorStatus SE_Init(SE_StatusTypeDef *peSE_Status, uint32_t uSystemCoreClock);
SE_ErrorStatus SE_Startup(void);

/* Lock function to prevent the user-app from running bootloader reserved code */
SE_ErrorStatus SE_LockRestrictServices(SE_StatusTypeDef *pSE_Status);

/* FUS or wireless stack update process managed by CM0 */
SE_ErrorStatus SE_CM0_Update(SE_StatusTypeDef *pSE_Status);

/* Configure "On The Fly DECryption" mechanism (OTFDEC) for external FLASH */
SE_ErrorStatus SE_ExtFlash_Decrypt_Init(SE_StatusTypeDef *pSE_Status, SE_FwRawHeaderTypeDef *pxSE_Metadata);

/* Images handling functions */
SE_ErrorStatus SE_SFU_IMG_Erase(SE_StatusTypeDef *pSE_Status, uint8_t *pDestination, uint32_t Length);
SE_ErrorStatus SE_SFU_IMG_Read(SE_StatusTypeDef *pSE_Status, uint8_t *pDestination, const uint8_t *pSource,
                               uint32_t Length);
SE_ErrorStatus SE_SFU_IMG_Write(SE_StatusTypeDef *pSE_Status, uint8_t *pDestination, const uint8_t *pSource,
                                uint32_t Length);
SE_ErrorStatus SE_VerifyHeaderSignature(SE_StatusTypeDef *peSE_Status, SE_FwRawHeaderTypeDef *pxFwRawHeader);
#ifdef ENABLE_IMAGE_STATE_HANDLING
SE_ErrorStatus SE_SFU_IMG_GetActiveFwState(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                           SE_FwStateTypeDef *pFwState);
SE_ErrorStatus SE_SFU_IMG_SetActiveFwState(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                           SE_FwStateTypeDef *pFwState);
#endif /* ENABLE_IMAGE_STATE_HANDLING */

/* Crypto-agnostic functions */
SE_ErrorStatus SE_Decrypt_Init(SE_StatusTypeDef *peSE_Status, SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType);
SE_ErrorStatus SE_Decrypt_Append(SE_StatusTypeDef *peSE_Status, const uint8_t *pInputBuffer, int32_t InputSize,
                                 uint8_t *pOutputBuffer, int32_t *pOutputSize);
SE_ErrorStatus SE_Decrypt_Finish(SE_StatusTypeDef *peSE_Status, uint8_t *pOutputBuffer, int32_t *pOutputSize);
SE_ErrorStatus SE_AuthenticateFW_Init(SE_StatusTypeDef *peSE_Status, SE_FwRawHeaderTypeDef *pxSE_Metadata,
                                      uint32_t SE_FwType);
SE_ErrorStatus SE_AuthenticateFW_Append(SE_StatusTypeDef *peSE_Status, const uint8_t *pInputBuffer, int32_t InputSize,
                                        uint8_t *pOutputBuffer, int32_t *pOutputSize);
SE_ErrorStatus SE_AuthenticateFW_Finish(SE_StatusTypeDef *peSE_Status, uint8_t *pOutputBuffer, int32_t *pOutputSize);

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

#endif /* SE_INTERFACE_BOOTLOADER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
