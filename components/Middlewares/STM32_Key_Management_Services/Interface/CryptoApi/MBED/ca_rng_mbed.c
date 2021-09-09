/**
  ******************************************************************************
  * @file    ca_rng_mbed.c
  * @author  MCD Application Team
  * @brief   This file contains the RNG router implementation of
  *          the Cryptographic API (CA) module to the MBED Cryptographic library.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* CA sources are built by building ca_core.c giving it the proper ca_config.h */
/* This file can not be build alone                                            */
#if defined(CA_CORE_C)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_RNG_MBED_C
#define CA_RNG_MBED_C

/* Includes ------------------------------------------------------------------*/
#include "ca_rng_mbed.h"


/* Private defines -----------------------------------------------------------*/


/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Functions Definition ------------------------------------------------------*/
#if defined(CA_ROUTE_RNG) && ((CA_ROUTE_RNG & CA_ROUTE_MASK) == CA_ROUTE_MBED)
int32_t CA_RNGinit(const CA_RNGinitInput_stt *P_pInputData, CA_RNGstate_stt *P_pRandomState)
{
  (void)P_pInputData;
  (void)P_pRandomState;
  return CA_RNG_SUCCESS;
}
int32_t CA_RNGfree(CA_RNGstate_stt *P_pRandomState)
{
  (void)P_pRandomState;
  return CA_RNG_SUCCESS;
}
#endif /* (CA_ROUTE_RNG & CA_ROUTE_MASK) == CA_ROUTE_MBED */




#endif /* CA_RNG_MBED_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

