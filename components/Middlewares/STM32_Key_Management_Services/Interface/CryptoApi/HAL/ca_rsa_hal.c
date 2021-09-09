/**
  ******************************************************************************
  * @file    ca_rsa_hal.c
  * @author  MCD Application Team
  * @brief   This file contains the RSA router implementation of
  *          the Cryptographic API (CA) module to the HAL Cryptographic library.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
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
#ifndef CA_RSA_HAL_C
#define CA_RSA_HAL_C

/* Includes ------------------------------------------------------------------*/
#include "ca_rsa_hal.h"

#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/

/**
  * @brief RSA's Macros
  */
#define WRAP_SHA1_SIZE      20          /*!< Size of a Sha1 digest*/
#define WRAP_SHA256_SIZE    32          /*!< Size of a Sha256 digest*/
#define RSA_PUBKEY_MAXSIZE  528         /*!< Maximum size of RSA's public key in bits*/
#define RSA_PRIVKEY_MAXSIZE 1320        /*!< Maximum size of RSA's private key in bits*/

/**
  * @brief      Used for creating the DER type
  */
#define DER_NB_PUB_TYPE 3               /*!< Number of type in a DER public key*/
#define DER_NB_PUB_SIZE 3               /*!< Number of size in a DER public key*/

/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
#if defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#include "rsa_stm32hal.c"
#endif /* (CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_HAL */

/* Functions Definition ------------------------------------------------------*/

#if defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#if (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_SIGN_ENABLE)
/**
  * @brief      PKCS#1v1.5 RSA Signature Generation Function
  * @param[in]  *P_pPrivKey: RSA private key structure (RSAprivKey_stt)
  * @param[in]  *P_pDigest: The message digest that will be signed
  * @param[in]  P_hashType: Identifies the type of Hash function used
  * @param[out] *P_pSignature: The returned message signature
  * @param[in]  *P_pMemBuf: Pointer to the membuf_stt structure
                that will be used to store the internal values
                required by computation. NOT USED
  * @note       P_pSignature has to point to a memory area of suitable size
  *             (modulus size).
  *             Only RSA 1024 and 2048 with SHA1 or SHA256 are supported
  * @retval     CA_RSA_SUCCESS: Operation Successful
  * @retval     CA_RSA_ERR_BAD_PARAMETER: Some of the inputs were NULL
  * @retval     CA_RSA_ERR_UNSUPPORTED_HASH: Hash type passed not supported
  * @retval     CA_ERR_MEMORY_FAIL: Not enough memory left available
  */
int32_t CA_RSA_PKCS1v15_Sign(const CA_RSAprivKey_stt *P_pPrivKey,
                             const uint8_t *P_pDigest,
                             CA_hashType_et P_hashType,
                             uint8_t *P_pSignature,
                             CA_membuf_stt *P_pMemBuf)
{
  int32_t RSA_ret_status = CA_RSA_SUCCESS;
  rsa_key_t sk;
  const rsa_pkcs_hash_t *hash_ctx;
  uint8_t wrap_hash_size;

  (void)P_pMemBuf;

  if ((P_pPrivKey == NULL)
      || (P_pDigest == NULL)
      || (P_pSignature == NULL))
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }
  if (P_pPrivKey->mModulusSize > RSA_PUBKEY_MAXSIZE)
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }
  /*Define algorithm and hash size with hash type*/
  switch (P_hashType)
  {
    /*Supported Hash*/
    case (CA_E_SHA1):
      wrap_hash_size = WRAP_SHA1_SIZE;
      hash_ctx = &RSA_Hash_SHA1;
      break;
    case (CA_E_SHA256):
      wrap_hash_size = WRAP_SHA256_SIZE;
      hash_ctx = &RSA_Hash_SHA256;
      break;
    default:
      /*Not supported Hash*/
      return CA_RSA_ERR_UNSUPPORTED_HASH;
      break;
  }

  if (RSA_SetKey(&sk, P_pPrivKey->pmModulus, (uint32_t)(P_pPrivKey->mModulusSize), P_pPrivKey->pmExponent,
                 (uint32_t)(P_pPrivKey->mExponentSize)) != RSA_SUCCESS)
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }

  if (RSA_PKCS1v15_Sign(&sk,
                        P_pDigest, wrap_hash_size,
                        hash_ctx,
                        P_pSignature) != RSA_SUCCESS)
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }

  return RSA_ret_status;
}
#endif /* (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_SIGN_ENABLE) */

#if (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_VERIFY_ENABLE)
/**
  * @brief     PKCS#1v1.5 RSA Signature Verification Function
  * @param[in] *P_pPubKey: RSA public key structure (RSApubKey_stt)
  * @param[in] *P_pDigest: The hash digest of the message to be verified
  * @param[in] P_hashType: Identifies the type of Hash function used
  * @param[in] *P_pSignature: The signature that will be checked
  * @param[in] *P_pMemBuf: Pointer to the membuf_stt structure that will be used
  *            to store the internal values required by computation. NOT USED
  * @retval    CA_SIGNATURE_VALID: The Signature is valid
  * @retval    CA_SIGNATURE_INVALID: The Signature is NOT valid
  * @retval    CA_RSA_ERR_BAD_PARAMETER: Some of the inputs were NULL
  * @retval    CA_RSA_ERR_UNSUPPORTED_HASH: The Hash type passed doesn't correspond
  *            to any among the supported ones
  * @retval    CA_ERR_MEMORY_FAIL: Not enough memory left available
  */
int32_t CA_RSA_PKCS1v15_Verify(const CA_RSApubKey_stt *P_pPubKey,
                               const uint8_t *P_pDigest,
                               CA_hashType_et P_hashType,
                               const uint8_t *P_pSignature,
                               CA_membuf_stt *P_pMemBuf)
{
  int32_t RSA_ret_status = CA_SIGNATURE_VALID;
  rsa_key_t sk;
  const rsa_pkcs_hash_t *hash_ctx;
  uint8_t wrap_hash_size;

  (void)P_pMemBuf;

  if ((P_pPubKey == NULL)
      || (P_pDigest == NULL)
      || (P_pSignature == NULL))
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }
  if (P_pPubKey->mModulusSize > RSA_PUBKEY_MAXSIZE)
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }

  /*Define algorithm and hash size with hash type*/
  switch (P_hashType)
  {
    /*Supported Hash*/
    case (CA_E_SHA1):
      wrap_hash_size = WRAP_SHA1_SIZE;
      hash_ctx = &RSA_Hash_SHA1;
      break;
    case (CA_E_SHA256):
      wrap_hash_size = WRAP_SHA256_SIZE;
      hash_ctx = &RSA_Hash_SHA256;
      break;
    default:
      /*Not supported Hash*/
      return CA_RSA_ERR_UNSUPPORTED_HASH;
      break;
  }

  if (RSA_SetKey(&sk, P_pPubKey->pmModulus, (uint32_t)(P_pPubKey->mModulusSize), P_pPubKey->pmExponent,
                 (uint32_t)(P_pPubKey->mExponentSize)) != RSA_SUCCESS)
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }

  if (RSA_PKCS1v15_Verify(&sk,
                          P_pDigest, wrap_hash_size,
                          hash_ctx,
                          P_pSignature, (uint32_t)(P_pPubKey->mModulusSize)) != RSA_SUCCESS)
  {
    return CA_RSA_ERR_BAD_PARAMETER;
  }

  return RSA_ret_status;
}
#endif /* (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_VERIFY_ENABLE) */
#endif /* (CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_HAL */




#endif /* CA_RSA_HAL_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

