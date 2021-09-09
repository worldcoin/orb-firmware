/**
  ******************************************************************************
  * @file    ca_hash_mbed.h
  * @author  MCD Application Team
  * @brief   This file contains the HASH router includes and definitions of
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
#ifndef CA_HASH_MBED_H
#define CA_HASH_MBED_H

#if !defined(CA_HASH_H)
#error "This file shall never be included directly by application but through the main header ca_hash.h"
#endif /* CA_HASH_H */

/* Configuration defines -----------------------------------------------------*/

/* Define & types if not already defined */
#if !defined(CA_HASH_ST_H)
typedef enum CA_HashType_e
{
  CA_E_SHA1,         /*!< SHA-1   */
  CA_E_SHA256,       /*!< SHA-256 */
} CA_hashType_et;

typedef enum CA_HashFlags_e
{
  CA_E_HASH_DEFAULT = (uint32_t)(0x00000000),   /*!< User Flag: No flag specified.
                                                 This is the default value that should be set to this flag  */
  CA_E_HASH_DONT_PERFORM_KEY_SCHEDULE = (uint32_t)(0x00000001),  /*!< User Flag: Used to force the init to not reperform key processing in HMAC mode  */
  CA_E_HASH_OPERATION_COMPLETED = (uint32_t)(0x00000002),   /*!< Internal Flag: used to check that the Finish function has been already called */
  CA_E_HASH_NO_MORE_APPEND_ALLOWED = (uint32_t)(0x00000004),  /*!< Internal Flag: it is set when the last append has been called.
                                                              Used where the append is called with an InputSize not
                                                              multiple of the block size, which means that is the last
                                                              input */
} CA_HashFlags_et;
#endif /* CA_HASH_ST_H */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> HASH SHA1 */
#if defined(CA_ROUTE_HASH_SHA1) && ((CA_ROUTE_HASH_SHA1 & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/
#include "mbedtls/sha1.h"

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  uint32_t mContextId;    /*!<  Unique ID of this context. \b Not \b used in current implementation  */
  CA_HashFlags_et mFlags; /*!<  32 bit mFlags, used to perform keyschedule */
  int32_t  mTagSize;      /*!<  Size of the required Digest */
  /* Additional information to maintain context to communicate with Mbed */
  mbedtls_sha1_context hash_ctx;
} CA_SHA1ctx_stt;

/* Exported functions -------------------------------------------------------*/
int32_t CA_SHA1_Init(CA_SHA1ctx_stt *P_pSHA1ctx);
int32_t CA_SHA1_Append(CA_SHA1ctx_stt *P_pSHA1ctx,
                       const uint8_t *P_pInputBuffer,
                       int32_t P_inputSize);
int32_t CA_SHA1_Finish(CA_SHA1ctx_stt *P_pSHA1ctx,
                       uint8_t *P_pOutputBuffer,
                       int32_t *P_pOutputSize);
#endif /* (CA_ROUTE_HASH_SHA1 & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< HASH SHA1 */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> HASH SHA256 */
#if defined(CA_ROUTE_HASH_SHA256) && ((CA_ROUTE_HASH_SHA256 & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/
#include "mbedtls/sha256.h"

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  uint32_t mContextId;    /*!<  Unique ID of this context. \b Not \b used in current implementation  */
  CA_HashFlags_et mFlags; /*!<  32 bit mFlags, used to perform keyschedule */
  int32_t  mTagSize;      /*!<  Size of the required Digest */
  /* Additional information to maintain context to communicate with Mbed */
  mbedtls_sha256_context hash_ctx;
} CA_SHA256ctx_stt;

/* Exported functions -------------------------------------------------------*/
int32_t CA_SHA256_Init(CA_SHA256ctx_stt *P_pSHA256ctx);
int32_t CA_SHA256_Append(CA_SHA256ctx_stt *P_pSHA256ctx,
                         const uint8_t *P_pInputBuffer,
                         int32_t P_inputSize);
int32_t CA_SHA256_Finish(CA_SHA256ctx_stt *P_pSHA256ctx,
                         uint8_t *P_pOutputBuffer,
                         int32_t *P_pOutputSize);
#endif /* (CA_ROUTE_HASH_SHA256 & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< HASH SHA256 */



#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_HASH_MBED_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

