/**
  ******************************************************************************
  * @file    ca.h
  * @author  MCD Application Team
  * @brief   This file constitutes the main header of Cryptographic API (CA)
  *          module.
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
#ifndef CA_H
#define CA_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "ca_conf.h"
#include "ca_err_codes.h"
#include "ca_defines.h"
#include "ca_types.h"

#include "ca_if_utils.h"
#include "ca_low_level.h"

/* Libraries -----------------------------------------------------------------*/

#if defined(CA_MBED_CRYPTOLIB_SUPP)
#if defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_MBED)     || \
    defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_MBED)   || \
    defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_MBED)     || \
    defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_MBED)     || \
    defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED) || \
    defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED)
#define CA_USES_PSA_CRYPTO
#endif /* All routes that uses PSA CRYPTO */
#endif /* CA_MBED_CRYPTOLIB_SUPP */


#if defined(CA_ST_CRYPTOLIB_SUPP)
/* Nothing to include, ST Cryptolib headers included into specific files upon needs */
#endif /* CA_ST_CRYPTOLIB_SUPP */

#if defined(CA_MBED_CRYPTOLIB_SUPP) && defined(CA_USES_PSA_CRYPTO)
#include "psa/crypto.h"
#endif /* CA_MBED_CRYPTOLIB_SUPP && CA_USES_PSA_CRYPTO */

#if defined(CA_HAL_CRYPTOLIB_SUPP)
/* Nothing to include, HAL drivers included by caller based on platform configuration */
#endif /* CA_HAL_CRYPTOLIB_SUPP */

/* Features ------------------------------------------------------------------*/

#if defined(CA_FEAT_RNG)
#include "ca_rng.h"
#endif /* CA_FEAT_RNG */

#if defined(CA_FEAT_AES)
#include "ca_aes.h"
#endif /* CA_FEAT_AES */

#if defined(CA_FEAT_HASH)
#include "ca_hash.h"
#endif /* CA_FEAT_HASH */

#if defined(CA_FEAT_ECC)
#include "ca_ecc.h"
#endif /* CA_FEAT_ECC */

#if defined(CA_FEAT_RSA)
#include "ca_rsa.h"
#endif /* CA_FEAT_RSA */

/* Exported functions prototypes ---------------------------------------------*/
int32_t CA_Init(void);
int32_t CA_DeInit(void);

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* CA_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

