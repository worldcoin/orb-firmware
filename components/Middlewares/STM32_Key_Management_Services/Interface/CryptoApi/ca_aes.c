/**
  ******************************************************************************
  * @file    ca_aes.c
  * @author  MCD Application Team
  * @brief   This file constitutes the Cryptographic API (CA) module AES sources
  *          as its inclusion allows based on the configuration to build every
  *          needed other AES related c files contents
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
#ifndef CA_AES_C
#define CA_AES_C

/* Includes ------------------------------------------------------------------*/

#if defined(CA_ST_CRYPTOLIB_SUPP)
/* No sources to include */
#endif /* CA_ST_CRYPTOLIB_SUPP */

#if defined(CA_MBED_CRYPTOLIB_SUPP)
#include "MBED/ca_aes_mbed.c"
#endif /* CA_MBED_CRYPTOLIB_SUPP */

#if defined(CA_HAL_CRYPTOLIB_SUPP)
#include "HAL/ca_aes_hal.c"
#endif /* CA_HAL_CRYPTOLIB_SUPP */

#endif /* CA_AES_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

