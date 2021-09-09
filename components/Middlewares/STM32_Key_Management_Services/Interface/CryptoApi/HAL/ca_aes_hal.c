/**
  ******************************************************************************
  * @file    ca_aes_hal.c
  * @author  MCD Application Team
  * @brief   This file contains the AES router implementation of
  *          the Cryptographic API (CA) module to the HAL Cryptographic drivers.
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
#ifndef CA_AES_HAL_C
#define CA_AES_HAL_C

/* Includes ------------------------------------------------------------------*/
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define TIMEOUT_VALUE           0xFF
#define CCM_AAD_LENGTH          64
/* CCM flags */
#define CCM_ENCRYPTION_ONGOING  0U
#define CCM_DECRYPTION_ONGOING  1U
/* GCM flags */
#define GCM_ENCRYPTION_ONGOING  (1U << 0)
#define GCM_DECRYPTION_ONGOING  (1U << 1)
#define GCM_INIT_NOT_DONE       (1U << 3)

/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void cleanup_handle(CRYP_HandleTypeDef *CrypHandle);

/* Private function definitions -----------------------------------------------*/
static void cleanup_handle(CRYP_HandleTypeDef *CrypHandle)
{
  (void)memset(CrypHandle, 0, sizeof(CRYP_HandleTypeDef));
}

#if defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#include "mac_stm32hal.c"
#endif /* (CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL */

/* Functions Definition ------------------------------------------------------*/
#if defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#if (CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES Encryption in CBC Mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Buffer with the IV
  * @note       1. P_pAESCBCctx.mKeySize (see AESCBCctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESCBCctx.mIvSize must be set with the size of the IV
  *             (default CRL_AES_BLOCK) prior to calling this function.
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

  if ((P_pAESCBCctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESCBCctx->mKeySize == 0)
      || (P_pAESCBCctx->mIvSize == 0))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESCBCctx->CrypHandle));
  P_pAESCBCctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&P_pAESCBCctx->CrypHandle) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESCBCctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  if (P_pAESCBCctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESCBCctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESCBCctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESCBCctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init IV and key here because of endianness */
  for (uint8_t i = 0; i < 4U; i++)
  {
    P_pAESCBCctx->Iv_endian[4U * i]        = P_pIv[3U + (4U * i)];
    P_pAESCBCctx->Iv_endian[1U + (4U * i)] = P_pIv[2U + (4U * i)];
    P_pAESCBCctx->Iv_endian[2U + (4U * i)] = P_pIv[1U + (4U * i)];
    P_pAESCBCctx->Iv_endian[3U + (4U * i)] = P_pIv[4U * i];
  }

  for (uint8_t i = 0; i < ((uint32_t)(P_pAESCBCctx->mKeySize) / 4U); i++)
  {
    P_pAESCBCctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESCBCctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESCBCctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESCBCctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }

  P_pAESCBCctx->CrypHandle.Init.Algorithm       = CRYP_AES_CBC;
  P_pAESCBCctx->CrypHandle.Init.pKey = (uint32_t *)(uint32_t)(P_pAESCBCctx->Key_endian);
  P_pAESCBCctx->CrypHandle.Init.pInitVect = (uint32_t *)(uint32_t)(P_pAESCBCctx->Iv_endian);

  P_pAESCBCctx->CrypHandle.Init.Header = NULL;
  P_pAESCBCctx->CrypHandle.Init.HeaderSize = 0;
  P_pAESCBCctx->CrypHandle.Init.B0 = NULL;
  P_pAESCBCctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESCBCctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  if (HAL_CRYP_Init(&P_pAESCBCctx->CrypHandle) != HAL_OK)
  {
    /* Initialization Error */
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
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
  * @retval     CA_AES_ERR_BAD_INPUT_SIZE:Size of input is less than CRL_AES_BLOCK
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed.
  */
int32_t CA_AES_CBC_Encrypt_Append(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t P_inputSize,
                                  uint8_t *P_pOutputBuffer,
                                  int32_t *P_pOutputSize)
{

  int32_t aes_ret_status = CA_AES_SUCCESS;

  if ((P_pAESCBCctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_inputSize < (int32_t)CA_CRL_AES_BLOCK)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  if (HAL_CRYP_Encrypt(&P_pAESCBCctx->CrypHandle, (uint32_t *)(uint32_t)P_pInputBuffer, (uint16_t)P_inputSize,
                       (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  else
  {
    *P_pOutputSize = P_inputSize;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Finalization of CBC mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC, already initialized, context. Not used.
  * @param[out] *P_pOutputBuffer: Output buffer. Not used.
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, in bytes. Not used.
  * @note       This function DeInit AES handle.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  */
int32_t CA_AES_CBC_Encrypt_Finish(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;

  (void)P_pOutputBuffer;
  (void)P_pOutputSize;

  if (HAL_CRYP_DeInit(&P_pAESCBCctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  cleanup_handle(&(P_pAESCBCctx->CrypHandle));

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
  * @note       1. P_pAESCBCctx.mKeySize (see AESCBCctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESCBCctx.mIvSize must be set with the size of the IV
  *             (default CRL_AES_BLOCK) prior to calling this function.
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

  if ((P_pAESCBCctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESCBCctx->mKeySize == 0)
      || (P_pAESCBCctx->mIvSize == 0))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESCBCctx->CrypHandle));
  P_pAESCBCctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&P_pAESCBCctx->CrypHandle) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESCBCctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  if (P_pAESCBCctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESCBCctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESCBCctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESCBCctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init IV and key here because of endianness */
  for (uint8_t i = 0; i < 4U; i++)
  {
    P_pAESCBCctx->Iv_endian[4U * i]        = P_pIv[3U + (4U * i)];
    P_pAESCBCctx->Iv_endian[1U + (4U * i)] = P_pIv[2U + (4U * i)];
    P_pAESCBCctx->Iv_endian[2U + (4U * i)] = P_pIv[1U + (4U * i)];
    P_pAESCBCctx->Iv_endian[3U + (4U * i)] = P_pIv[4U * i];
  }

  for (uint8_t i = 0; i < ((uint8_t)(P_pAESCBCctx->mKeySize) / 4U); i++)
  {
    P_pAESCBCctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESCBCctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESCBCctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESCBCctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }

  P_pAESCBCctx->CrypHandle.Init.Algorithm       = CRYP_AES_CBC;
  P_pAESCBCctx->CrypHandle.Init.pKey = (uint32_t *)(uint32_t)(P_pAESCBCctx->Key_endian);
  P_pAESCBCctx->CrypHandle.Init.pInitVect = (uint32_t *)(uint32_t)(P_pAESCBCctx->Iv_endian);

  P_pAESCBCctx->CrypHandle.Init.Header = NULL;
  P_pAESCBCctx->CrypHandle.Init.HeaderSize = 0;
  P_pAESCBCctx->CrypHandle.Init.B0 = NULL;
  P_pAESCBCctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESCBCctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  if (HAL_CRYP_Init(&P_pAESCBCctx->CrypHandle) != HAL_OK)
  {
    /* Initialization Error */
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
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

  if ((P_pAESCBCctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_inputSize < (int32_t)CA_CRL_AES_BLOCK)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }


  if (HAL_CRYP_Decrypt(&P_pAESCBCctx->CrypHandle, (uint32_t *)(uint32_t)P_pInputBuffer, (uint16_t)P_inputSize,
                       (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  else
  {
    *P_pOutputSize = P_inputSize;
  }

  return aes_ret_status;
}

/**
  * @brief      AES Decryption Finalization of CBC mode
  * @param[in,out]
  *             *P_pAESCBCctx: AES CBC, already initialized, context. Not used.
  * @param[out] *P_pOutputBuffer: Output buffer. Not used.
  * @param[out] *P_pOutputSize: Pointer to integer containing size of written
  *             output data, in bytes. Not used.
  * @note       This function will DeInit AES handle.
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  */
int32_t CA_AES_CBC_Decrypt_Finish(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;

  (void)P_pOutputBuffer;
  (void)P_pOutputSize;
  if (HAL_CRYP_DeInit(&P_pAESCBCctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  cleanup_handle(&(P_pAESCBCctx->CrypHandle));

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_HAL */

/* ------------------------------------------------------------------------------------------------------------------ */

#if defined(CA_ROUTE_AES_CCM) && ((CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#if (CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES CCM encryption
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pNonce: Buffer with the Nonce
  * @note       1. P_pAESCCMctx->mKeySize (see \ref CA_AESCCMctx_stt) must be set
  *             with the size of the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESCCMctx->mNonceSize must be set with the size
  *             of the CCM Nonce. Possible values are {7,8,9,10,11,12,13}.
  *             3. P_pAESCCMctx->mAssDataSize must be set with the size of
  *             the Associated Data (i.e. Header or any data that will be
  *             authenticated but not encrypted).
  *             4. P_pAESCCMctx->mPayloadSize must be set with the size of
  *             the Payload (i.e. Data that will be authenticated and encrypted).
  * @retval     CA_AES_ERR_BAD_PARAMETER: Operation NOT Successful
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values
  */
int32_t CA_AES_CCM_Encrypt_Init(CA_AESCCMctx_stt *P_pAESCCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pNonce)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  uint8_t temp;                         /* for B0 swappping */
  uint8_t i;

  if ((P_pAESCCMctx == NULL) || (P_pKey == NULL) || (P_pNonce == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (P_pAESCCMctx->mKeySize <= 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if ((P_pAESCCMctx->mNonceSize < 7)  && (P_pAESCCMctx->mNonceSize > 13))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCCMctx->mTagSize <= 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCCMctx->mAssDataSize < 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCCMctx->mPayloadSize  < 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESCCMctx->CrypHandle));
  P_pAESCCMctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&(P_pAESCCMctx->CrypHandle)) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESCCMctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  if (P_pAESCCMctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESCCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESCCMctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESCCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init key here because of endianness */
  for (i = 0; i < ((uint8_t)(P_pAESCCMctx->mKeySize) / 4U); i++)
  {
    P_pAESCCMctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESCCMctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESCCMctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESCCMctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }

  /* Copy the nonce to the static array*/
  (void)memset(P_pAESCCMctx->Iv_endian, 0, 16);
  (void)memcpy(P_pAESCCMctx->Iv_endian, P_pNonce, (uint32_t)(P_pAESCCMctx->mNonceSize));

  P_pAESCCMctx->CrypHandle.Init.Algorithm       = CRYP_AES_CCM;
  P_pAESCCMctx->CrypHandle.Init.pKey            = (uint32_t *)(uint32_t)(P_pAESCCMctx->Key_endian);
  P_pAESCCMctx->CrypHandle.Init.pInitVect       = (uint32_t *)NULL;/*P_pAESCCMctx->Iv_endian;*/

  P_pAESCCMctx->CrypHandle.Init.Header = NULL;
  P_pAESCCMctx->CrypHandle.Init.HeaderSize = (uint32_t)(P_pAESCCMctx->mAssDataSize);
  P_pAESCCMctx->CrypHandle.Init.B0 = (uint32_t *)(uint32_t)(P_pAESCCMctx->B0);
  P_pAESCCMctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESCCMctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  /* Calculate B0 information                                   */
  /* Formatting of the Flags Octet in B0 */
  P_pAESCCMctx->B0[0] = (P_pAESCCMctx->mAssDataSize != 0) ? (1U << 6) : 0U; /* Bit 6: Adata */
  P_pAESCCMctx->B0[0] |= ((((uint8_t)(P_pAESCCMctx->mTagSize) - 2U) / 2U) & 0x7U) << 3; /* Bit 5..3: [(t-2)/2]3 */
  P_pAESCCMctx->B0[0] |= ((15U - (uint8_t)(P_pAESCCMctx->mNonceSize) - 1U)) & 0x7U; /* Bit 2..0: [q-1]3 */
  (void)memcpy(&(P_pAESCCMctx->B0[1]), P_pNonce, (uint32_t)(P_pAESCCMctx->mNonceSize)); /* N */
  /* Q */
  for (i = 1U + (uint8_t)(P_pAESCCMctx->mNonceSize); i <= (15U - 4U); i++)
  {
    P_pAESCCMctx->B0[i] = 0;
  }
  if (i <= 12U)
  {
    P_pAESCCMctx->B0[12] = (uint8_t)(((uint32_t)(P_pAESCCMctx->mPayloadSize) >> 24U) & 0xFFU);
  }
  if (i <= 13U)
  {
    P_pAESCCMctx->B0[13] = (uint8_t)(((uint32_t)(P_pAESCCMctx->mPayloadSize) >> 16U) & 0xFFU);
  }
  if (i <= 14U)
  {
    P_pAESCCMctx->B0[14] = (uint8_t)(((uint32_t)(P_pAESCCMctx->mPayloadSize) >> 8U) & 0xFFU);
  }
  if (i <= 15U)
  {
    P_pAESCCMctx->B0[15] = (uint8_t)((uint32_t)(P_pAESCCMctx->mPayloadSize) & 0xFFU);
  }

  /* endianness      */
  for (i = 0; i < 4U; i++)
  {
    temp = P_pAESCCMctx->B0[(4U * i)];
    P_pAESCCMctx->B0[(4U * i)] = P_pAESCCMctx->B0[3U + (4U * i)];
    P_pAESCCMctx->B0[3U + (4U * i)] = temp;
    temp = P_pAESCCMctx->B0[1U + (4U * i)];
    P_pAESCCMctx->B0[1U + (4U * i)] = P_pAESCCMctx->B0[2U + (4U * i)];
    P_pAESCCMctx->B0[2U + (4U * i)] = temp;
  }

  if (HAL_CRYP_Init(&(P_pAESCCMctx->CrypHandle)) != HAL_OK)
  {
    /* Initialization Error */
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }

  P_pAESCCMctx->flags = CCM_ENCRYPTION_ONGOING;

  return aes_ret_status;
}

/**
  * @brief      AES CCM Encryption function
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Size of written output data, expressed in bytes
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a not valid
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed
  */
int32_t CA_AES_CCM_Encrypt_Append(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;

  if ((P_pAESCCMctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (P_inputSize < 0)
  {
    return CA_AES_ERR_BAD_OPERATION;
  }

  if (HAL_CRYP_Encrypt(&(P_pAESCCMctx->CrypHandle), (uint32_t *)(uint32_t)P_pInputBuffer,
                       (uint16_t)P_inputSize, (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  *P_pOutputSize = P_inputSize;

  return aes_ret_status;
}

/**
  * @brief      AES CCM Finalization during encryption,
  *             this will create the Authentication TAG
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[out] *P_pOutputBuffer: Output Authentication TAG
  * @param[out] *P_pOutputSize: Size of returned TAG
  * @retval     CA_AES_SUCCESS: Operation Successful
  */
int32_t CA_AES_CCM_Encrypt_Finish(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  uint8_t tag[16] __attribute__((aligned(4)));

  if (HAL_CRYPEx_AESCCM_GenerateAuthTAG(&(P_pAESCCMctx->CrypHandle),
                                        (uint32_t *)(uint32_t)tag,
                                        TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  else
  {
    (void)memcpy(P_pOutputBuffer, tag, (uint32_t)(P_pAESCCMctx->mTagSize));
  }

  if (HAL_CRYP_DeInit(&(P_pAESCCMctx->CrypHandle)) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  cleanup_handle(&(P_pAESCCMctx->CrypHandle));

  *P_pOutputSize = P_pAESCCMctx->mTagSize;

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
/**
  * @brief       Initialization for AES CCM Decryption
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pNonce: Buffer with the Nonce
  * @note       1. P_pAESCCMctx->mKeySize (see \ref CA_AESCCMctx_stt) must be set
  *             with the size of the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESCCMctx->mNonceSize must be set with the size
  *             of the CCM Nonce. Possible values are {7,8,9,10,11,12,13}.
  *             3. P_pAESCCMctx->mTagSize must be set with the size of
  *             authentication TAG that will be generated by
  *             the CA_AES_CCM_Encrypt_Finish.
  *             Possible values are values are {4,6,8,10,12,14,16}.
  *             4. P_pAESCCMctx->mAssDataSize must be set with the size of
  *             the Associated Data (i.e. Header or any data that will be
  *             authenticated but not encrypted).
  *             5. P_pAESCCMctx->mPayloadSize must be set with the size of
  *             the Payload (i.e. Data that will be authenticated and encrypted).
  *             6. CCM standard expects the authentication TAG to be passed
  *             as part of the ciphertext. In this implementations the tag
  *             should be not be passed to CA_AES_CCM_Decrypt_Append.
  *             Instead a pointer to the TAG must be set in P_pAESCCMctx.pmTag
  *             and this will be checked by CA_AES_CCM_Decrypt_Finish
  * @retval     CA_AES_ERR_BAD_PARAMETER: Operation NOT Successful
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values
  */
int32_t CA_AES_CCM_Decrypt_Init(CA_AESCCMctx_stt *P_pAESCCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pNonce)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  uint8_t temp;                         /* for B0 swappping */
  uint8_t i;

  if ((P_pAESCCMctx == NULL) || (P_pKey == NULL) || (P_pNonce == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (P_pAESCCMctx->mKeySize <= 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if ((P_pAESCCMctx->mNonceSize < 7)  && (P_pAESCCMctx->mNonceSize > 13))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCCMctx->mTagSize <= 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCCMctx->mAssDataSize < 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCCMctx->mPayloadSize  < 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESCCMctx->CrypHandle));
  P_pAESCCMctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&(P_pAESCCMctx->CrypHandle)) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESCCMctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  if (P_pAESCCMctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESCCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESCCMctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESCCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init key here because of endianness */
  for (i = 0; i < ((uint8_t)(P_pAESCCMctx->mKeySize) / 4U); i++)
  {
    P_pAESCCMctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESCCMctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESCCMctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESCCMctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }

  /* Copy the nonce to the static array*/
  (void)memset(P_pAESCCMctx->Iv_endian, 0, 16U);
  (void)memcpy(P_pAESCCMctx->Iv_endian, P_pNonce, (uint32_t)(P_pAESCCMctx->mNonceSize));

  P_pAESCCMctx->CrypHandle.Init.Algorithm = CRYP_AES_CCM;
  P_pAESCCMctx->CrypHandle.Init.pKey      = (uint32_t *)(uint32_t)(P_pAESCCMctx->Key_endian);
  P_pAESCCMctx->CrypHandle.Init.pInitVect = (uint32_t *)NULL;/*P_pAESCCMctx->Iv_endian;*/


  P_pAESCCMctx->CrypHandle.Init.Header = NULL;
  P_pAESCCMctx->CrypHandle.Init.HeaderSize = 0;/*P_pAESCCMctx->mAssDataSize;*/
  P_pAESCCMctx->CrypHandle.Init.B0 = (uint32_t *)(uint32_t)(P_pAESCCMctx->B0);
  P_pAESCCMctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESCCMctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  /* Calculate B0 information                                    */
  /* Formatting of the Flags Octet in B0 */
  P_pAESCCMctx->B0[0] = (P_pAESCCMctx->mAssDataSize != 0) ? (1U << 6) : 0U; /* Bit 6: Adata */
  P_pAESCCMctx->B0[0] |= ((((uint8_t)(P_pAESCCMctx->mTagSize) - 2U) / 2U) & 0x7U) << 3; /* Bit 5..3: [(t-2)/2]3 */
  P_pAESCCMctx->B0[0] |= ((15u - (uint8_t)(P_pAESCCMctx->mNonceSize) - 1U)) & 0x7U; /* Bit 2..0: [q-1]3 */
  (void)memcpy(&(P_pAESCCMctx->B0[1]), P_pNonce, (uint32_t)(P_pAESCCMctx->mNonceSize)); /* N */
  /* Q */
  for (i = 1U + (uint8_t)(P_pAESCCMctx->mNonceSize); i <= (15U - 4U); i++)
  {
    P_pAESCCMctx->B0[i] = 0;
  }
  if (i <= 12U)
  {
    P_pAESCCMctx->B0[12] = (uint8_t)(((uint32_t)(P_pAESCCMctx->mPayloadSize) >> 24U) & 0xFFU);
  }
  if (i <= 13U)
  {
    P_pAESCCMctx->B0[13] = (uint8_t)(((uint32_t)(P_pAESCCMctx->mPayloadSize) >> 16U) & 0xFFU);
  }
  if (i <= 14U)
  {
    P_pAESCCMctx->B0[14] = (uint8_t)(((uint32_t)(P_pAESCCMctx->mPayloadSize) >> 8U) & 0xFFU);
  }
  if (i <= 15U)
  {
    P_pAESCCMctx->B0[15] = (uint8_t)((uint32_t)(P_pAESCCMctx->mPayloadSize) & 0xFFU);
  }

  /* endianness      */
  for (i = 0; i < 4U; i++)
  {
    temp = P_pAESCCMctx->B0[4U * i];
    P_pAESCCMctx->B0[4U * i] = P_pAESCCMctx->B0[3U + (4U * i)];
    P_pAESCCMctx->B0[3U + (4U * i)] = temp;
    temp = P_pAESCCMctx->B0[1U + (4U * i)];
    P_pAESCCMctx->B0[1U + (4U * i)] = P_pAESCCMctx->B0[2U + (4U * i)];
    P_pAESCCMctx->B0[2U + (4U * i)] = temp;
  }

  if (HAL_CRYP_Init(&(P_pAESCCMctx->CrypHandle)) != HAL_OK)
  {
    /* Initialization Error */
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }

  P_pAESCCMctx->flags = CCM_DECRYPTION_ONGOING;

  return aes_ret_status;
}

/**
  * @brief      AES CCM Decryption function
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @param[out] *P_pOutputBuffer: Output buffer
  * @param[out] *P_pOutputSize: Size of written output data, expressed in bytes
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a not valid
  * @retval     CA_AES_ERR_BAD_OPERATION: Append not allowed
  */
int32_t CA_AES_CCM_Decrypt_Append(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;


  if ((P_pAESCCMctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (P_inputSize < 0)
  {
    return CA_AES_ERR_BAD_OPERATION;
  }

  if (HAL_CRYP_Decrypt(&(P_pAESCCMctx->CrypHandle), (uint32_t *)(uint32_t)P_pInputBuffer,
                       (uint16_t)P_inputSize, (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  *P_pOutputSize = P_inputSize;

  return aes_ret_status;
}

/**
  * @brief      AES CCM Finalization during decryption,
  *             the authentication TAG will be checked
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[in]  *P_pOutputBuffer: Not Used
  * @param[in]  *P_pOutputSize: No Used
  * @note       This function requires:
  *             - P_pAESCCMctx->pmTag to be set to a valid pointer
  *             to the tag to be checked
  *             - P_pAESCCMctx->mTagSize to contain a valid value in
  *             the set {4,6,8,10,12,14,16}
  * @retval     CA_AES_ERR_BAD_PARAMETER: *P_pAESCCMctx is NULL
  * @retval     CA_AUTHENTICATION_SUCCESSFUL: the TAG is checked
  *             and match the compute one
  * @retval     CA_AUTHENTICATION_FAILED: the TAG does NOT match the compute one
  */
int32_t CA_AES_CCM_Decrypt_Finish(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AUTHENTICATION_FAILED;
  uint8_t tag[16] __attribute__((aligned(4)));

  (void)P_pOutputBuffer;
  (void)P_pOutputSize;

  if (HAL_CRYPEx_AESCCM_GenerateAuthTAG(&(P_pAESCCMctx->CrypHandle),
                                        (uint32_t *)(uint32_t)tag,
                                        TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }
  else if (memcmp(tag, P_pAESCCMctx->pmTag, (uint32_t)(P_pAESCCMctx->mTagSize)) == 0)
  {
    /* Check if tag is valid */
    aes_ret_status = CA_AUTHENTICATION_SUCCESSFUL;
  }
  else
  {
    /* Nothing to do */
  }

  if (HAL_CRYP_DeInit(&(P_pAESCCMctx->CrypHandle)) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }
  cleanup_handle(&(P_pAESCCMctx->CrypHandle));

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
/**
  * @brief      AES CCM Header processing function
  * @param[in, out]
  *             *P_pAESCCMctx: AES CCM context
  * @param[in]  *P_pInputBuffer: Input buffer
  * @param[in]  P_inputSize: Size of input data, expressed in bytes
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_SUCCESS: Operation Successful
  * @retval     CA_AES_ERR_BAD_OPERATION : Append not allowed
  * restriction : This function accepted only ADD with size < 14. Other case
  note manage. In theory ADD size can be up to 2^64.
  */
int32_t CA_AES_CCM_Header_Append(CA_AESCCMctx_stt *P_pAESCCMctx,
                                 const uint8_t *P_pInputBuffer,
                                 int32_t        P_inputSize)

{
  int32_t aes_ret_status = CA_AES_SUCCESS;
  CRYP_ConfigTypeDef config;
  uint8_t header[CCM_AAD_LENGTH] __attribute__((aligned(4)));

  if ((P_pAESCCMctx == NULL) || (P_pInputBuffer == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_inputSize != P_pAESCCMctx->mAssDataSize)
  {
    /* AAD size mismatch between init and header append call -> error */
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (HAL_CRYP_GetConfig(&(P_pAESCCMctx->CrypHandle), &config) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  else
  {
    config.HeaderSize = (uint32_t)P_inputSize;
    if ((uint32_t)P_inputSize < ((2UL << 16) - (2UL << 8)))
    {
      /* If 0 < a < 2^16-2^8, then a is encoded as [a]16, i.e., two octets */
      if ((P_inputSize + 2) > CCM_AAD_LENGTH)
      {
        return CA_AES_ERR_BAD_PARAMETER;
      }
      header[0] = (uint8_t)(((uint32_t)(P_inputSize) >> 8) & 0xFFU);
      header[1] = (uint8_t)((uint32_t)(P_inputSize) & 0xFFU);
      (void)memcpy(&(header[2]), P_pInputBuffer, (uint32_t)P_inputSize);
      config.HeaderSize += 2U;
      /* 4 bytes padding */
      if ((config.HeaderSize % 4U) != 0U)
      {
        (void)memset(&(header[config.HeaderSize]), 0, 4U - (config.HeaderSize % 4U));
        config.HeaderSize = config.HeaderSize + 4U - (config.HeaderSize % 4U);
      }
      /* 16 Bytes block padding done by HAL */
    }
    else /*if (P_inputSize < (2 << 32)) */
    {
      /* If 2^16-2^8 <= a < 2^32, then a is encoded as 0xff || 0xfe || [a]32, i.e., six octets */
      if ((P_inputSize + 6) > CCM_AAD_LENGTH)
      {
        return CA_AES_ERR_BAD_PARAMETER;
      }
      header[0] = 0xFFU;
      header[1] = 0xFEU;
      header[2] = (uint8_t)(((uint32_t)(P_inputSize) >> 24) & 0xFFU);
      header[3] = (uint8_t)(((uint32_t)(P_inputSize) >> 16) & 0xFFU);
      header[4] = (uint8_t)(((uint32_t)(P_inputSize) >> 8)  & 0xFFU);
      header[5] = (uint8_t)((uint32_t)(P_inputSize)         & 0xFFU);
      (void)memcpy(&(header[6]), P_pInputBuffer, (uint32_t)P_inputSize);
      config.HeaderSize += 6U;
      /* 4 bytes padding */
      if ((config.HeaderSize % 4U) != 0U)
      {
        (void)memset(&(header[config.HeaderSize]), 0, 4U - (config.HeaderSize % 4U));
        config.HeaderSize = config.HeaderSize + 4U - (config.HeaderSize % 4U);
      }
      /* 16 Bytes block padding done by HAL */
    }

    config.HeaderSize = config.HeaderSize / 4U;  /* Header interpreted as U32 */
    config.Header = (uint32_t *)(uint32_t)header;
    if (HAL_CRYP_SetConfig(&(P_pAESCCMctx->CrypHandle), &config) != HAL_OK)
    {
      aes_ret_status = CA_AES_ERR_BAD_OPERATION;
    }
    else
    {
      if (P_pAESCCMctx->flags == CCM_ENCRYPTION_ONGOING)
      {
        if (HAL_CRYP_Encrypt(&(P_pAESCCMctx->CrypHandle), (uint32_t *)NULL, 0, (uint32_t *)NULL,
                             TIMEOUT_VALUE) != HAL_OK)
        {
          aes_ret_status = CA_AES_ERR_BAD_OPERATION;
        }
      }
      else
      {
        if (HAL_CRYP_Decrypt(&(P_pAESCCMctx->CrypHandle), (uint32_t *)NULL, 0, (uint32_t *)NULL,
                             TIMEOUT_VALUE) != HAL_OK)
        {
          aes_ret_status = CA_AES_ERR_BAD_OPERATION;
        }
      }
    }
  }

  return aes_ret_status;
}
#endif /* (CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_HAL */

/* ------------------------------------------------------------------------------------------------------------------ */


#if defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL)
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
  if (P_pAESCMACctx == NULL)
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_pAESCMACctx->pmKey == NULL)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if ((P_pAESCMACctx->mKeySize != (int32_t)CA_CRL_AES128_KEY)
      && (P_pAESCMACctx->mKeySize != (int32_t)CA_CRL_AES256_KEY))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if ((P_pAESCMACctx->mTagSize <= 0) || (P_pAESCMACctx->mTagSize > (int32_t)CA_CRL_AES_BLOCK))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  return CA_AES_SUCCESS;
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
  mac_error_t error;
  if ((P_pAESCMACctx == NULL) || (P_pInputBuffer == NULL) || (P_inputSize == 0))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  error = CMAC_compute(P_pInputBuffer,
                       (uint32_t)P_inputSize,
                       P_pAESCMACctx->pmKey,
                       (uint32_t)(P_pAESCMACctx->mKeySize),
                       (uint32_t)(P_pAESCMACctx->mTagSize),
                       P_pAESCMACctx->mac);
  if (error == MAC_SUCCESS)
  {
    return CA_AES_SUCCESS;
  }
  else
  {
    return CA_AES_ERR_BAD_OPERATION;
  }
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
  if ((P_pAESCMACctx == NULL) || (P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  (void)memcpy(P_pOutputBuffer, P_pAESCMACctx->mac, (uint32_t)(P_pAESCMACctx->mTagSize));
  *P_pOutputSize = P_pAESCMACctx->mTagSize;
  return CA_AES_SUCCESS;
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
  if (P_pAESCMACctx == NULL)
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if (P_pAESCMACctx->pmKey == NULL)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if ((P_pAESCMACctx->mKeySize != (int32_t)CA_CRL_AES128_KEY)
      && (P_pAESCMACctx->mKeySize != (int32_t)CA_CRL_AES256_KEY))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if (P_pAESCMACctx->pmTag == NULL)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  if ((P_pAESCMACctx->mTagSize <= 0) || (P_pAESCMACctx->mTagSize > (int32_t)CA_CRL_AES_BLOCK))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }
  return CA_AES_SUCCESS;
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
  mac_error_t error;
  if ((P_pAESCMACctx == NULL) || (P_pInputBuffer == NULL) || (P_inputSize == 0))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  error = CMAC_compute(P_pInputBuffer,
                       (uint32_t)P_inputSize,
                       P_pAESCMACctx->pmKey,
                       (uint32_t)(P_pAESCMACctx->mKeySize),
                       (uint32_t)(P_pAESCMACctx->mTagSize),
                       P_pAESCMACctx->mac);
  if (error == MAC_SUCCESS)
  {
    return CA_AES_SUCCESS;
  }
  else
  {
    return CA_AES_ERR_BAD_OPERATION;
  }
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
  (void)P_pOutputBuffer;
  if ((P_pAESCMACctx == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  *P_pOutputSize = 0;
  if (memcmp(P_pAESCMACctx->pmTag, P_pAESCMACctx->mac, (uint32_t)(P_pAESCMACctx->mTagSize)) != 0)
  {
    return CA_AUTHENTICATION_FAILED;
  }
  else
  {
    return CA_AUTHENTICATION_SUCCESSFUL;
  }
}
#endif /* CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL */

/* ------------------------------------------------------------------------------------------------------------------ */
#if defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#if (CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES Encryption in ECB Mode
  * @param[in,out]
  *             *P_pAESECBctx: AES ECB context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Not used since no IV is required in ECB
  * @note       1. P_pAESECBctx.mKeySize (see AESECBctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
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
  int32_t aes_ret_status = CA_AES_SUCCESS;

  (void)P_pIv;

  if ((P_pAESECBctx == NULL) || (P_pKey == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (P_pAESECBctx->mKeySize == 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESECBctx->CrypHandle));
  P_pAESECBctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&P_pAESECBctx->CrypHandle) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESECBctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  P_pAESECBctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_WORD;
  if (P_pAESECBctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESECBctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESECBctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESECBctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init key here because of endianness */
  for (uint8_t i = 0; i < ((uint8_t)(P_pAESECBctx->mKeySize) / 4U); i++)
  {
    P_pAESECBctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESECBctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESECBctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESECBctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }

  P_pAESECBctx->CrypHandle.Init.Algorithm       = CRYP_AES_ECB;
  P_pAESECBctx->CrypHandle.Init.pKey            = (uint32_t *)(uint32_t)(P_pAESECBctx->Key_endian);

  P_pAESECBctx->CrypHandle.Init.Header = NULL;
  P_pAESECBctx->CrypHandle.Init.HeaderSize = 0;
  P_pAESECBctx->CrypHandle.Init.B0 = NULL;
  P_pAESECBctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESECBctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  if (HAL_CRYP_Init(&P_pAESECBctx->CrypHandle) != HAL_OK)
  {
    /* Initialization Error */
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
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

  if ((P_pAESECBctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_inputSize % (int32_t)CA_CRL_AES_BLOCK) != 0)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  if (HAL_CRYP_Encrypt(&P_pAESECBctx->CrypHandle, (uint32_t *)(uint32_t)P_pInputBuffer, (uint16_t)P_inputSize,
                       (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  else
  {
    *P_pOutputSize = P_inputSize;
  }

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
  int32_t aes_ret_status = CA_AES_SUCCESS;

  (void)P_pOutputBuffer;
  (void)P_pOutputSize;

  if (HAL_CRYP_DeInit(&P_pAESECBctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  cleanup_handle(&(P_pAESECBctx->CrypHandle));

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
/**
  * @brief      Initialization for AES Decryption in ECB Mode
  * @param[in,out]
  *             *P_pAESECBctx: AES ECB context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[in]  *P_pIv: Not used since no IV is required in ECB
  * @note       1. P_pAESECBctx.mKeySize (see AESECBctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Instead of the size of the key, you can also use the following
  *             predefined values:
  *             - CA_CRL_AES128_KEY
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
  int32_t aes_ret_status = CA_AES_SUCCESS;

  (void)P_pIv;

  if ((P_pAESECBctx == NULL) || (P_pKey == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if (P_pAESECBctx->mKeySize == 0)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESECBctx->CrypHandle));
  P_pAESECBctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&P_pAESECBctx->CrypHandle) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESECBctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  P_pAESECBctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_WORD;
  if (P_pAESECBctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESECBctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESECBctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESECBctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init key here because of endianness */
  for (uint8_t i = 0; i < ((uint8_t)(P_pAESECBctx->mKeySize) / 4U); i++)
  {
    P_pAESECBctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESECBctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESECBctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESECBctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }
  P_pAESECBctx->CrypHandle.Init.Algorithm       = CRYP_AES_ECB;
  P_pAESECBctx->CrypHandle.Init.pKey            = (uint32_t *)(uint32_t)(P_pAESECBctx->Key_endian);

  P_pAESECBctx->CrypHandle.Init.Header = NULL;
  P_pAESECBctx->CrypHandle.Init.HeaderSize = 0;
  P_pAESECBctx->CrypHandle.Init.B0 = NULL;
  P_pAESECBctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESECBctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  if (HAL_CRYP_Init(&P_pAESECBctx->CrypHandle) != HAL_OK)
  {
    /* Initialization Error */
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
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

  if ((P_pAESECBctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }
  if ((P_inputSize % (int32_t)CA_CRL_AES_BLOCK) != 0)
  {
    return CA_AES_ERR_BAD_INPUT_SIZE;
  }

  if (HAL_CRYP_Decrypt(&P_pAESECBctx->CrypHandle, (uint32_t *)(uint32_t)P_pInputBuffer, (uint16_t)P_inputSize,
                       (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  else
  {
    *P_pOutputSize = P_inputSize;
  }

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
  int32_t aes_ret_status = CA_AES_SUCCESS;

  (void)P_pOutputBuffer;
  (void)P_pOutputSize;

  if (HAL_CRYP_DeInit(&P_pAESECBctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_PARAMETER;
  }
  cleanup_handle(&(P_pAESECBctx->CrypHandle));

  return aes_ret_status;
}
#endif /* CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_HAL */

/* ------------------------------------------------------------------------------------------------------------------ */

#if defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)
#if (CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
/**
  * @brief      Initialization for AES GCM encryption
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM context
  * @param[in]  *P_pKey: Buffer with the Key
  * @param[out] *P_pIv: Buffer with the IV

  * @note       1. P_pAESGCMctx.mKeySize (see AESGCMctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESGCMctx.mIvSize must be set with the size of the IV
  *             prior to calling this function. IV is composed of IV (12 bytes)
  *             more counter (4 bytes).
  *             3. Following recommendation by NIST expressed in section 5.2.1.1
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

  if ((P_pAESGCMctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESGCMctx->mKeySize == 0) || (P_pAESGCMctx->mIvSize != 12))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESGCMctx->CrypHandle));
  P_pAESGCMctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&P_pAESGCMctx->CrypHandle) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESGCMctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  if (P_pAESGCMctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESGCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESGCMctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESGCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init IV and key here because of endianness */
  for (uint8_t i = 0; i < 3U; i++)
  {
    P_pAESGCMctx->Iv_endian[4U * i]        = P_pIv[3U + (4U * i)];
    P_pAESGCMctx->Iv_endian[1U + (4U * i)] = P_pIv[2U + (4U * i)];
    P_pAESGCMctx->Iv_endian[2U + (4U * i)] = P_pIv[1U + (4U * i)];
    P_pAESGCMctx->Iv_endian[3U + (4U * i)] = P_pIv[4U * i];
  }
  P_pAESGCMctx->Iv_endian[12] = 2;
  P_pAESGCMctx->Iv_endian[13] = 0;
  P_pAESGCMctx->Iv_endian[14] = 0;
  P_pAESGCMctx->Iv_endian[15] = 0;

  for (uint8_t i = 0; i < ((uint8_t)(P_pAESGCMctx->mKeySize) / 4U); i++)
  {
    P_pAESGCMctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESGCMctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESGCMctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESGCMctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }


  P_pAESGCMctx->CrypHandle.Init.Algorithm       = CRYP_AES_GCM_GMAC;
  P_pAESGCMctx->CrypHandle.Init.pKey            = (uint32_t *)(uint32_t)(P_pAESGCMctx->Key_endian);
  P_pAESGCMctx->CrypHandle.Init.pInitVect       = (uint32_t *)(uint32_t)(P_pAESGCMctx->Iv_endian);

  P_pAESGCMctx->CrypHandle.Init.Header = NULL;
  P_pAESGCMctx->CrypHandle.Init.HeaderSize = 0;
  P_pAESGCMctx->CrypHandle.Init.B0 = NULL;
  P_pAESGCMctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESGCMctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  P_pAESGCMctx->flags = GCM_ENCRYPTION_ONGOING | GCM_INIT_NOT_DONE;
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
  int32_t aes_ret_status = CA_AES_SUCCESS;

  if ((P_pAESGCMctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESGCMctx->flags & GCM_INIT_NOT_DONE) == GCM_INIT_NOT_DONE)
  {
    if (HAL_CRYP_Init(&P_pAESGCMctx->CrypHandle) != HAL_OK)
    {
      *P_pOutputSize = 0;
      return CA_AES_ERR_BAD_OPERATION;
    }
    P_pAESGCMctx->flags &= ~ GCM_INIT_NOT_DONE;
  }
  if (HAL_CRYP_Encrypt(&P_pAESGCMctx->CrypHandle, (uint32_t *)(uint32_t)P_pInputBuffer, (uint16_t)P_inputSize,
                       (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  *P_pOutputSize = P_inputSize;
  return aes_ret_status;
}

/**
  * @brief      AES GCM Finalization during encryption, this will create the Authentication TAG
  * @param[in,out]
  *             *P_pAESGCMctx: AES GCM, already initialized, context
  * @param[out] *P_pOutputBuffer: Output Authentication TAG
  * @param[out] *P_pOutputSize: Size of returned TAG
  * @note       This function requires P_pAESGCMctx mTagSize to contain a valid
  *             value between 1 and 16
  * @retval     CA_AES_SUCCESS: Operation Successfuf
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_GCM_Encrypt_Finish(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AES_SUCCESS;


  if ((P_pAESGCMctx == NULL) || (P_pOutputBuffer == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESGCMctx->mTagSize < 0) || (P_pAESGCMctx->mTagSize > 16))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  if (HAL_CRYPEx_AESGCM_GenerateAuthTAG(&P_pAESGCMctx->CrypHandle,
                                        (uint32_t *)(uint32_t)P_pOutputBuffer, TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }

  if (HAL_CRYP_DeInit(&P_pAESGCMctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }
  cleanup_handle(&(P_pAESGCMctx->CrypHandle));

  *P_pOutputSize = P_pAESGCMctx->mTagSize;

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

  * @note       1. P_pAESGCMctx.mKeySize (see AESGCMctx_stt) must be set with
  *             the size of the key prior to calling this function.
  *             Otherwise the following predefined values can be used:
  *             - CA_CRL_AES128_KEY
  *             - CA_CRL_AES256_KEY
  *             2. P_pAESGCMctx.mIvSize must be set with the size of the IV
  *             prior to calling this function. IV is composed of IV (12 bytes)
  *             more counter (4 bytes).
  *             3. Following recommendation by NIST expressed in section 5.2.1.1
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
  int32_t aes_ret_status = CA_AES_SUCCESS;

  if ((P_pAESGCMctx == NULL) || (P_pKey == NULL) || (P_pIv == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESGCMctx->mKeySize == 0) || (P_pAESGCMctx->mIvSize != 12))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  cleanup_handle(&(P_pAESGCMctx->CrypHandle));
  P_pAESGCMctx->CrypHandle.Instance = CA_AES_INSTANCE;

  if (HAL_CRYP_DeInit(&P_pAESGCMctx->CrypHandle) != HAL_OK)
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Initialize the CRYP peripheral */
  P_pAESGCMctx->CrypHandle.Init.DataType      = CRYP_DATATYPE_8B;
  if (P_pAESGCMctx->mKeySize == (int32_t)CA_CRL_AES128_KEY)
  {
    P_pAESGCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_128B;
  }
  else if (P_pAESGCMctx->mKeySize == (int32_t)CA_CRL_AES256_KEY)
  {
    P_pAESGCMctx->CrypHandle.Init.KeySize     = CRYP_KEYSIZE_256B;
  }
  else
  {
    /* Not supported by HW accelerator*/
    return CA_AES_ERR_BAD_CONTEXT;
  }

  /* Init IV and key here because of endianness */
  for (uint8_t i = 0; i < 3U; i++)
  {
    P_pAESGCMctx->Iv_endian[4U * i]        = P_pIv[3U + (4U * i)];
    P_pAESGCMctx->Iv_endian[1U + (4U * i)] = P_pIv[2U + (4U * i)];
    P_pAESGCMctx->Iv_endian[2U + (4U * i)] = P_pIv[1U + (4U * i)];
    P_pAESGCMctx->Iv_endian[3U + (4U * i)] = P_pIv[4U * i];
  }
  P_pAESGCMctx->Iv_endian[12] = 2;
  P_pAESGCMctx->Iv_endian[13] = 0;
  P_pAESGCMctx->Iv_endian[14] = 0;
  P_pAESGCMctx->Iv_endian[15] = 0;

  for (uint8_t i = 0; i < ((uint8_t)(P_pAESGCMctx->mKeySize) / 4U); i++)
  {
    P_pAESGCMctx->Key_endian[4U * i]        = P_pKey[3U + (4U * i)];
    P_pAESGCMctx->Key_endian[1U + (4U * i)] = P_pKey[2U + (4U * i)];
    P_pAESGCMctx->Key_endian[2U + (4U * i)] = P_pKey[1U + (4U * i)];
    P_pAESGCMctx->Key_endian[3U + (4U * i)] = P_pKey[4U * i];
  }

  P_pAESGCMctx->CrypHandle.Init.Algorithm       = CRYP_AES_GCM_GMAC;
  P_pAESGCMctx->CrypHandle.Init.pKey            = (uint32_t *)(uint32_t)(P_pAESGCMctx->Key_endian);
  P_pAESGCMctx->CrypHandle.Init.pInitVect       = (uint32_t *)(uint32_t)(P_pAESGCMctx->Iv_endian);

  P_pAESGCMctx->CrypHandle.Init.Header = NULL;
  P_pAESGCMctx->CrypHandle.Init.HeaderSize = 0;
  P_pAESGCMctx->CrypHandle.Init.B0 = NULL;
  P_pAESGCMctx->CrypHandle.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
  P_pAESGCMctx->CrypHandle.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ONCE;

  P_pAESGCMctx->flags = GCM_DECRYPTION_ONGOING | GCM_INIT_NOT_DONE;
  return aes_ret_status;
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

  if ((P_pAESGCMctx == NULL)
      || (P_pInputBuffer == NULL)
      || (P_pOutputBuffer == NULL)
      || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESGCMctx->flags & GCM_INIT_NOT_DONE) == GCM_INIT_NOT_DONE)
  {
    if (HAL_CRYP_Init(&P_pAESGCMctx->CrypHandle) != HAL_OK)
    {
      *P_pOutputSize = 0;
      return CA_AES_ERR_BAD_OPERATION;
    }
    P_pAESGCMctx->flags &= ~ GCM_INIT_NOT_DONE;
  }
  if (HAL_CRYP_Decrypt(&P_pAESGCMctx->CrypHandle, (uint32_t *)(uint32_t)P_pInputBuffer, (uint16_t)P_inputSize,
                       (uint32_t *)(uint32_t)P_pOutputBuffer,
                       TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }

  *P_pOutputSize = P_inputSize;

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
  * @retval     CA_AES_SUCCESS: Operation Successfu
  * @retval     CA_AES_ERR_BAD_PARAMETER: At least one parameter is a NULL pointer
  * @retval     CA_AES_ERR_BAD_CONTEXT: Context not initialized with valid values.
  *                                  See note
  */
int32_t CA_AES_GCM_Decrypt_Finish(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize)
{
  int32_t aes_ret_status = CA_AUTHENTICATION_SUCCESSFUL;
  uint8_t tag[16] = {0};

  (void)P_pOutputBuffer;

  if ((P_pAESGCMctx == NULL) || (P_pOutputSize == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  if ((P_pAESGCMctx->mTagSize < 0) || (P_pAESGCMctx->mTagSize > 16))
  {
    return CA_AES_ERR_BAD_CONTEXT;
  }

  if (HAL_CRYPEx_AESGCM_GenerateAuthTAG(&P_pAESGCMctx->CrypHandle, (uint32_t *)(uint32_t)&tag[0],
                                        TIMEOUT_VALUE) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }

  /* Check if tag is valid                                               */
  if (memcmp(tag, P_pAESGCMctx->pmTag, 16) != 0)
  {
    aes_ret_status =  CA_AUTHENTICATION_FAILED;
  }

  if (HAL_CRYP_DeInit(&P_pAESGCMctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_CONTEXT;
  }
  cleanup_handle(&(P_pAESGCMctx->CrypHandle));

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
  int32_t aes_ret_status = CA_AES_SUCCESS;


  if ((P_pAESGCMctx == NULL) || (P_pInputBuffer == NULL))
  {
    return CA_AES_ERR_BAD_PARAMETER;
  }

  P_pAESGCMctx->mAADsize = P_inputSize;
  P_pAESGCMctx->CrypHandle.Init.HeaderSize = (uint32_t)(P_inputSize) / 4U;
  P_pAESGCMctx->CrypHandle.Init.Header     = (uint32_t *)(uint32_t)P_pInputBuffer;

  if (HAL_CRYP_Init(&P_pAESGCMctx->CrypHandle) != HAL_OK)
  {
    aes_ret_status = CA_AES_ERR_BAD_OPERATION;
  }
  else
  {
    P_pAESGCMctx->flags &= ~ GCM_INIT_NOT_DONE;
    if ((P_pAESGCMctx->flags & GCM_ENCRYPTION_ONGOING) == GCM_ENCRYPTION_ONGOING)
    {
      if (HAL_CRYP_Encrypt(&(P_pAESGCMctx->CrypHandle), (uint32_t *)NULL, 0, (uint32_t *)NULL,
                           TIMEOUT_VALUE) != HAL_OK)
      {
        aes_ret_status = CA_AES_ERR_BAD_OPERATION;
      }
    }
    else
    {
      if (HAL_CRYP_Decrypt(&(P_pAESGCMctx->CrypHandle), (uint32_t *)NULL, 0, (uint32_t *)NULL,
                           TIMEOUT_VALUE) != HAL_OK)
      {
        aes_ret_status = CA_AES_ERR_BAD_OPERATION;
      }
    }
  }
  return aes_ret_status;
}
#endif /* (CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_HAL */

#endif /* CA_AES_HAL_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

