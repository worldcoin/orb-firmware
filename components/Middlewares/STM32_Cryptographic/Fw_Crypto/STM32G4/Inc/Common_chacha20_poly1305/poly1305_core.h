/**
  ******************************************************************************
  * @file    poly1305_core.h
  * @author  MCD Application Team
  * @brief   Implements the core poly1305 functionality
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  * The implementation of poly1305 is heavily based on the 32-bit version of the
  * code available at https://github.com/floodyberry/poly1305-donna which was
  * released by the author (Andrew Moon) in the public domain.
  ******************************************************************************
  */

#ifndef __POLY1305_CORE_H__
#define __POLY1305_CORE_H__

/* Includes ------------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/** @addtogroup SymKeylowlevel
* @{
*/

/** @defgroup POLY1305lowlevel Poly1305-AES
* @brief Internal  Poly1305-AES Function
* @{
*/

void Poly1305_Core_Update (uint32_t *P_h,          \
                           const uint32_t *P_r,    \
                           const uint8_t *P_pInput,\
                           uint32_t P_LastBlock);

void Poly1305_Core_Finish(uint32_t *P_h,        \
                          const uint32_t *P_pad,\
                          uint32_t *P_pTag);                           

/**
* @}
*/

/**
* @}
*/

#endif /* __POLY1305_CORE_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
