/**
  ******************************************************************************
  * @file    ca_hash_mbed.c
  * @author  MCD Application Team
  * @brief   This file contains the HASH router implementation of
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
#ifndef CA_HASH_MBED_C
#define CA_HASH_MBED_C

/* Includes ------------------------------------------------------------------*/
#include "ca_hash_mbed.h"


/* Private defines -----------------------------------------------------------*/


/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Functions Definition ------------------------------------------------------*/
#if defined(CA_ROUTE_HASH_SHA1) && ((CA_ROUTE_HASH_SHA1 & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/**
  * @brief      Initialize a new SHA1 context
  * @param[in, out]
  *             *P_pSHA1ctx: The context that will be initialized. Not used
  * @retval     CA_HASH_SUCCESS: Operation Successful
  * @retval     CA_HASH_ERR_BAD_PARAMETER
  */
int32_t CA_SHA1_Init(CA_SHA1ctx_stt *P_pSHA1ctx)
{
  int32_t hash_ret_status;

  mbedtls_sha1_init(&(P_pSHA1ctx->hash_ctx));
  if (mbedtls_sha1_starts_ret(&(P_pSHA1ctx->hash_ctx)) != 0)
  {
    hash_ret_status = CA_HASH_ERR_BAD_PARAMETER;
  }
  else
  {
    hash_ret_status = CA_HASH_SUCCESS;
  }
  return hash_ret_status;
}

/**
  * @brief      Process input data and update a SHA1ctx_stt
  * @param[in, out]
    *           *P_pSHA1ctx: SHA1 context that will be updated. Not used
  * @param[in]  *P_pInputBuffer: The data that will be processed using SHA1
  * @param[in]  P_inputSize: Size of input data expressed in bytes
  * @note       This function can be called multiple times with no restrictions
  *             on the value of P_inputSize
  * @retval     CA_HASH_SUCCESS: Operation Successful
  * @retval     CA_HASH_ERR_BAD_PARAMETER: P_pInputBuffer == NULL
  * @retval     CA_HASH_ERR_BAD_OPERATION
  */
int32_t CA_SHA1_Append(CA_SHA1ctx_stt *P_pSHA1ctx,
                       const uint8_t *P_pInputBuffer,
                       int32_t P_inputSize)
{
  int32_t hash_ret_status;

  if (P_pInputBuffer == NULL)
  {
    return CA_HASH_ERR_BAD_PARAMETER;
  }

  if (mbedtls_sha1_update_ret(&(P_pSHA1ctx->hash_ctx), P_pInputBuffer, (size_t)P_inputSize) != 0)
  {
    hash_ret_status = CA_HASH_ERR_BAD_OPERATION;
  }
  else
  {
    hash_ret_status = CA_HASH_SUCCESS;
  }

  return hash_ret_status;
}

/**
  * @brief
  * @param[in,out]
  *             *P_pSHA1ctx: HASH contex
  * @param[in]  *P_pOutputBuffer: Buffer that will contain the digest
  * @param[in]  *P_pOutputSize: Size of the data written to P_pOutputBuffer
  * @retval     CA_HASH_SUCCESS: Operation Successful
  * @retval     CA_HASH_ERR_BAD_PARAMETER: P_pOutputBuffer == NULL || P_pOutputSize == NULL
  * @retval     CA_HASH_ERR_BAD_CONTEXT
  */
int32_t CA_SHA1_Finish(CA_SHA1ctx_stt *P_pSHA1ctx,
                       uint8_t *P_pOutputBuffer,
                       int32_t *P_pOutputSize)
{
  int32_t hash_ret_status;

  if ((P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_HASH_ERR_BAD_PARAMETER;
  }

  if (mbedtls_sha1_finish_ret(&(P_pSHA1ctx->hash_ctx), P_pOutputBuffer) != 0)
  {
    hash_ret_status = CA_HASH_ERR_BAD_CONTEXT;
  }
  else
  {
    *P_pOutputSize = (int32_t)CA_CRL_SHA1_SIZE;
    hash_ret_status = CA_HASH_SUCCESS;
  }

  return hash_ret_status;
}
#endif /* (CA_ROUTE_HASH_SHA1 & CA_ROUTE_MASK) == CA_ROUTE_MBED */


#if defined(CA_ROUTE_HASH_SHA256) && ((CA_ROUTE_HASH_SHA256 & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/**
  * @brief      Initialize a new SHA256 context
  * @param[in, out]
  *             *P_pSHA256ctx: The context that will be initialized. Not used
  * @retval     CA_HASH_SUCCESS: Operation Successful
  * @retval     CA_HASH_ERR_BAD_PARAMETER
  */
int32_t CA_SHA256_Init(CA_SHA256ctx_stt *P_pSHA256ctx)
{
  int32_t hash_ret_status;

  mbedtls_sha256_init(&(P_pSHA256ctx->hash_ctx));
  if (mbedtls_sha256_starts_ret(&(P_pSHA256ctx->hash_ctx), 0 /* is224 = 0 for SHA256 */) != 0)
  {
    hash_ret_status = CA_HASH_ERR_BAD_PARAMETER;
  }
  else
  {
    hash_ret_status = CA_HASH_SUCCESS;
  }

  return hash_ret_status;
}

/**
  * @brief      Process input data and update a SHA256ctx_stt
  * @param[in, out]
    *           *P_pSHA256ctx: SHA256 context that will be updated. Not used
  * @param[in]  *P_pInputBuffer: The data that will be processed using SHA256
  * @param[in]  P_inputSize: Size of input data expressed in bytes
  * @note       This function can be called multiple times with no restrictions
  *             on the value of P_inputSize
  * @retval     CA_HASH_SUCCESS: Operation Successful
  * @retval     CA_HASH_ERR_BAD_PARAMETER: P_pInputBuffer == NULL
  * @retval     CA_HASH_ERR_BAD_OPERATION
  */
int32_t CA_SHA256_Append(CA_SHA256ctx_stt *P_pSHA256ctx,
                         const uint8_t *P_pInputBuffer,
                         int32_t P_inputSize)
{
  int32_t hash_ret_status;

  if (P_pInputBuffer == NULL)
  {
    return CA_HASH_ERR_BAD_PARAMETER;
  }

  if (mbedtls_sha256_update_ret(&(P_pSHA256ctx->hash_ctx), P_pInputBuffer, (size_t)P_inputSize) != 0)
  {
    hash_ret_status = CA_HASH_ERR_BAD_OPERATION;
  }
  else
  {
    hash_ret_status = CA_HASH_SUCCESS;
  }

  return hash_ret_status;
}

/**
  * @brief
  * @param[in,out]
  *             *P_pSHA256ctx: HASH contex
  * @param[in]  *P_pOutputBuffer: Buffer that will contain the digest
  * @param[in]  *P_pOutputSize: Size of the data written to P_pOutputBuffer
  * @retval     CA_HASH_SUCCESS: Operation Successful
  * @retval     CA_HASH_ERR_BAD_PARAMETER: P_pOutputBuffer == NULL || P_pOutputSize == NULL
  * @retval     CA_HASH_ERR_BAD_CONTEXT
  */
int32_t CA_SHA256_Finish(CA_SHA256ctx_stt *P_pSHA256ctx,
                         uint8_t *P_pOutputBuffer,
                         int32_t *P_pOutputSize)
{
  int32_t hash_ret_status;

  if ((P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_HASH_ERR_BAD_PARAMETER;
  }

  if (mbedtls_sha256_finish_ret(&(P_pSHA256ctx->hash_ctx), P_pOutputBuffer) != 0)
  {
    hash_ret_status = CA_HASH_ERR_BAD_CONTEXT;
  }
  else
  {
    *P_pOutputSize = (int32_t)CA_CRL_SHA256_SIZE;
    hash_ret_status = CA_HASH_SUCCESS;
  }

  return hash_ret_status;
}
#endif /* (CA_ROUTE_HASH_SHA256 & CA_ROUTE_MASK) == CA_ROUTE_MBED */


#endif /* CA_HASH_MBED_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

