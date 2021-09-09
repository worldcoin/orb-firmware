/**
  ******************************************************************************
  * @file    se_user_application.h
  * @author  MCD Application Team
  *          This file is a placeholder for the code dedicated to the user application.
  *          These services are used by the application.
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
#ifndef SE_USER_APPLICATION_H
#define SE_USER_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @addtogroup SE_APPLI SE Code for Application
  * @{
  */

/** @defgroup SE_APPLI_Struct Structure(s) for user defined SE code
  * @{
  */

/**
  * @brief  Firmware Information structure definition.
  *         This structure is used to retrieve some information
  *         about the active Firmware.
  */
typedef struct
{
  uint16_t ActiveFwVersion;        /*!< Firmware version (see @ref SFU_FW_VERSION_START_NUM for the min. valid value) */
  uint32_t ActiveFwSize;           /*!< Firmware size (bytes) */
} SE_APP_ActiveFwInfo_t;

/**
  * @}
  */

/** @addtogroup SE_APPLI_Exported_Functions
  * @{
  */

SE_ErrorStatus SE_APPLI_GetActiveFwInfo(uint32_t SlotNumber, SE_APP_ActiveFwInfo_t *p_FwInfo);

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

#endif /* SE_USER_APPLICATION_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

