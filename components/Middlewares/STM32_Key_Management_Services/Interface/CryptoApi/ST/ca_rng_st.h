/**
  ******************************************************************************
  * @file    ca_rng_st.h
  * @author  MCD Application Team
  * @brief   This file contains the RNG router includes and definitions of
  *          the Cryptographic API (CA) module to the ST Cryptographic library.
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
#ifndef CA_RNG_ST_H
#define CA_RNG_ST_H

#if !defined(CA_RNG_H)
#error "This file shall never be included directly by application but through the main header ca_rng.h"
#endif /* CA_RNG_H */

/* Configuration defines -----------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/

/* Structures Remapping -------------------------------------------------------*/

/* Defines Remapping ----------------------------------------------------------*/


/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> RNG */
#if defined(CA_ROUTE_RNG) && ((CA_ROUTE_RNG & CA_ROUTE_MASK) == CA_ROUTE_ST)
/* Defines Remapping ----------------------------------------------------------*/
#ifndef INCLUDE_RNG
#define INCLUDE_RNG
#endif /* INCLUDE_RNG */

#include "RNG/rng.h"

#define CA_CRL_DRBG_AES_MAX_BYTES_PER_REQUEST CRL_DRBG_AES_MAX_BYTES_PER_REQUEST
#define CA_CRL_DRBG_AES128_ENTROPY_MIN_LEN CRL_DRBG_AES128_ENTROPY_MIN_LEN
#define CA_CRL_DRBG_AES_ENTROPY_MAX_LEN CRL_DRBG_AES_ENTROPY_MAX_LEN
#define CA_CRL_DRBG_AES_MAX_PERS_STR_LEN CRL_DRBG_AES_MAX_PERS_STR_LEN
#define CA_CRL_DRBG_AES_MAX_ADD_INPUT_LEN CRL_DRBG_AES_MAX_ADD_INPUT_LEN
#define CA_CRL_DRBG_AES_MAX_NONCE_LEN CRL_DRBG_AES_MAX_NONCE_LEN
#define CA_CRL_DRBG_AES_REQS_BTW_RESEEDS CRL_DRBG_AES_REQS_BTW_RESEEDS
#define CA_CRL_DRBG_AES128_STATE_SIZE CRL_DRBG_AES128_STATE_SIZE

/* Structures Remapping -------------------------------------------------------*/
typedef RNGstate_stt CA_RNGstate_stt;
typedef RNGinitInput_stt CA_RNGinitInput_stt;
typedef RNGreInput_stt CA_RNGreInput_stt;
typedef RNGaddInput_stt CA_RNGaddInput_stt;

/* API Remapping --------------------------------------------------------------*/
#define CA_RNGreseed      RNGreseed
#define CA_RNGinit        RNGinit
#define CA_RNGfree        RNGfree
#define CA_RNGgenBytes    RNGgenBytes
#define CA_RNGgenWords    RNGgenWords

#endif /* CA_ROUTE_RNG == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< RNG */



#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_RNG_ST_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

