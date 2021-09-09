/**
  ******************************************************************************
  * @file    ca_aes_mbed.h
  * @author  MCD Application Team
  * @brief   This file contains the AES router includes and definitions of
  *          the Cryptographic API (CA) module to the MBED Cryptographic library.
  * @note    This file shall never be included directly by application but
  *          through the main header ca_aes.h
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
#ifndef CA_AES_MBED_H
#define CA_AES_MBED_H

#if !defined(CA_AES_H)
#error "This file shall never be included directly by application but through the main header ca_aes.h"
#endif /* CA_AES_H */

/* Configuration defines -----------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "mbedtls/aes.h"

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CBC */
#if defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  uint32_t   mContextId;            /*!< Unique ID of this context. \b Not \b used in current implementation  */
  CA_SKflags_et mFlags;             /*!< 32 bit mFlags, used to perform keyschedule */
  const uint8_t *pmKey;             /*!< Pointer to original Key buffer */
  const uint8_t *pmIv;              /*!< Pointer to original Initialization Vector buffer */
  int32_t   mIvSize;                /*!< Size of the Initialization Vector in bytes */
  uint32_t   amIv[4];               /*!< Temporary result/IV */
  int32_t   mKeySize;               /*!< Key length in bytes */
  uint32_t   amExpKey[1];           /*!< Expanded AES key - Don't care with this router */
  /* Additional information to maintain context to communicate with Mbed */
  psa_cipher_operation_t cipher_op; /*!< Psa Cipher Operation*/
  psa_key_handle_t psa_key_handle;  /*!< Psa Key Handle*/
} CA_AESCBCctx_stt;

/* Exported functions -------------------------------------------------------*/
#if (CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
int32_t CA_AES_CBC_Encrypt_Init(CA_AESCBCctx_stt *P_pAESCBCctx, const uint8_t *P_pKey, const uint8_t *P_pIv);
int32_t CA_AES_CBC_Encrypt_Append(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t P_inputSize,
                                  uint8_t *P_pOutputBuffer,
                                  int32_t *P_pOutputSize);
int32_t CA_AES_CBC_Encrypt_Finish(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
int32_t CA_AES_CBC_Decrypt_Init(CA_AESCBCctx_stt *P_pAESCBCctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv);
int32_t CA_AES_CBC_Decrypt_Append(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
int32_t CA_AES_CBC_Decrypt_Finish(CA_AESCBCctx_stt *P_pAESCBCctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_CBC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CBC */


/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CMAC */
#if defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  uint32_t   mContextId;   /*!< Unique ID of this context. \b Not \b used in current implementation  */
  SKflags_et mFlags; /*!< 32 bit mFlags, used to perform keyschedule and future use */
  const uint8_t *pmKey; /*!< Pointer to original Key buffer */
  const uint8_t *pmIv; /*!< Pointer to original Initialization Vector buffer */
  int32_t   mIvSize; /*!< Size of the Initialization Vector in bytes */
  uint32_t   amIv[4]; /*!< Temporary result/IV */
  int32_t   mKeySize;   /*!< Key length in bytes */
  uint32_t   amExpKey[1]; /*!< Expanded AES key - Don't care with this router */
  const uint8_t *pmTag;   /*!< Pointer to Authentication TAG. This value must be set in decryption, and this TAG will be verified */
  int32_t mTagSize; /*!< Size of the Tag to return. This must be set by the caller prior to calling Init */
  /* Additional information to maintain context to communicate with Mbed */
  psa_mac_operation_t cmac_op;      /*!< Psa Mac Operation*/
  psa_key_handle_t psa_key_handle;  /*!< Psa Key Handle*/
} CA_AESCMACctx_stt;

/* Exported functions -------------------------------------------------------*/
#if (CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
int32_t CA_AES_CMAC_Encrypt_Init(CA_AESCMACctx_stt *P_pAESCMACctx);
int32_t CA_AES_CMAC_Encrypt_Append(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   const uint8_t *P_pInputBuffer,
                                   int32_t P_inputSize);
int32_t CA_AES_CMAC_Encrypt_Finish(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   uint8_t       *P_pOutputBuffer,
                                   int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
int32_t CA_AES_CMAC_Decrypt_Init(CA_AESCMACctx_stt *P_pAESCMACctx);
int32_t CA_AES_CMAC_Decrypt_Append(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   const uint8_t *P_pInputBuffer,
                                   int32_t        P_inputSize);
int32_t CA_AES_CMAC_Decrypt_Finish(CA_AESCMACctx_stt *P_pAESCMACctx,
                                   uint8_t       *P_pOutputBuffer,
                                   int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_CMAC & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CMAC */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES ECB */
#if defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  uint32_t   mContextId;            /*!< Unique ID of this context. \b Not \b used in current implementation  */
  CA_SKflags_et mFlags;             /*!< 32 bit mFlags, used to perform keyschedule */
  const uint8_t *pmKey;             /*!< Pointer to original Key buffer */
  const uint8_t *pmIv;              /*!< Pointer to original Initialization Vector buffer */
  int32_t   mIvSize;                /*!< Size of the Initialization Vector in bytes */
  uint32_t   amIv[4];               /*!< Temporary result/IV */
  int32_t   mKeySize;               /*!< Key length in bytes */
  uint32_t   amExpKey[1];           /*!< Expanded AES key - Don't care with this router */
  /* Additional information to maintain context to communicate with Mbed */
  mbedtls_aes_context  mbedtls_ctx; /*!< Context for the mbedtls layer*/
} CA_AESECBctx_stt;

/* Exported functions -------------------------------------------------------*/
#if (CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
int32_t CA_AES_ECB_Encrypt_Init(CA_AESECBctx_stt *P_pAESECBctx, const uint8_t *P_pKey, const uint8_t *P_pIv);
int32_t CA_AES_ECB_Encrypt_Append(CA_AESECBctx_stt *P_pAESECBctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t P_inputSize,
                                  uint8_t *P_pOutputBuffer,
                                  int32_t *P_pOutputSize);
int32_t CA_AES_ECB_Encrypt_Finish(CA_AESECBctx_stt *P_pAESECBctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
int32_t CA_AES_ECB_Decrypt_Init(CA_AESECBctx_stt *P_pAESECBctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv);
int32_t CA_AES_ECB_Decrypt_Append(CA_AESECBctx_stt *P_pAESECBctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
int32_t CA_AES_ECB_Decrypt_Finish(CA_AESECBctx_stt *P_pAESECBctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_ECB & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
#endif /* (CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES ECB */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES GCM */
#if defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  uint32_t   mContextId;            /*!< Unique ID of this AES-GCM Context. \b Not \b used in current implementation  */
  CA_SKflags_et mFlags;             /*!< 32 bit mFlags, used to perform keyschedule */
  const uint8_t *pmKey;             /*!< Pointer to original Key buffer */
  const uint8_t *pmIv;              /*!< Pointer to original Initialization Vector buffer */
  int32_t   mIvSize;                /*!< Size of the Initialization Vector in bytes. This must be set by the caller
                                         prior to calling Init */
  uint32_t   amIv[4];               /*!< This is the current IV value */
  int32_t   mKeySize;               /*!< AES Key length in bytes. This must be set by the caller prior to calling
                                         Init */
  const uint8_t *pmTag;             /*!< Pointer to Authentication TAG. This value must be set in decryption,
                                         and this TAG will be verified */
  int32_t mTagSize;                 /*!< Size of the Tag to return. This must be set by the caller prior
                                         to calling Init */
  int32_t mAADsize;                 /*!< Additional authenticated data size. For internal use  */
  /* Additional information to maintain context to communicate with Mbed */
  psa_key_handle_t psa_key_handle;  /*!< Psa Key Handle*/
  uint8_t wrap_size_cipher;         /*!< Size of the Cipher*/
  uint8_t wrap_is_use;              /*!< To know what operation we are doing*/
  mbedtls_gcm_context mbedtls_ctx;  /*!< Context for the mbedtls layer*/
} CA_AESGCMctx_stt;

/* Exported functions -------------------------------------------------------*/
#if (CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
int32_t CA_AES_GCM_Encrypt_Init(CA_AESGCMctx_stt *P_pAESGCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv);
int32_t CA_AES_GCM_Encrypt_Append(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
int32_t CA_AES_GCM_Encrypt_Finish(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
int32_t CA_AES_GCM_Decrypt_Init(CA_AESGCMctx_stt *P_pAESGCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pIv);
int32_t CA_AES_GCM_Decrypt_Append(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
int32_t CA_AES_GCM_Decrypt_Finish(CA_AESGCMctx_stt *P_pAESGCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_GCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
int32_t CA_AES_GCM_Header_Append(CA_AESGCMctx_stt *P_pAESGCMctx,
                                 const uint8_t *P_pInputBuffer,
                                 int32_t        P_inputSize);
#endif /* (CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES GCM */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_AES_MBED_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

