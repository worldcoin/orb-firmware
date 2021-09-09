/**
  ******************************************************************************
  * @file    ca_aes_mbed.c
  * @author  MCD Application Team
  * @brief   This file contains the AES router implementation of
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
#ifndef CA_AES_MBED_C
#define CA_AES_MBED_C

/* Includes ------------------------------------------------------------------*/
#include "ca_aes_mbed.h"

#include "psa/crypto.h"
#include "mbedtls/aes.h"

#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define WRAP_STATE_IDLE     (0U)
#define WRAP_STATE_ENCRYPT  (1U)
#define WRAP_STATE_DECRYPT  (2U)

#define MBEDTLS_SUCCESS 0               /*!< Mbed's return's type*/

/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
#if (defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_MBED)) \
    || (defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_MBED)) \
    || (defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_MBED)) \
    || (defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_MBED))
/**
  * @brief      Change the iv's format from a psa format to a cryptoLib one.
    *           (from a continuous buffer to a buffer dived into 4 sections)
  * @param[in]  *P_pPsaIv: IV in psa format
  * @param[out] *P_pCryptoIv: IV in CryptoLib format
  *
  */
static void wrap_iv_psa_to_crypto(uint8_t  *P_pPsaIv,
                                  uint32_t *P_pCryptoIv)
{
  uint8_t i;
  uint8_t j;

  for (i = 0; i < 4U; i++) /*4 is the size of the array amIv*/
  {
    P_pCryptoIv[i] = 0;
    for (j = 0U; j < (uint8_t)(MBEDTLS_MAX_IV_LENGTH / 4); j++)
    {
      P_pCryptoIv[i] += ((uint32_t)(P_pPsaIv[j + (i * 4U)]) << (8U * ((uint8_t)(MBEDTLS_MAX_IV_LENGTH / 4) - j - 1U)));
    }
  }
}

/**
  * @brief      Import a raw key into a psa struct with the good parameters
  * @param[in,out]
                *P_Key_Handle: Handle of the key
  * @param[in]  P_Psa_Usage: Key usage
  * @param[in]  P_Psa_Algorithm: Algorithm that will be used with the key
  * @param[in]  *P_pAes_Key: The key
  * @param[in]  P_KeySize: The size of the key in bytes
  * @retval     CA_AES_ERR_BAD_PARAMETER: Could not import the key
  * @retval     CA_AES_SUCCESS: Operation Successful
  */
static psa_status_t wrap_import_raw_aes_key_into_psa(psa_key_handle_t *P_Key_Handle,
                                                     psa_key_usage_t P_Psa_Usage,
                                                     psa_algorithm_t  P_Psa_Algorithm,
                                                     const uint8_t *P_pAes_Key,
                                                     uint32_t P_KeySize)
{
  psa_status_t psa_ret_status;
  psa_key_policy_t psa_key_policy = {0};

  psa_ret_status = psa_allocate_key(P_Key_Handle);
  if (psa_ret_status == CA_AES_SUCCESS)
  {
    /*Link the Algorithm with the Key*/
    psa_key_policy_set_usage(&psa_key_policy, P_Psa_Usage, P_Psa_Algorithm);
    psa_ret_status = psa_set_key_policy(*P_Key_Handle, &psa_key_policy);

    if (psa_ret_status == CA_AES_SUCCESS)
    {
      /* Transform the raw key into the right format*/
      psa_ret_status = psa_import_key(*P_Key_Handle,
                                      PSA_KEY_TYPE_AES,
                                      P_pAes_Key,
                                      P_KeySize);
    }
    else
    {
      psa_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    psa_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return psa_ret_status;
}
#endif /* (CA_ROUTE_AES_XXX & CA_ROUTE_MASK) == CA_ROUTE_MBED) */

/* Functions Definition ------------------------------------------------------*/
#if defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_MBED)
#if (CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES Encryption in CBC Mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Buffer with the IV
  * @note       1. P_pAESCBCctx->mKeySize (see CA_AESCBCctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESCBCctx->mIvSize must be set with the size of the IV
  *             (default CA_CRL_AES_BLOCK) prior to calling this function.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_CBC_Encrypt_Init(CA_AESCBCctx_stt *P_pAESCBCctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  psa_status_t psa_ret_status;
  const psa_algorithm_t psa_algorithm = PSA_ALG_CBC_NO_PADDING;

  if ((P_pAESCBCctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESCBCctx->mKeySize == 0)
      || (P_pAESCBCctx->mIvSize == 0))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /*Fill the ctx with pointers*/
  if (P_pAESCBCctx->pmKey != P_pKey)
  {
    P_pAESCBCctx->pmKey = P_pKey;
  }
  if (P_pAESCBCctx->pmIv != P_pIv)
  {
    P_pAESCBCctx->pmIv = P_pIv;
  }

  P_pAESCBCctx->cipher_op = psa_cipher_operation_init();

  /* Initialize MBED crypto library*/
  psa_ret_status = psa_crypto_init();

  if ((psa_ret_status == PSA_SUCCESS) && (aes_ret_status == CA_AES_SUCCESS))
  {
    /*Import the raw AES key into a PSA struct*/
    psa_ret_status = wrap_import_raw_aes_key_into_psa(&(P_pAESCBCctx->psa_key_handle),
                                                      PSA_KEY_USAGE_ENCRYPT,
                                                      psa_algorithm,
                                                      P_pKey,
                                                      (uint32_t) P_pAESCBCctx->mKeySize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /*Init the operation and setup the encryption*/
      psa_ret_status = psa_cipher_encrypt_setup(&(P_pAESCBCctx->cipher_op),
                                                P_pAESCBCctx->psa_key_handle,
                                                psa_algorithm);
      if (psa_ret_status == PSA_SUCCESS)
      {
        psa_ret_status = psa_cipher_set_iv(&(P_pAESCBCctx->cipher_op),
                                           P_pIv,
                                           (uint32_t) P_pAESCBCctx->mIvSize);
        if (psa_ret_status != PSA_SUCCESS)
        {
          aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
        }
      }
      else
      {
        aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Encryption in CBC Mode
  * @param[in]  *P_pAESCBCctx: AES CBC, already initialized, context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, expressed in bytes
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer.
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE:Size of input is less than CA_CRL_AES_BLOCK
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed.
  */
int32_t CA_AES_CBC_Encrypt_Append(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t P_inputSize,
                                  uint8_t *P_pOutputBuffer,
                                  int32_t *P_pOutputSize)
{
  psa_status_t psa_ret_status;
  int32_t aes_ret_status;

  if ((P_pAESCBCctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((uint32_t)P_inputSize < CA_CRL_AES_BLOCK)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  psa_ret_status = psa_cipher_update(&(P_pAESCBCctx->cipher_op),
                                     P_pInputBuffer,
                                     (uint32_t) P_inputSize,
                                     P_pOutputBuffer,
                                     (uint32_t) P_inputSize,
                                     (size_t *)(uint32_t)P_pOutputSize);

  if (psa_ret_status == PSA_SUCCESS)
  {
    aes_ret_status = CA_AES_SUCCESS;
    /*Update of the output context with the new IV*/
    wrap_iv_psa_to_crypto(P_pAESCBCctx->cipher_op.ctx.cipher.iv, P_pAESCBCctx->amIv);
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Finalization of CBC mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC, already initialized, context
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, in bytes
  * @note       This function will write output data.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  */
int32_t CA_AES_CBC_Encrypt_Finish(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  psa_status_t psa_ret_status;
  int32_t aes_ret_status;

  if ((P_pAESCBCctx == NULL) || (P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  psa_ret_status = psa_destroy_key(P_pAESCBCctx->psa_key_handle);
  if (psa_ret_status == PSA_SUCCESS)
  {
    psa_ret_status = psa_cipher_finish(&(P_pAESCBCctx->cipher_op),
                                       P_pOutputBuffer,
                                       (uint32_t) P_pAESCBCctx->mIvSize,
                                       (size_t *)(uint32_t)P_pOutputSize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
/**
  * @brief      Initialization for AES Decryption in CBC Mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Buffer with the IV
  * @note       1. P_pAESCBCctx->mKeySize (see CA_AESCBCctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESCBCctx->mIvSize must be set with the size of the IV
  *             (default CA_CRL_AES_BLOCK) prior to calling this function.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_CBC_Decrypt_Init(CA_AESCBCctx_stt *P_pAESCBCctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  psa_status_t psa_ret_status;
  const psa_algorithm_t psa_algorithm = PSA_ALG_CBC_NO_PADDING;

  if ((P_pAESCBCctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESCBCctx->mIvSize == 0) || (P_pAESCBCctx->mKeySize == 0))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /*Output the IV into the right format*/
  wrap_iv_psa_to_crypto((uint8_t *)(uint32_t)P_pIv, P_pAESCBCctx->amIv);

  P_pAESCBCctx->cipher_op = psa_cipher_operation_init();
  /* Initialize MBED crypto library*/
  psa_ret_status = psa_crypto_init();
  if (psa_ret_status == PSA_SUCCESS)
  {
    /*Import the raw AES key into a PSA struct*/
    psa_ret_status = wrap_import_raw_aes_key_into_psa(&(P_pAESCBCctx->psa_key_handle),
                                                      PSA_KEY_USAGE_DECRYPT,
                                                      psa_algorithm,
                                                      P_pKey,
                                                      (uint32_t) P_pAESCBCctx->mKeySize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /*Cipher setup*/
      psa_ret_status = psa_cipher_decrypt_setup(&P_pAESCBCctx->cipher_op,
                                                P_pAESCBCctx->psa_key_handle,
                                                psa_algorithm);
      if (psa_ret_status == PSA_SUCCESS)
      {
        psa_ret_status = psa_cipher_set_iv(&P_pAESCBCctx->cipher_op,
                                           P_pIv,
                                           (uint32_t) P_pAESCBCctx->mIvSize);
        if (psa_ret_status != PSA_SUCCESS)
        {
          aes_ret_status =  CA_AES_ERR_BAD_PARAMETER;
        }
      }
      else
      {
        aes_ret_status =  CA_AES_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      aes_ret_status =  CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Decryption in CBC Mode
  * @param[in]  *P_pAESCBCctx: AES CBC context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Size of written output data in bytes
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer.
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE: P_inputSize < 16
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed.
  */
int32_t CA_AES_CBC_Decrypt_Append(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  psa_status_t psa_ret_status;

  if ((P_pAESCBCctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((uint32_t)P_inputSize < CA_CRL_AES_BLOCK)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  psa_ret_status = psa_cipher_update(&(P_pAESCBCctx->cipher_op),
                                     P_pInputBuffer,
                                     (uint32_t) P_inputSize,
                                     P_pOutputBuffer,
                                     (uint32_t) P_inputSize,
                                     (size_t *)(uint32_t)P_pOutputSize);
  if (psa_ret_status == PSA_SUCCESS)
  {
    /*Update of the output context with the new IV*/
    wrap_iv_psa_to_crypto(P_pAESCBCctx->cipher_op.ctx.cipher.iv,
                          P_pAESCBCctx->amIv);
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Decryption Finalization of CBC mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC, already initialized, context
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, in bytes
  * @note       This function will write output data.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  */
int32_t CA_AES_CBC_Decrypt_Finish(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;

  if ((P_pAESCBCctx == NULL) || (P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  psa_ret_status = psa_destroy_key(P_pAESCBCctx->psa_key_handle);
  if (psa_ret_status == PSA_SUCCESS)
  {
    psa_ret_status = psa_cipher_finish(&(P_pAESCBCctx->cipher_op),
                                       P_pOutputBuffer,
                                       (uint32_t) P_pAESCBCctx->mIvSize,
                                       (size_t *)(uint32_t)P_pOutputSize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_MBED */

/* ------------------------------------------------------------------------------------------------------------------ */


#if defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_MBED)
#if (CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Inilization for AES-CMAC for Authentication TAG Generation
  * @param[in,out]
  *             *P_pAESCMACctx: AES CMAC context
  * @note       1. P_pAESCMACctx.pmKey (see AESCMACctx_stt) must be set with
  *             a pointer to the AES key before calling this function.
  *             2. P_pAESCMACctx.mKeySize must be set with the size of
  *             the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  *             3. P_pAESCMACctx.mTagSize must be set with the size of
  *             authentication TAG that will be generated by the
  *             CA_AES_CMAC_Encrypt_Finish.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values
  */
int32_t CA_AES_CMAC_Encrypt_Init(CA_AESCMACctx_stt *P_pAESCMACctx)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;
  P_pAESCMACctx->cmac_op = psa_mac_operation_init();
  psa_algorithm_t psa_algorithm = PSA_ALG_CMAC;

  if (P_pAESCMACctx == NULL)
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESCMACctx->pmKey == NULL)
      || (P_pAESCMACctx->mKeySize == 0)
      || (P_pAESCMACctx->mTagSize == 0))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize MBED crypto library*/
  psa_ret_status = psa_crypto_init();
  if (psa_ret_status == PSA_SUCCESS)
  {
    /*Import the raw AES key into a PSA struct*/
    psa_ret_status = wrap_import_raw_aes_key_into_psa(&(P_pAESCMACctx->psa_key_handle),
                                                      PSA_KEY_USAGE_SIGN,
                                                      psa_algorithm,
                                                      P_pAESCMACctx->pmKey,
                                                      (uint32_t) P_pAESCMACctx->mKeySize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /*Setup CMAC*/
      psa_ret_status = psa_mac_sign_setup(&(P_pAESCMACctx->cmac_op),
                                          P_pAESCMACctx->psa_key_handle,
                                          psa_algorithm);
      if (psa_ret_status == PSA_SUCCESS)
      {
        aes_ret_status = CA_AES_SUCCESS;
      }
      else
      {
        aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  return aes_ret_status;
}

/**
  * @brief      AES Encryption in CMAC Mode
  * @param[in,out]
  *             *P_pAESCMACctx AES CMAC, already initialized, context
  * @param[in]  *P_pInputBuffer Input buffer
  * @param[in]  P_inputSize Size of input data in uint8_t (octets)
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE: P_inputSize < 0
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed
  */
int32_t CA_AES_CMAC_Encrypt_Append(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   const uint8_t  *P_pInputBuffer,
                                   int32_t         P_inputSize)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;
  if ((P_pAESCMACctx == NULL) || (P_pInputBuffer == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_inputSize < 0)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  /*Update CMAC*/
  psa_ret_status = psa_mac_update(&(P_pAESCMACctx->cmac_op),
                                  P_pInputBuffer,
                                  (uint32_t) P_inputSize);
  if (psa_ret_status == PSA_SUCCESS)
  {
    aes_ret_status = CA_AES_SUCCESS;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Finalization of CMAC Mode
  * @param[in,out]
  *             *P_pAESCMACctx AES CMAC, already initialized, context
  * @param[out] *P_pOutputBuffer Output buffer
  * @param[out] *P_pOutputSize Size of written output data in uint8_t
  * @note       This function requires P_pAESCMACctx mTagSize to contain valid
  *             value between 1 and 16.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values,
  *             see note
  */
int32_t CA_AES_CMAC_Encrypt_Finish(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   uint8_t        *P_pOutputBuffer,
                                   int32_t        *P_pOutputSize)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;
#if 0
  /* Temporary tag buffer cause mbed supports only 16 bytes long tags */
  uint8_t tagbuffer[16];
  uint32_t tag_length = 16;
#endif /* 0 */

  if ((P_pAESCMACctx == NULL) || (P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESCMACctx->mTagSize < 1) || (P_pAESCMACctx->mTagSize > 16))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /*Free the key*/
  psa_ret_status = psa_destroy_key(P_pAESCMACctx->psa_key_handle);
  if (psa_ret_status == PSA_SUCCESS)
  {
    /*Finish CMAC*/
#if 0
    /* Temporary tag buffer cause mbed supports only 16 bytes long tags */
    psa_ret_status = psa_mac_sign_finish(&(P_pAESCMACctx->cmac_op),
                                         tagbuffer,
                                         (uint32_t) tag_length,
                                         (size_t *)&tag_length);
    if (psa_ret_status == PSA_SUCCESS)
    {
      memcpy(P_pOutputBuffer, tagbuffer, P_pAESCMACctx->mTagSize);
      *P_pOutputSize = P_pAESCMACctx->mTagSize;
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
#else /* 0 */
    psa_ret_status = psa_mac_sign_finish(&(P_pAESCMACctx->cmac_op),
                                         P_pOutputBuffer,
                                         (uint32_t) P_pAESCMACctx->mTagSize,
                                         (size_t *)(uint32_t)P_pOutputSize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
#endif /* 0 */
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
/**
  * @brief      Initialization for AES-CMAC for Authentication TAG Verification
  * @param[in,out]
  *             *P_pAESCMACctx: AES CMAC context
  * @note       1. P_pAESCMACctx.pmKey (see AESCMACctx_stt) must be set with
  *             a pointer to the AES key before calling this function.
  *             2. P_pAESCMACctx.mKeySize must be set with the size of
  *             the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  *             3. P_pAESCMACctx.pmTag must be set with a pointer to
  *             the authentication TAG that will be checked during
  *             CA_AES_CMAC_Decrypt_Finish.
  *             4. P_pAESCMACctx.mTagSize must be set with the size of
  *             authentication TAG.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values,
  *                                   see the note below
  */
int32_t CA_AES_CMAC_Decrypt_Init(CA_AESCMACctx_stt *P_pAESCMACctx)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;
  P_pAESCMACctx->cmac_op = psa_mac_operation_init();
  psa_algorithm_t psa_algorithm = PSA_ALG_CMAC;

  if (P_pAESCMACctx == NULL)
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESCMACctx->pmKey == NULL)
      || (P_pAESCMACctx->pmTag == NULL)
      || (P_pAESCMACctx->mKeySize == 0)
      || (P_pAESCMACctx->mTagSize == 0))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize MBED crypto library*/
  psa_ret_status = psa_crypto_init();
  if (psa_ret_status == PSA_SUCCESS)
  {
    /*Import the raw AES key into a PSA struct*/
    psa_ret_status = wrap_import_raw_aes_key_into_psa(&(P_pAESCMACctx->psa_key_handle),
                                                      PSA_KEY_USAGE_VERIFY,
                                                      psa_algorithm,
                                                      P_pAESCMACctx->pmKey,
                                                      (uint32_t) P_pAESCMACctx->mKeySize);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /*Setup CMAC*/
      psa_ret_status = psa_mac_verify_setup(&(P_pAESCMACctx->cmac_op),
                                            P_pAESCMACctx->psa_key_handle,
                                            psa_algorithm);
      if (psa_ret_status == PSA_SUCCESS)
      {
        aes_ret_status = CA_AES_SUCCESS;
      }
      else
      {
        aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
      }
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}

/**
  * @brief      AES-CMAC Data Processing
  * @param[in,out]
  *             *P_pAESCMACctx AES CMAC, already initialized, context
  * @param[in]  *P_pInputBuffer Input buffer
  * @param[in]  P_inputSize Size of input data in uint8_t (octets)
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE: P_inputSize < 0
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed
  */
int32_t CA_AES_CMAC_Decrypt_Append(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   const uint8_t  *P_pInputBuffer,
                                   int32_t         P_inputSize)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;

  if ((P_pAESCMACctx == NULL) || (P_pInputBuffer == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_inputSize < 0)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  /*Update CMAC*/
  psa_ret_status = psa_mac_update(&(P_pAESCMACctx->cmac_op),
                                  P_pInputBuffer,
                                  (uint32_t) P_inputSize);
  if (psa_ret_status == PSA_SUCCESS)
  {
    aes_ret_status = CA_AES_SUCCESS;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Finalization of CMAC Mode
  * @param[in,out]
  *             *P_pAESCMACctx AES CMAC, already initialized, context
  * @param[out] *P_pOutputBuffer Output buffer NOT USED
  * @param[out] *P_pOutputSize Size of written output data in uint8_t
  * @note       This function requires:
  *             P_pAESGCMctx->pmTag to be set to a valid pointer to the tag
  *             to be checked.
  *             P_pAESCMACctx->mTagSize to contain a valid value between 1 and 16.
  * @retval     CA_AUTHENTICATION_SUCCESSFUL if the TAG is verified
  * @retval     CA_AUTHENTICATION_FAILED if the TAG is not verified
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values,
  *             see note
  */
int32_t CA_AES_CMAC_Decrypt_Finish(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   uint8_t        *P_pOutputBuffer,
                                   int32_t        *P_pOutputSize)
{
  int32_t aes_ret_status;
  psa_status_t psa_ret_status;

  (void)P_pOutputBuffer;
  if ((P_pAESCMACctx == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESCMACctx->mTagSize < 1)
      || (P_pAESCMACctx->mTagSize > 16)
      || (P_pAESCMACctx->pmTag == NULL))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /*Free the key*/
  psa_ret_status = psa_destroy_key(P_pAESCMACctx->psa_key_handle);
  if (psa_ret_status == PSA_SUCCESS)
  {
    /*Finish CMAC*/
    psa_ret_status = psa_mac_verify_finish(&(P_pAESCMACctx->cmac_op),
                                           P_pAESCMACctx->pmTag,
                                           (uint32_t)(P_pAESCMACctx->mTagSize));
    if (psa_ret_status == PSA_ERROR_INVALID_SIGNATURE)
    {
      aes_ret_status = CA_AUTHENTICATION_FAILED;
    }
    else if (psa_ret_status == PSA_SUCCESS)
    {
      *P_pOutputSize = 0;/*P_pAESCMACctx->mTagSize;*/
      aes_ret_status = CA_AUTHENTICATION_SUCCESSFUL;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_MBED */

/* ------------------------------------------------------------------------------------------------------------------ */
#if defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_MBED)
#if (CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES Encryption in ECB Mode
  * @param[in,out]
  *             *P_pAESECBctx: AES ECB context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Not used since no IV is required in ECB
  * @note       1. P_pAESECBctx.mKeySize (see CA_AESECBctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_ECB_Encrypt_Init(CA_AESECBctx_stt *P_pAESECBctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv)
{
  int32_t aes_ret_status;
  int32_t mbedtls_ret_status;

  (void)P_pIv;
  if ((P_pAESECBctx == NULL) || (P_pKey == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_pAESECBctx->mKeySize == 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  mbedtls_aes_init(&P_pAESECBctx->mbedtls_ctx);
  mbedtls_ret_status = mbedtls_aes_setkey_enc(&P_pAESECBctx->mbedtls_ctx,
                                              P_pKey,
                                              (uint32_t)(P_pAESECBctx->mKeySize) * 8U);
  if (mbedtls_ret_status == MBEDTLS_SUCCESS)
  {
    aes_ret_status = CA_AES_SUCCESS;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Encryption in ECB Mode
  * @param[in]  *P_pAESECBctx: AES ECB, already initialized, context. Not used.
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, expressed in bytes
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer.
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE:Size of input is not a multiple
  *                                    of CRL_AES_BLOCK
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed.
  */
int32_t CA_AES_ECB_Encrypt_Append(CA_AESECBctx_stt *P_pAESECBctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  int32_t mbedtls_ret_status;
  uint32_t wrap_computed;

  if ((P_pOutputBuffer == NULL) || (P_pInputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_inputSize < 16) || ((P_inputSize % 16) != 0))
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  for (wrap_computed = 0U; wrap_computed < (uint32_t)P_inputSize; wrap_computed += 16U)
  {
    mbedtls_ret_status = mbedtls_aes_crypt_ecb(&P_pAESECBctx->mbedtls_ctx,
                                               MBEDTLS_AES_ENCRYPT,
                                               &(P_pInputBuffer[wrap_computed]),
                                               &(P_pOutputBuffer[wrap_computed]));
    if (mbedtls_ret_status == MBEDTLS_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
  }
  *P_pOutputSize = P_inputSize;
  return aes_ret_status;
}

/**
  * @brief      AES Finalization of ECB mode
  * @param[in,out]
  *             *P_pAESECBctx: AES ECB, already initialized, context. Not used.
  * @param[out] *P_pOutputBuffer: Output buffer. Not used.
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, in bytes. Not used.
  * @retval     CA_AES_SUCCESS: Operation Successful
  */
int32_t CA_AES_ECB_Encrypt_Finish(CA_AESECBctx_stt *P_pAESECBctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  (void)P_pOutputBuffer;
  (void)P_pOutputSize;
  /*According to documentation does not output any data*/
  mbedtls_aes_free(&P_pAESECBctx->mbedtls_ctx);
  return CA_AES_SUCCESS;
}
#endif /* CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
/**
  * @brief      Initialization for AES Decryption in ECB Mode
  * @param[in,out]
  *             *P_pAESECBctx: AES ECB context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Not used since no IV is required in ECB
  * @note       1. P_pAESECBctx.mKeySize (see CA_AESECBctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_ECB_Decrypt_Init(CA_AESECBctx_stt *P_pAESECBctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv)
{
  int32_t aes_ret_status;
  int32_t mbedtls_ret_status;

  (void)P_pIv;
  if ((P_pAESECBctx == NULL) || (P_pKey == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_pAESECBctx->mKeySize == 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  mbedtls_aes_init(&P_pAESECBctx->mbedtls_ctx);
  mbedtls_ret_status = mbedtls_aes_setkey_dec(&P_pAESECBctx->mbedtls_ctx,
                                              P_pKey,
                                              (uint32_t)(P_pAESECBctx->mKeySize) * 8U);
  if (mbedtls_ret_status == MBEDTLS_SUCCESS)
  {
    aes_ret_status = CA_AES_SUCCESS;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Decryption in ECB Mode
  * @param[in]  *P_pAESECBctx: AES ECB context. Not used.
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Size of written output data in bytes.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer.
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE: P_inputSize is not a multiple of CRL_AES_BLOCK
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed.
  */
int32_t CA_AES_ECB_Decrypt_Append(CA_AESECBctx_stt *P_pAESECBctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  int32_t mbedtls_ret_status;
  uint32_t wrap_computed;

  if ((P_inputSize < 16) || ((P_inputSize % 16) != 0))
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  for (wrap_computed = 0U; wrap_computed < (uint32_t)P_inputSize; wrap_computed += 16U)
  {
    mbedtls_ret_status = mbedtls_aes_crypt_ecb(&P_pAESECBctx->mbedtls_ctx,
                                               MBEDTLS_AES_DECRYPT,
                                               &(P_pInputBuffer[wrap_computed]),
                                               &(P_pOutputBuffer[wrap_computed]));
    if (mbedtls_ret_status == MBEDTLS_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
  }

  *P_pOutputSize = P_inputSize;
  return aes_ret_status;
}

/**
  * @brief      AES Decryption Finalization of ECB mode
  * @param[in,out]
  *             *P_pAESECBctx: AES ECB context. Not used.
  * @param[out] *P_pOutputBuffer: Output buffer. Not used.
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, in bytes. Not used.
  * @retval     CA_AES_SUCCESS: Operation Successful
  */
int32_t CA_AES_ECB_Decrypt_Finish(CA_AESECBctx_stt *P_pAESECBctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  (void)P_pOutputBuffer;
  (void)P_pOutputSize;
  /*According to documentation does not output any data*/
  mbedtls_aes_free(&P_pAESECBctx->mbedtls_ctx);
  return CA_AES_SUCCESS;
}
#endif /* CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_MBED */

/* ------------------------------------------------------------------------------------------------------------------ */

#if defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_MBED)
#if (CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES GCM encryption
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[out] *P_pIv: Buffer with the IV

  * @note       1. P_pAESGCMctx->mKeySize (see AESGCMctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESGCMctx->mIvSize must be set with the size of the IV
  *             (12 is the only supported value) prior to calling this function.
  *             3. P_pAESGCMctx->mTagSize must be set with the size of
  *             authentication TAG that will be generated by the
  *             AES_GCM_Encrypt_Finish.
  *             4. Following recommendation by NIST expressed in section 5.2.1.1
  *             of NIST SP 800-38D, this implementation supports only IV whose
  *             size is of 96 bits.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values,
  *                                  see note
  */
int32_t CA_AES_GCM_Encrypt_Init(CA_AESGCMctx_stt *P_pAESGCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  int32_t mbedtls_status;
  if ((P_pAESGCMctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  /* 12 is the only valid value for the Iv size */
  if ((P_pAESGCMctx->mKeySize == 0)
      || (P_pAESGCMctx->mTagSize == 0)
      || (P_pAESGCMctx->mIvSize != 12))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /*Fill the ctx*/
  if (P_pAESGCMctx->pmKey != P_pKey)
  {
    P_pAESGCMctx->pmKey = P_pKey;
  }
  if (P_pAESGCMctx->pmIv != P_pIv)
  {
    P_pAESGCMctx->pmIv = P_pIv;
  }

  /*Init mbedtls*/
  mbedtls_gcm_init(&P_pAESGCMctx->mbedtls_ctx);
  /*Set the key*/
  mbedtls_status = mbedtls_gcm_setkey(&P_pAESGCMctx->mbedtls_ctx,
                                      MBEDTLS_CIPHER_ID_AES,
                                      P_pKey,
                                      (uint32_t)(P_pAESGCMctx->mKeySize) * 8U); /* 8: to pass from bytes to bits*/
  if (mbedtls_status == MBEDTLS_SUCCESS)
  {
    /* We are doing an encryption, so we set the wrap_is_use var to know it*/
    P_pAESGCMctx->wrap_is_use = WRAP_STATE_ENCRYPT;
    /*Output the IV into the right format*/
    wrap_iv_psa_to_crypto((uint8_t *)(uint32_t)P_pIv, P_pAESGCMctx->amIv);
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return aes_ret_status;
}
/**
  * @brief      AES GCM Encryption function
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM, already initialized, context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Pointer to integer that will contain the size
  *             of written output data, expressed in bytes
  * @retval    CA_AES_SUCCESS: Operation Successful
  * @retval    CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval    CA_AES_ERR_BAD_OPERATION: Append not allowed
  */
int32_t CA_AES_GCM_Encrypt_Append(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t mbedtls_status;
  if ((P_pAESGCMctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  mbedtls_status = mbedtls_gcm_update(&P_pAESGCMctx->mbedtls_ctx,
                                      (uint32_t) P_inputSize,
                                      P_pInputBuffer,
                                      P_pOutputBuffer);
  /*Put data into struct*/
  if (mbedtls_status == MBEDTLS_SUCCESS)
  {
    *P_pOutputSize = P_inputSize;
    /*Transform the IV into the right format*/
    wrap_iv_psa_to_crypto((uint8_t *)(uint32_t)P_pAESGCMctx->pmIv, P_pAESGCMctx->amIv);
  }
  else
  {
    return CA_AES_ERR_BAD_OPERATION;
  }
  return CA_AES_SUCCESS;
}

/**
  * @brief      AES GCM Finalization during encryption, this will create the Authentication TAG
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM, already initialized, context
  * @param[out] *P_pOutputBuffer: Output Authentication TAG
  * @param[out] *P_pOutputSize: Size of returned TAG
  * @note       This function requires P_pAESGCMctx mTagSize to contain a valid
  *             value between 1 and 16
  * @retval     CA_AES_SUCCESS: Operation Successfu
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_GCM_Encrypt_Finish(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  int32_t mbedtls_status;
  if ((P_pAESGCMctx == NULL) || (P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESGCMctx->mTagSize < 0) || (P_pAESGCMctx->mTagSize > 16))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  mbedtls_status = mbedtls_gcm_finish(&P_pAESGCMctx->mbedtls_ctx,
                                      P_pOutputBuffer,
                                      (uint32_t) P_pAESGCMctx->mTagSize);
  if (mbedtls_status == MBEDTLS_SUCCESS)
  {
    *P_pOutputSize = P_pAESGCMctx->mTagSize;
    mbedtls_gcm_free(&P_pAESGCMctx->mbedtls_ctx);
    /*The encryption is finished*/
    P_pAESGCMctx->wrap_is_use = WRAP_STATE_IDLE;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
/**
  * @brief      Initialization for AES GCM Decryption
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[out] *P_pIv: Buffer with the IV

  * @note       1. P_pAESGCMctx->mKeySize (see AESGCMctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES192_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESGCMctx->mIvSize must be set with the size of the IV
  *             (12 is the only supported value) prior to calling this function.
  *             4. Following recommendation by NIST expressed in section 5.2.1.1
  *             of NIST SP 800-38D, this implementation supports only IV whose
  *             size is of 96 bits.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values,
  *                                  see note
  */
int32_t CA_AES_GCM_Decrypt_Init(CA_AESGCMctx_stt *P_pAESGCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv)
{
  int32_t CA_aes_ret_status = CA_AES_SUCCESS;
  int32_t  mbedtls_status;
  if ((P_pAESGCMctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  /* 12 is the only valid value for the Iv size */
  if ((P_pAESGCMctx->mKeySize == 0)
      || (P_pAESGCMctx->mTagSize == 0)
      || (P_pAESGCMctx->mIvSize != 12))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /*Fill the ctx*/
  if (P_pAESGCMctx->pmKey != P_pKey)
  {
    P_pAESGCMctx->pmKey = P_pKey;
  }
  if (P_pAESGCMctx->pmIv != P_pIv)
  {
    P_pAESGCMctx->pmIv = P_pIv;
  }

  /*Init mbedtls*/
  mbedtls_gcm_init(&P_pAESGCMctx->mbedtls_ctx);
  mbedtls_status = mbedtls_gcm_setkey(&P_pAESGCMctx->mbedtls_ctx,
                                      MBEDTLS_CIPHER_ID_AES,
                                      P_pKey,
                                      (uint32_t)(P_pAESGCMctx->mKeySize) * 8UL);
  if (mbedtls_status == MBEDTLS_SUCCESS)
  {
    /* We are doing a decryption, so we set the wrap_is_use var to know it*/
    P_pAESGCMctx->wrap_is_use = WRAP_STATE_DECRYPT;
  }
  else
  {
    CA_aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }

  return CA_aes_ret_status;
}

/**
  * @brief      AES GCM Decryption function
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data in uint8_t (octets)
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Size of written output data in uint8_t
  * @retval    CA_AES_SUCCESS: Operation Successful
  * @retval    CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval    CA_AES_ERR_BAD_OPERATION: Append not allowed
  */
int32_t CA_AES_GCM_Decrypt_Append(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  int32_t mbedtls_status;
  if ((P_pAESGCMctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  mbedtls_status = mbedtls_gcm_update(&P_pAESGCMctx->mbedtls_ctx,
                                      (uint32_t) P_inputSize,
                                      P_pInputBuffer,
                                      P_pOutputBuffer);
  if (mbedtls_status == MBEDTLS_SUCCESS)
  {
    *P_pOutputSize = P_inputSize;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  return aes_ret_status;
}

/**
  * @brief      AES GCM Finalization during decryption, the authentication
  *             TAG will be checked
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM, already initialized, context
  * @param[out] *P_pOutputBuffer: Kept for API compatibility but won't be used,
  *             should be NULL
  * @param[out] *P_pOutputSize: Kept for API compatibility, must be provided
  *             but will be set to zero
  * @note       This function requires:
  *             P_pAESGCMctx->pmTag to be set to a valid pointer to the tag
  *             to be checked.
  *             P_pAESGCMctx->mTagSize to contain a valid value between 1 and 16
  * @retval     CA_AUTHENTICATION_SUCCESSFUL: The Tag is verified
  * @retval     CA_AUTHENTICATION_FAILED: The Tag is not verified
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_GCM_Decrypt_Finish(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status;
  int32_t mbedtls_status;
  (void)P_pOutputBuffer;

  if ((P_pAESGCMctx == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_pAESGCMctx == NULL)
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_pAESGCMctx->pmTag == NULL)
      || ((P_pAESGCMctx->mTagSize > 0) && (P_pAESGCMctx->mTagSize < 16)))
  {
    return CA_AES_ERR_BAD_OPERATION;
  }

  mbedtls_status = mbedtls_gcm_finish(&P_pAESGCMctx->mbedtls_ctx,
                                      (uint8_t *)(uint32_t)P_pAESGCMctx->pmTag,
                                      (uint32_t) P_pAESGCMctx->mTagSize);
  if (mbedtls_status == MBEDTLS_ERR_GCM_AUTH_FAILED)
  {
    aes_ret_status = CA_AUTHENTICATION_FAILED;
  }
  else if (mbedtls_status == MBEDTLS_SUCCESS)
  {
    aes_ret_status = CA_AUTHENTICATION_SUCCESSFUL;
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  mbedtls_gcm_free(&P_pAESGCMctx->mbedtls_ctx);

  /*The encryption is finished*/
  P_pAESGCMctx->wrap_is_use = WRAP_STATE_IDLE;

  /*According to documentation as to be set to 0*/
  *P_pOutputSize = 0;
  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
/**
  * @brief      AES GCM Header processing function
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM, already initialized, context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_OPERATION Append not allowed
  */
int32_t CA_AES_GCM_Header_Append(CA_AESGCMctx_stt *P_pAESGCMctx,
                                 const uint8_t *P_pInputBuffer,
                                 int32_t        P_inputSize)
{
  int32_t aes_ret_status;
  int32_t mbedtls_status;

  /*Manage AAD: input = AAD*/
  if ((P_pAESGCMctx == NULL) || (P_pInputBuffer == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_pAESGCMctx->wrap_is_use == WRAP_STATE_ENCRYPT)
  {
    P_pAESGCMctx->mAADsize = P_inputSize; /*Update done by the ST lib*/
    mbedtls_status = mbedtls_gcm_starts(&P_pAESGCMctx->mbedtls_ctx,
                                        MBEDTLS_GCM_ENCRYPT,
                                        P_pAESGCMctx->pmIv,
                                        (uint32_t) P_pAESGCMctx->mIvSize,
                                        P_pInputBuffer,
                                        (uint32_t) P_pAESGCMctx->mAADsize);
    if (mbedtls_status == MBEDTLS_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
  }

  /*Or are we decrypting something ?*/
  else if (P_pAESGCMctx->wrap_is_use == WRAP_STATE_DECRYPT)
  {
    P_pAESGCMctx->mAADsize = P_inputSize; /*Update done by the ST lib*/
    mbedtls_status = mbedtls_gcm_starts(&P_pAESGCMctx->mbedtls_ctx,
                                        MBEDTLS_GCM_DECRYPT,
                                        P_pAESGCMctx->pmIv,
                                        (uint32_t) P_pAESGCMctx->mIvSize,
                                        P_pInputBuffer,
                                        (uint32_t) P_pAESGCMctx->mAADsize);
    if (mbedtls_status == MBEDTLS_SUCCESS)
    {
      aes_ret_status = CA_AES_SUCCESS;
    }
    else
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
  }
  else
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  return aes_ret_status;
}
#endif /* (CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_MBED */

#endif /* CA_AES_MBED_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

