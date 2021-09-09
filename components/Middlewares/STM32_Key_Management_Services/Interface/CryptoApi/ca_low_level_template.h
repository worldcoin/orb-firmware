/**
  ******************************************************************************
  * @file    ca_low_level_template.h
  * @author  MCD Application Team
  * @brief   This file contains the low level definitions of the Cryptographic API (CA) module.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_LOW_LEVEL_H
#define CA_LOW_LEVEL_H

/* Includes ------------------------------------------------------------------*/
/*
 * STM32 target HAL driver include
 * ex: stm32l4xx_hal.h
 */
#include "stm32XXX_hal.h"

/* Exported constants --------------------------------------------------------*/
#if defined(CA_HAL_CRYPTOLIB_SUPP)
#if ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_HAL)  \
 || ((CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)  \
 || ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL) \
 || ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_HAL)  \
 || ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)
/*
 * AES/CRYP instance to use
 * ex: AES1
 */
#define CA_AES_INSTANCE     (AESx)

#endif /* CA_ROUTE_AES_CBC || CA_ROUTE_AES_CCM || CA_ROUTE_AES_CMAC \
       || CA_ROUTE_AES_ECB || CA_ROUTE_AES_GCM => CA_ROUTE_HAL */


#if ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_HAL) || ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_HAL)
/*
 * PKA instance to use
 * ex: PKA
 */
#define CA_PKA_INSTANCE     (PKAx)

#endif /* CA_ROUTE_ECC_ECDSA || CA_ROUTE_RSA => CA_ROUTE_HAL */

#endif /* CA_HAL_CRYPTOLIB_SUPP */

/* Exported function ---------------------------------------------------------*/
#if defined(CA_ST_CRYPTOLIB_SUPP)
void CA_LL_CRC_Init(void);

#endif /* CA_ST_CRYPTOLIB_SUPP */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_LOW_LEVEL_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

