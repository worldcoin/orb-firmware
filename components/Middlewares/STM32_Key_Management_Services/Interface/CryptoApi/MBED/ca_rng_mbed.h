/**
  ******************************************************************************
  * @file    ca_rng_mbed.h
  * @author  MCD Application Team
  * @brief   This file contains the RNG router includes and definitions of
  *          the Cryptographic API (CA) module to the MBED Cryptographic library.
  * @note    This file shall never be included directly by application but
  *          through the main header ca_hash.h
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
#ifndef CA_RNG_MBED_H
#define CA_RNG_MBED_H

#if !defined(CA_RNG_H)
#error "This file shall never be included directly by application but through the main header ca_rng.h"
#endif /* CA_RNG_H */

/* Configuration defines -----------------------------------------------------*/


/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> RNG */
#if defined(CA_ROUTE_RNG) && ((CA_ROUTE_RNG & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/


/* Defines ------------------------------------------------------------------*/


/* Typedef ------------------------------------------------------------------*/
/**
  * @brief  Structure that contains the RNG stat
  */
typedef struct
{
  uint8_t mRNGstate[4];  /*!< Underlying DRBG context. It is initialized by \ref RNGinit */

  int32_t mDRBGtype;     /*!< Used to check if the random state has been mFlag */

  uint32_t mFlag;        /*!< Used to check if the random state has been mFlag */
}
CA_RNGstate_stt;

/**
  * @brief  Structure used by RNGinit to initialize a DRBG
  */
typedef struct
{

  uint8_t *pmEntropyData;   /*!< The entropy data input */

  int32_t mEntropyDataSize; /*!< Size of the entropy data input */

  uint8_t *pmNonce;         /*!< The Nonce data */

  int32_t mNonceSize;       /*!< Size of the Nonce */

  uint8_t *pmPersData;      /*!< Personalization String */

  int32_t mPersDataSize;    /*!< Size of personalization string*/
} CA_RNGinitInput_stt;

/* Exported functions -------------------------------------------------------*/
int32_t CA_RNGinit(const CA_RNGinitInput_stt *P_pInputData, CA_RNGstate_stt *P_pRandomState);
int32_t CA_RNGfree(CA_RNGstate_stt *P_pRandomState);

#endif /* (CA_ROUTE_RNG & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< RNG */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_RNG_MBED_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

