/**
  ******************************************************************************
  * @file    se_utils.c
  * @author  MCD Application Team
  * @brief   Secure Engine UTILS module.
  *          This file provides set of firmware functions to manage SE Utils
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

/* Includes ------------------------------------------------------------------*/
#include "stddef.h"
#include "se_low_level.h"
#include "se_utils.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup  SE_UTILS SE Utils
  * @{
  */

/** @defgroup SE_UTILS_Private_Variables Private Variables
  * @{
  */
static uint32_t m_uSE_SystemCoreClock;           /*!< System clock used for tick calculation */

/**
  * @}
  */

/** @defgroup SE_UTILS_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief Set Secure Engine SystemCoreClock variable.
  * @param uSystemCoreClock Secure Engine SystemCoreClock variable.
  * @retval None.
  */
void SE_SetSystemCoreClock(uint32_t uSystemCoreClock)
{
  m_uSE_SystemCoreClock = uSystemCoreClock;
}

/**
  * @brief Provide a tick value in millisecond.
  * @note This function overwrites the one declared as __weak in HAL.
  *       In this implementation, time is counted without using SysTick timer interrupts.
  * @retval tick value
  */
uint32_t HAL_GetTick(void)
{
  static uint32_t m_uTick = 0U;
  static uint32_t t1 = 0U, tdelta = 0U;
  uint32_t t2;

  t2 =  SysTick->VAL;

  if (t2 <= t1)
  {
    tdelta += t1 - t2;
  }
  else
  {
    tdelta += t1 + SysTick->LOAD - t2;
  }

  if (tdelta > (m_uSE_SystemCoreClock / 1000U))
  {
    tdelta = 0U;
    m_uTick ++;
  }

  t1 = t2;
  return m_uTick;
}

/**
  * @brief This function provides minimum delay (in milliseconds) based
  *        on variable incremented.
  * @param Delay  specifies the delay time length, in milliseconds.
  * @retval None
  */
void HAL_Delay(uint32_t Delay)
{
  uint32_t tickstart = HAL_GetTick();
  uint32_t wait = Delay;

  /* Add a period to guaranty minimum wait */
  if (wait < HAL_MAX_DELAY)
  {
    wait++;
  }

  while ((HAL_GetTick() - tickstart) < wait)
  {
  }
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
