/**
  ******************************************************************************
  * @file    se_crypto_common.c
  * @author  MCD Application Team
  * @brief   Secure Engine CRYPTO module.
  *          This file provides set of firmware functions to manage SE Crypto
  *          functionalities.
  *          These functions are some common code for the functions used by the
  *          bootloader and the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */
#include "se_crypto_common.h"
#include "crypto.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @addtogroup  SE_CRYPTO SE Crypto
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON SE Common Crypto elements
  * @brief Common Crypto items for the functions used by the bootloader and the application.
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON_Exported_Variables Exported Variables
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON_Exported_Variables_AES_GCM AES GCM Exported Variables
  * @brief Private variables used for AES GCM symmetric crypto
  * @{
  */
AESGCMctx_stt m_xSE_AESGCMCtx; /*!< Variable used to store the AES128 context */

/**
  * @}
  */

/**
  * @}
  */

/** @defgroup SE_CRYPTO_COMMON_Exported_Functions Exported Functions
  * @brief Common crypto functions (helpers) to be used by the bootloader and the user application services if needed.
  *   This aims at factorizing the crypto code (core code with crypto-dependent APIs).
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON_AES_GCM_Exported_Functions AES GCM Exported Functions
  * @brief Common crypto functions (helpers) for AES GCM symmetric crypto.
  * @{
  */

/**
  * @brief Secure Engine AES GCM Encrypt Init function.
  *        It is a wrapper of AES_GCM_Encrypt_Init function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pKey: pointer to the key.
  * @param pxSE_GCMInit: Secure Engine AES GCM Init structure.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Encrypt_Init(const uint8_t *pKey, SE_GCMInitTypeDef *pxSE_GCMInit)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if (pxSE_GCMInit == NULL)
  {
    return SE_ERROR;
  }

  /* Set flag field to default value */
  m_xSE_AESGCMCtx.mFlags = E_SK_DEFAULT;

  /* Set key size to 16 (corresponding to AES-128) */
  m_xSE_AESGCMCtx.mKeySize = (int32_t) SE_SYMKEY_LEN;

  /* Set nonce size field to iv_length, note that valid values are 7,8,9,10,11,12,13 */
  m_xSE_AESGCMCtx.mIvSize = pxSE_GCMInit->NonceSize;

  /* Size of returned authentication TAG */
  m_xSE_AESGCMCtx.mTagSize = pxSE_GCMInit->TagSize;

  /* Set the size of the header */
  m_xSE_AESGCMCtx.mAADsize = pxSE_GCMInit->HeaderSize;

  /* Set the size of thepayload */
  m_xSE_AESGCMCtx.mPayloadSize = pxSE_GCMInit->PayloadSize;

  /* Crypto function call */
  cryptolib_status = AES_GCM_Encrypt_Init(&m_xSE_AESGCMCtx, pKey, pxSE_GCMInit->pNonce);

  /* Return status */
  if (cryptolib_status == AES_SUCCESS)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}


/**
  * @brief Secure Engine AES GCM Header Append function.
  *        It is a wrapper of AES_GCM_Header_Append function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Header_Append(const uint8_t *pInputBuffer, int32_t InputSize)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if (pInputBuffer == NULL)
  {
    return SE_ERROR;
  }

  /* Crypto function call */

  cryptolib_status = AES_GCM_Header_Append(&m_xSE_AESGCMCtx, pInputBuffer, InputSize);

  /* Return status */
  if (cryptolib_status == AES_SUCCESS)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief Secure Engine AES GCM Encrypt Append function.
  *        It is a wrapper of AES_GCM_Encrypt_Append function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Encrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                                int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if ((pInputBuffer == NULL) || (pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    return SE_ERROR;
  }

  /* Crypto function call */
  cryptolib_status = AES_GCM_Encrypt_Append(&m_xSE_AESGCMCtx, pInputBuffer, InputSize, pOutputBuffer, pOutputSize);

  /* Return status */
  if (cryptolib_status == AES_SUCCESS)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief Secure Engine AES GCM Encrypt Finish function.
  *        It is a wrapper of AES_GCM_Encrypt_Finish function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Encrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if ((pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    return SE_ERROR;
  }

  /* Crypto function call */
  cryptolib_status = AES_GCM_Encrypt_Finish(&m_xSE_AESGCMCtx, pOutputBuffer, pOutputSize);

  /* Return status */
  if (cryptolib_status == AES_SUCCESS)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief Secure Engine AES GCM Decrypt Init function.
  *        It is a wrapper of AES_GCM_Decrypt_Init function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pKey: pointer to the key.
  * @param pxSE_GCMInit: Secure Engine AES GCM Init structure.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Decrypt_Init(const uint8_t *pKey, SE_GCMInitTypeDef *pxSE_GCMInit)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if (pxSE_GCMInit == NULL)
  {
    return SE_ERROR;
  }

  /* Set flag field to default value */
  m_xSE_AESGCMCtx.mFlags = E_SK_DEFAULT;

  /* Set key size to 16 (corresponding to AES-128) */
  m_xSE_AESGCMCtx.mKeySize = (int32_t) SE_SYMKEY_LEN;

  /* Set nonce size field to iv_length, note that valid values are 7,8,9,10,11,12,13 */
  m_xSE_AESGCMCtx.mIvSize = pxSE_GCMInit->NonceSize;

  /* Set the size of the header */
  m_xSE_AESGCMCtx.mAADsize = pxSE_GCMInit->HeaderSize;

  /* Set the size of thepayload */
  m_xSE_AESGCMCtx.mPayloadSize = pxSE_GCMInit->PayloadSize;

  /* Set the pointer to the TAG to be checked */
  m_xSE_AESGCMCtx.pmTag = pxSE_GCMInit->pTag;

  /* Size of returned authentication TAG */
  m_xSE_AESGCMCtx.mTagSize = pxSE_GCMInit->TagSize;

  /* Crypto function call */
  cryptolib_status = AES_GCM_Decrypt_Init(&m_xSE_AESGCMCtx, pKey, pxSE_GCMInit->pNonce);

  /* Return status */
  if (cryptolib_status == AES_SUCCESS)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief Secure Engine AES GCM Decrypt Append function.
  *        It is a wrapper of AES_GCM_Decrypt_Append function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Decrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                                int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if ((pInputBuffer == NULL) || (pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    return SE_ERROR;
  }

  /* Crypto function call */
  cryptolib_status = AES_GCM_Decrypt_Append(&m_xSE_AESGCMCtx, pInputBuffer, InputSize, pOutputBuffer, pOutputSize);

  /* Return status */
  if (cryptolib_status == AES_SUCCESS)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief Secure Engine AES GCM Decrypt Finish function.
  *        It is a wrapper of AES_GCM_Decrypt_Finish function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AES_GCM_Decrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  int32_t cryptolib_status;

  /* Check the pointers allocation */
  if ((pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    return SE_ERROR;
  }

  /* Crypto function call */
  cryptolib_status = AES_GCM_Decrypt_Finish(&m_xSE_AESGCMCtx, pOutputBuffer, pOutputSize);

  /* Return status */
  if (cryptolib_status == AUTHENTICATION_SUCCESSFUL)
  {
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
