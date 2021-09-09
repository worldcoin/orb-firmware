/**
  ******************************************************************************
  * @file    se_fwimg.h
  * @author  MCD Application Team
  * @brief   This file provides set of firmware functions to manage Secure Firmware
  *          Update functionalities.
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
#ifndef SE_FWIMG_H
#define SE_FWIMG_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup SE
  * @{
  */

/** @addtogroup SE_CORE
  * @{
  */

/** @addtogroup SE_IMG
  * @{
  */

/** @addtogroup SE_IMG_Exported_Functions
  * @{
  */
SE_ErrorStatus SE_IMG_Erase(uint8_t *pDestination, uint32_t Length);
SE_ErrorStatus SE_IMG_Read(uint8_t *pDestination, const uint8_t *pSource, uint32_t Length);
SE_ErrorStatus SE_IMG_Write(uint8_t *pDestination, const uint8_t *pSource, uint32_t Length);
#ifdef ENABLE_IMAGE_STATE_HANDLING
SE_ErrorStatus SE_IMG_SetActiveFwState(uint32_t SlotNumber, SE_FwStateTypeDef *pFwState);
SE_ErrorStatus SE_IMG_GetActiveFwState(uint32_t SlotNumber, SE_FwStateTypeDef *pFwState);
#endif /* ENABLE_IMAGE_STATE_HANDLING */

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

#endif /* SE_FWIMG_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
