/**
  ******************************************************************************
  * @file    se_utils.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Engine UTILS module
  *          functionalities.
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
#ifndef SE_UTILS_H
#define SE_UTILS_H

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

/** @addtogroup SE_UTILS SE Utils
  * @{
  */

/** @addtogroup SE_UTILS_Exported_Functions
  * @{
  */
void SE_SetSystemCoreClock(uint32_t uSystemCoreClock);

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

#endif /* SE_UTILS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

