/**
  ******************************************************************************
  * @file    se_crypto_common.h
  * @author  MCD Application Team
  *          This file provides set of firmware functions to manage SE Crypto
  *          functionalities.
  *          These functions are some common code for the functions used by the bootloader and the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SE_CRYPTO_COMMON_H
#define SE_CRYPTO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @addtogroup SE_CRYPTO SE Crypto
  * @{
  */

/** @addtogroup SE_CRYPTO_COMMON
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON_Struct SE crypto common structures
  *  @brief Structures used to work with the crypto-specific functions.
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON_Struct_AES_GCM SE crypto AES GCM structures
  * @brief Structures used to work with the AES GCM symmetric crypto functions.
  * @{
  */

/**
  * @brief  Secure Engine AES GCM structure definition
  */
typedef struct
{
  uint8_t   *pNonce;               /*!< Pointer Nonce buffer.*/
  int32_t   NonceSize;             /*!< Size of the Nonce in bytes.*/
  uint8_t   *pTag;                 /*!< Pointer to Authentication TAG. This value must be set in decryption, and this
                                        TAG will be verified */
  int32_t   TagSize;               /*!< Size of the Tag to return.*/
  int32_t   HeaderSize;            /*!< Header Size.*/
  int32_t   PayloadSize;           /*!< Payload Size.*/
} SE_GCMInitTypeDef;

/**
  * @}
  */

/**
  * @}
  */

/** @defgroup SE_CRYPTO_COMMON_Public_Macros Public Macros
  * @brief Common macros used by all files dealing with some crypto parameters
  * @{
  */

/** @defgroup SE_CRYPTO_COMMON_Public_Macros_AES_GCM AES GCM Public Macros
  * @brief Common macros used by all files dealing with some AES GCM crypto parameters
  * @{
  */
#define IS_SE_CRYPTO_AES_GCM_NONCE_SIZE(SIZE)   ((SIZE) == SE_NONCE_LEN)  /*!< Verify the size of the AES nonce */
#define IS_SE_CRYPTO_AES_GCM_TAG_SIZE(SIZE)     ((SIZE) == SE_TAG_LEN)    /*!< Verify the size of the AES tag */
/**
  * @}
  */

/**
  * @}
  */



/** @addtogroup SE_CRYPTO_COMMON_Functions SE Crypto common functions
  * @{
  */

/** @addtogroup SE_CRYPTO_COMMON_Exported_Functions
  * @{
  */

/** @addtogroup SE_CRYPTO_COMMON_AES_GCM_Exported_Functions
  * @{
  */


/*Low level functions*/
SE_ErrorStatus SE_CRYPTO_AES_GCM_Encrypt_Init(const uint8_t *pKey, SE_GCMInitTypeDef *pxSE_GCMInit);
SE_ErrorStatus SE_CRYPTO_AES_GCM_Header_Append(const uint8_t *pInputBuffer, int32_t InputSize);
SE_ErrorStatus SE_CRYPTO_AES_GCM_Encrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                                int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_AES_GCM_Encrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_AES_GCM_Decrypt_Init(const uint8_t *pKey, SE_GCMInitTypeDef *pxSE_GCMInit);
SE_ErrorStatus SE_CRYPTO_AES_GCM_Decrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                                int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_AES_GCM_Decrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize);

/*High level functions*/

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

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* SE_CRYPTO_COMMON_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

