/**
  ******************************************************************************
  * @file    ca_rsa_mbed.c
  * @author  MCD Application Team
  * @brief   This file contains the RSA router implementation of
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
#ifndef CA_RSA_MBED_C
#define CA_RSA_MBED_C

/* Includes ------------------------------------------------------------------*/
#include "ca_rsa_mbed.h"

#include "mbedtls/pk.h"
#include "mbedtls/pk_internal.h"

#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/

#if defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED)
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
/**
  * @brief      Change the key's format from a PUBLIC rsa key to a der one.
  * @param[out] *P_pDer: Der key
  * @param[in]  *P_pRsa_Modulus: Modulus
  * @param[in]  *P_pRsa_PubExponent: Public Exponent
  * @param[out] *P_pDer_size: the size of the Der key
  * @param[in]  P_Rsa_Modulus_Size: Size of the modulus
  * @param[in]  P_Rsa_PubExponent_Size: Size of the public exponent
  * @retval     WRAP_FAILURE: Operation not possible
  * @retval     WRAP_SUCCESS: Operation Successful
  */
static uint8_t wrap_pubkey_rsa_to_der(uint8_t  *P_pDer,
                                      uint8_t  *P_pRsa_Modulus,
                                      uint8_t  *P_pRsa_PubExponent,
                                      uint32_t  *P_pDer_size,
                                      int32_t  P_Rsa_Modulus_Size,
                                      int32_t  P_Rsa_PubExponent_Size)
{
  uint32_t wrap_pos_buffer;
  uint32_t wrap_out_size;
  uint32_t temp_modulus_size;
  uint32_t temp_pubexponent_size;
  uint32_t temp_output_size;
  uint8_t wrap_bytes_modulus = 0;
  uint8_t wrap_bytes_exponent = 0;
  uint8_t wrap_bytes_out = 0;
  uint8_t i;

  /* Check parameters */
  if ((P_pDer == NULL)
      || (P_pRsa_Modulus == NULL)
      || (P_pRsa_PubExponent == NULL))
  {
    return WRAP_FAILURE;
  }
  if ((P_Rsa_Modulus_Size < 0) || (P_Rsa_PubExponent_Size < 0))
  {
    return WRAP_FAILURE;
  }

  temp_modulus_size = (uint32_t)P_Rsa_Modulus_Size;
  temp_pubexponent_size = (uint32_t)P_Rsa_PubExponent_Size;
  /* What is the size needed to store the length of the modulus in bytes ? */
  while (temp_modulus_size > 0UL)
  {
    temp_modulus_size = temp_modulus_size >> 8U;
    wrap_bytes_modulus++;
    /* temp_modulus_size > 0:
     * - add one byte to the size field to store the temp_modulus_size lower 8 bits
     * - remove the temp_modulus_size lower 8 bits and check again
     */
  };
  /* What is the size needed to store the length of the exponent in bytes ? */
  while (temp_pubexponent_size > 0UL)
  {
    temp_pubexponent_size = temp_pubexponent_size >> 8U;
    wrap_bytes_exponent++;
    /* temp_pubexponent_size > 0:
     * - add one byte to the size field to store the temp_pubexponent_size lower 8 bits
     * - remove the temp_pubexponent_size lower 8 bits and check again
     */
  };

  /*Format RSA to public "DER":
   0x30   0x8x size  0x02 0x8x size     Modulus   0x02    0x8x size     Exponent
   Type |           |Type|   Length     | Value  |Type|    Length     | Value   */

  /*Because we don't know the output size yet,
   we have to use the max bytes size (i.e. 4),
   and after sustract it and add the real one*/
  wrap_out_size = ((uint32_t)P_Rsa_Modulus_Size + (uint32_t)P_Rsa_PubExponent_Size + (uint32_t)DER_NB_PUB_TYPE \
                   + (uint32_t)DER_NB_PUB_SIZE + (uint32_t)wrap_bytes_modulus + (uint32_t)wrap_bytes_exponent + 4UL);
  if (wrap_out_size > (uint32_t)RSA_PUBKEY_MAXSIZE)
  {
    return WRAP_FAILURE;
  }

  temp_output_size = wrap_out_size;
  /* What is the size needed to store the length of the output in bytes ? */
  while (temp_output_size > 0U)
  {
    temp_output_size = temp_output_size >> 8U;
    wrap_bytes_out++;
    /* temp_output_size > 0:
     * - add one byte to the size field to store the temp_output_size lower 8 bits
     * - remove the temp_output_size lower 8 bits and check again
     */
  };
  wrap_out_size = wrap_out_size - 4U + wrap_bytes_out;

  /* It's a DER specific to MBED */
  P_pDer[0] = 0x30;
  P_pDer[1] = wrap_bytes_out + (uint8_t)0x80; /* Number of bytes of total der key */
  for (i = wrap_bytes_out; i > 0UL; i--)
  {
    /* The size of what is left of the output */
    P_pDer[1U + i] = (uint8_t)((wrap_out_size - 2U - wrap_bytes_out) >> (8U * (wrap_bytes_out - i)));
  }

  P_pDer[2U + wrap_bytes_out] = 0x02; /* INTEGER */
  P_pDer[3U + wrap_bytes_out] = wrap_bytes_modulus + (uint8_t)0x80; /* Number of bytes of data */
  for (i = wrap_bytes_modulus; i > 0U; i--)
  {
    /* Modulus Size */
    P_pDer[3U + wrap_bytes_out + i] = (uint8_t)((uint32_t)(P_Rsa_Modulus_Size) >> (8U * (wrap_bytes_modulus - i)));
  }
  /* Write the modulus into the buffer */
  (void)memcpy(&P_pDer[4U + wrap_bytes_out + wrap_bytes_modulus],
               P_pRsa_Modulus,
               (uint32_t) P_Rsa_Modulus_Size);

  /* Save the position */
  wrap_pos_buffer = 4UL + wrap_bytes_out + wrap_bytes_modulus + (uint32_t)P_Rsa_Modulus_Size;
  P_pDer[wrap_pos_buffer] = 0x02; /* INTEGER */
  P_pDer[1U + wrap_pos_buffer] = wrap_bytes_exponent + (uint8_t)0x80; /* Number of bytes of data */
  for (i = wrap_bytes_exponent; i > 0U; i--)
  {
    /* Exponent Size */
    P_pDer[1U + wrap_pos_buffer + i] = (uint8_t)((uint32_t)P_Rsa_PubExponent_Size >> (8U * (wrap_bytes_exponent - i)));
  }
  /* Write the public exponent into the buffer */
  (void)memcpy(&P_pDer[2U + wrap_pos_buffer + wrap_bytes_exponent],
               P_pRsa_PubExponent,
               (uint32_t) P_Rsa_PubExponent_Size);
  /* Update constructed DER size */
  *P_pDer_size = wrap_out_size;
  return WRAP_SUCCESS;
}

/**
  * @brief      Change the key's format from a rsa KEYPAIR to a der one.
  * @param[out] *P_pDer: Der key
  * @param[in]  *P_pRsa_Modulus: Modulus
  * @param[in]  *P_pRsa_PrivExponent: Private Exponent
  * @param[in]  *P_pRsa_PubExponent: Public Exponent
  * @param[out] *P_pDer_size: the size of the Der key
  * @param[in]  P_Rsa_Modulus_Size: Size of the modulus
  * @param[in]  P_Rsa_PrivExponent_Size: Size of the private exponent
  * @param[in]  P_Rsa_PubExponent_Size: Size of the public exponent
  * @param[out] *P_plength: Size of the output buffer
  * note        The output buffer write data starting at the end
  * @retval     WRAP_FAILURE: Operation not possible
  * @retval     WRAP_SUCCESS: Operation Successful
  * @retaval    WRAP_BAD_KEY: Bad key
  */
static uint8_t wrap_keypair_rsa_to_der(uint8_t  *P_pDer,
                                       uint8_t  *P_pRsa_Modulus,
                                       uint8_t  *P_pRsa_PrivExponent,
                                       uint8_t  *P_pRsa_PubExponent,
                                       uint32_t P_pDer_size,
                                       int32_t  P_Rsa_Modulus_Size,
                                       int32_t  P_Rsa_PrivExponent_Size,
                                       int32_t  P_Rsa_PubExponent_Size,
                                       uint32_t *P_plength)
{
  int32_t mbedtls_ret;
  uint8_t wrap_ret;
  mbedtls_mpi N;
  mbedtls_mpi D;
  mbedtls_mpi E;
  mbedtls_pk_context pk_ctx;
  mbedtls_rsa_context *rsa_ctx = NULL;

  /* Check parameters */
  if ((P_pDer == NULL)
      || (P_pRsa_Modulus == NULL)
      || (P_pRsa_PrivExponent == NULL)
      || (P_pRsa_PubExponent == NULL)
      || (P_plength == NULL))
  {
    return  WRAP_FAILURE;
  }
  if ((P_Rsa_Modulus_Size < 0)
      || (P_Rsa_PrivExponent_Size < 0)
      || (P_Rsa_PubExponent_Size < 0))
  {
    return WRAP_FAILURE;
  }

  /* Init Mpi */
  mbedtls_mpi_init(&N);  /* N = Modulus */
  mbedtls_mpi_init(&D);  /* D = Private exponent */
  mbedtls_mpi_init(&E);  /* E = Public exponent */

  /* Transform key's components into MPI */
  /* Pointer already checked */
  wrap_ret = uint8_t_to_mpi(&N, P_pRsa_Modulus, P_Rsa_Modulus_Size);
  wrap_ret |= uint8_t_to_mpi(&D, P_pRsa_PrivExponent, P_Rsa_PrivExponent_Size);
  wrap_ret |= uint8_t_to_mpi(&E, P_pRsa_PubExponent, P_Rsa_PubExponent_Size);

  if (wrap_ret != WRAP_SUCCESS)
  {
    /* Upon error, release mpi elements */
    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&D);
    mbedtls_mpi_free(&E);
  }

  /* Init contexts */
  mbedtls_pk_init(&pk_ctx);
  /* Prepare PK context to host RSA information */
  mbedtls_ret = mbedtls_pk_setup(&pk_ctx, &mbedtls_rsa_info);
  if (mbedtls_ret == 0)
  {
    /* Get a RSA context from a PK one */
    rsa_ctx = mbedtls_pk_rsa(pk_ctx);
    /* Import RSA key into context */
    mbedtls_ret = mbedtls_rsa_import(rsa_ctx, &N, NULL, NULL, &D, &E);
    if (mbedtls_ret == 0)
    {
      /* Complete the key's components */
      mbedtls_ret = mbedtls_rsa_complete(rsa_ctx);
      if (mbedtls_ret == 0)
      {
        /* Export to a DER format the keypair */
        *P_plength = (uint32_t) mbedtls_pk_write_key_der(&pk_ctx, P_pDer, P_pDer_size);
        wrap_ret = WRAP_SUCCESS;
      }
      else
      {
        wrap_ret = WRAP_FAILURE;
      }
    }
    else
    {
      /* Import failed */
      wrap_ret = WRAP_BAD_KEY;
    }
  }
  else
  {
    /* Contexts preparation failed */
    wrap_ret = WRAP_FAILURE;
  }

  /* free the contexts and mpi elements*/
  mbedtls_rsa_free(rsa_ctx);
  mbedtls_pk_free(&pk_ctx);
  mbedtls_mpi_free(&N);
  mbedtls_mpi_free(&D);
  mbedtls_mpi_free(&E);
  return wrap_ret;
}

/**
  * @brief      Import a DER public key to a psa struct
  * @param[in,out]
                *P_Key_Handle: Handle of the key
  * @param[in]  P_Psa_Usage: Key usage
  * @param[in]  P_Psa_Algorithm: Algorithm that will be used with the key
  * @param[in]  *P_pDer_pubKey: The key
  * @param[in]  P_KeySize: The size of the key in bytes
  * @retval     CA_RSA_ERR_BAD_PARAMETER: Could not import the key
  * @retval     CA_RSA_SUCCESS: Operation Successful
  */
static psa_status_t wrap_import_der_Key_into_psa(psa_key_handle_t *P_Key_Handle,
                                                 psa_key_usage_t P_Psa_Usage,
                                                 psa_algorithm_t  P_Psa_Algorithm,
                                                 psa_key_type_t P_Psa_key_type,
                                                 const uint8_t *P_pDer_pubKey,
                                                 uint32_t P_KeySize)
{
  psa_status_t psa_ret_status;
  psa_key_policy_t key_policy = {0};

  /* Import key into psa struct */
  /* Allocate key storage */
  psa_ret_status = psa_allocate_key(P_Key_Handle);
  if (psa_ret_status == PSA_SUCCESS)
  {
    /* Set algorithm for this key */
    psa_key_policy_set_usage(&key_policy, P_Psa_Usage, P_Psa_Algorithm);
    psa_ret_status = psa_set_key_policy(*P_Key_Handle, &key_policy);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /* Import key into storage */
      psa_ret_status = psa_import_key(*P_Key_Handle,
                                      P_Psa_key_type,
                                      P_pDer_pubKey,
                                      P_KeySize);
      if (psa_ret_status == PSA_SUCCESS)
      {
        psa_ret_status = CA_RSA_SUCCESS;
      }
      else
      {
        psa_ret_status = CA_RSA_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      /* Algorithm setting failed */
      psa_ret_status = CA_RSA_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    /* Key storage allocation failed */
    psa_ret_status = CA_RSA_ERR_BAD_PARAMETER;
  }
  return psa_ret_status;
}
/* Functions Definition ------------------------------------------------------*/

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
  int32_t RSA_ret_status;
  psa_status_t psa_ret_status;
  psa_algorithm_t psa_algorithm;
  psa_key_handle_t psa_key_handle = 0;
  uint8_t wrap_hash_size;
  uint8_t DER_privKey[RSA_PRIVKEY_MAXSIZE];
  int32_t wrap_signature_lenth = 0;
  uint8_t wrap_ret_status;
  uint32_t length = 0;
  uint32_t wrap_der_size = sizeof(DER_privKey);

  (void)P_pMemBuf;
  /* Check parameters */
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
  /* Define algorithm and hash size with hash type */
  switch (P_hashType)
  {
    /* Supported Hash */
    case (CA_E_SHA1):
      wrap_hash_size = WRAP_SHA1_SIZE;
      psa_algorithm = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_1);
      break;
    case (CA_E_SHA256):
      wrap_hash_size = WRAP_SHA256_SIZE;
      psa_algorithm = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
      break;
    default:
      /*Not supported Hash*/
      return CA_RSA_ERR_UNSUPPORTED_HASH;
  }

  /* Fill a DER structure from a psa key */
  wrap_ret_status = wrap_keypair_rsa_to_der(DER_privKey,
                                            P_pPrivKey->pmModulus,
                                            P_pPrivKey->pmExponent,
                                            P_pPrivKey->pmPubExponent,
                                            wrap_der_size,
                                            P_pPrivKey->mModulusSize,
                                            P_pPrivKey->mExponentSize,
                                            P_pPrivKey->mPubExponentSize,
                                            &length);
  if (wrap_ret_status == WRAP_SUCCESS)
  {
    /* The size of DER_privKey is upper than the real size
    and the data are written starting at the end of the buffer */
    psa_ret_status = wrap_import_der_Key_into_psa(&psa_key_handle,
                                                  PSA_KEY_USAGE_SIGN,
                                                  psa_algorithm,
                                                  PSA_KEY_TYPE_RSA_KEYPAIR,
                                                  &DER_privKey[wrap_der_size - length],
                                                  length);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /* Sign */
      psa_ret_status = psa_asymmetric_sign(psa_key_handle,
                                           psa_algorithm,
                                           P_pDigest,
                                           wrap_hash_size,
                                           P_pSignature,
                                           (uint32_t) P_pPrivKey->mModulusSize,
                                           (size_t *)(uint32_t)&wrap_signature_lenth);

      if (psa_ret_status == PSA_ERROR_INSUFFICIENT_MEMORY)
      {
        RSA_ret_status =  CA_ERR_MEMORY_FAIL;
      }
      else if (psa_ret_status == PSA_SUCCESS)
      {
        RSA_ret_status =  CA_RSA_SUCCESS;
      }
      /* In case of other return status*/
      else
      {
        RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
      }

      /*We won't be able to reuse the key, so we destroy it*/
      psa_ret_status = psa_destroy_key(psa_key_handle);
      if (psa_ret_status != PSA_SUCCESS)
      {
        return CA_RSA_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      /* Key importation into PSA failed */
      RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
    }
  }
  else if (wrap_ret_status == WRAP_BAD_KEY)
  {
    /* RSA to DER transformation failed: key format */
    RSA_ret_status = CA_RSA_ERR_BAD_KEY;
  }
  else
  {
    /* RSA to DER transformation failed: general error */
    RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
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
  int32_t RSA_ret_status;
  uint32_t pwrap_der_size = 0;
  psa_status_t psa_ret_status;
  psa_status_t psa_ret_status_tp;
  psa_algorithm_t psa_algorithm;
  psa_key_handle_t psa_key_handle = 0;
  uint8_t wrap_ret_status;
  uint8_t wrap_hash_size;
  uint8_t DER_pubKey[RSA_PUBKEY_MAXSIZE];

  (void)P_pMemBuf;
  /* Check parameters */
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

  switch (P_hashType)
  {
    /* Supported Hash */
    case (CA_E_SHA1):
      wrap_hash_size = WRAP_SHA1_SIZE;
      psa_algorithm = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_1);
      break;
    case (CA_E_SHA256):
      wrap_hash_size = WRAP_SHA256_SIZE;
      psa_algorithm = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
      break;
    default:
      /* Not supported Hash */
      return CA_RSA_ERR_UNSUPPORTED_HASH;
  }

  /* Change the key to a DER format */
  wrap_ret_status = wrap_pubkey_rsa_to_der(DER_pubKey,
                                           P_pPubKey->pmModulus,
                                           P_pPubKey->pmExponent,
                                           &pwrap_der_size,
                                           P_pPubKey->mModulusSize,
                                           P_pPubKey->mExponentSize);
  if (wrap_ret_status == WRAP_SUCCESS)
  {
    /* Import the key into psa */
    psa_ret_status = wrap_import_der_Key_into_psa(&psa_key_handle,
                                                  PSA_KEY_USAGE_VERIFY,
                                                  psa_algorithm,
                                                  PSA_KEY_TYPE_RSA_PUBLIC_KEY,
                                                  DER_pubKey,
                                                  pwrap_der_size);
    if (psa_ret_status == CA_RSA_SUCCESS)
    {
      /* Verify */
      psa_ret_status = psa_asymmetric_verify(psa_key_handle,
                                             psa_algorithm,
                                             P_pDigest,
                                             wrap_hash_size,
                                             P_pSignature,
                                             (uint32_t) P_pPubKey->mModulusSize);

      /* We won't be able to reuse the key, so we destroy it */
      psa_ret_status_tp = psa_destroy_key(psa_key_handle);
      if (psa_ret_status_tp == PSA_SUCCESS)
      {
        if (psa_ret_status == PSA_ERROR_INVALID_SIGNATURE)
        {
          RSA_ret_status = CA_SIGNATURE_INVALID;
        }
        else if (psa_ret_status == PSA_ERROR_INSUFFICIENT_MEMORY)
        {
          RSA_ret_status = CA_ERR_MEMORY_FAIL;
        }
        else if (psa_ret_status == PSA_ERROR_INVALID_ARGUMENT)
        {
          RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
        }
        /* Success */
        else if (psa_ret_status == PSA_SUCCESS)
        {
          RSA_ret_status =  CA_SIGNATURE_VALID;
        }
        /* In case of other return status*/
        else
        {
          RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
        }
      }
      else
      {
        /* Key destruction failed */
        RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      /* Key importation failed */
      RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    /* Key conversion failed */
    RSA_ret_status = CA_RSA_ERR_BAD_PARAMETER;
  }
  return RSA_ret_status;
}
#endif /* (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_VERIFY_ENABLE) */
#endif /* (CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED */




#endif /* CA_RSA_MBED_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

