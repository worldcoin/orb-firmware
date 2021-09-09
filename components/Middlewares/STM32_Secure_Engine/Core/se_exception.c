/**
  ******************************************************************************
  * @file    se_exception.c
  * @author  MCD Application Team
  * @brief   Secure Engine exception module.
  *          This file provides set of firmware functions to manage SE
  *          non maskeable interruption
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
/* Includes ------------------------------------------------------------------*/
#include <stddef.h>
#include "se_low_level.h"
#include "se_exception.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup  SE_EXCEPTIONS SE Exception
  * @{
  */

/** @defgroup SE_EXCEPTIONS_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief SE Non Maskeable Interrupt handler
  * @param None.
  * @retval None.
  */

void SE_NMI_ExceptionHandler(void)
{
  NVIC_SystemReset();
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

/**
  * @}
  */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
