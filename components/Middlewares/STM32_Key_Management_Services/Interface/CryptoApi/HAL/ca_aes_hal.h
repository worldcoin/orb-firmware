/**
  ******************************************************************************
  * @file    ca_aes_hal.h
  * @author  MCD Application Team
  * @brief   This file contains the AES router includes and definitions of
  *          the Cryptographic API (CA) module to the HAL Cryptographic drivers.
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
#ifndef CA_AES_HAL_H
#define CA_AES_HAL_H

#if !defined(CA_AES_H)
#error "This file shall never be included directly by application but through the main header ca_aes.h"
#endif /* CA_AES_H */

/* Configuration defines -----------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CBC */
#if defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_HAL)
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
  /* Additional information to maintain context to communicate with HAL */
  CRYP_HandleTypeDef     CrypHandle;
  uint8_t Iv_endian[16];
  uint8_t Key_endian[CA_CRL_AES256_KEY];
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
#endif /* (CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_HAL */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CBC */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CCM */
#if defined(CA_ROUTE_AES_CCM) && ((CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)
/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/

/* Typedef ------------------------------------------------------------------*/
typedef struct
{
  /** Unique ID of this AES-GCM Context. \b Not \b used in current implementation  */
  uint32_t   mContextId;
  /** 32 bit mFlags, used to perform keyschedule */
  SKflags_et mFlags;
  /** Pointer to original Key buffer */
  const uint8_t *pmKey;
  /** Pointer to original Nonce buffer */
  const uint8_t *pmNonce;
  /** Size of the Nonce in bytes. This must be set by the caller prior to calling Init.
       Possible values are {7,8,9,10,11,12,13}  */
  int32_t   mNonceSize;
  /** This is the current IV value for encryption */
  uint32_t   amIvCTR[4];
  /** This is the current IV value for authentication */
  uint32_t   amIvCBC[4];
  /** AES Key length in bytes. This must be set by the caller prior to calling Init */
  int32_t   mKeySize;
  /** Pointer to Authentication TAG. This value must be set in decryption, and this TAG will be verified */
  const uint8_t *pmTag;
  /** Size of the Tag to return. This must be set by the caller prior to calling Init.
      Possible values are values are {4,6,8,10,12,14,16} */
  int32_t mTagSize;
  /** Size of the associated data to be processed yet. This must be set by the caller prior to calling Init */
  int32_t mAssDataSize;
  /** Size of the payload data to be processed yet size. This must be set by the caller prior to calling Init  */
  int32_t mPayloadSize;
  /** AES Expanded key. For internal use.  - Don't care with this router */
  uint32_t   amExpKey[1];
  /** Temp Buffer  - Don't care with this router*/
  uint32_t amTmpBuf[1];
  /**  Number of bytes actually in use */
  int32_t mTmpBufUse;
  /* Additional information to maintain context to communicate with HAL */
  CRYP_HandleTypeDef     CrypHandle;
  uint8_t Iv_endian[16] /*__attribute__((aligned(4)))*/;
  uint8_t Key_endian[CA_CRL_AES256_KEY] /*__attribute__((aligned(4)))*/;
  uint8_t B0[16] /*__attribute__((aligned(4)))*/;
  uint32_t flags;
} CA_AESCCMctx_stt;

/* Exported functions -------------------------------------------------------*/
#if (CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE)
int32_t CA_AES_CCM_Encrypt_Init(CA_AESCCMctx_stt *P_pAESCCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pNonce);
int32_t CA_AES_CCM_Encrypt_Append(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
int32_t CA_AES_CCM_Encrypt_Finish(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_ENCRYPT_ENABLE */
#if (CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE)
int32_t CA_AES_CCM_Decrypt_Init(CA_AESCCMctx_stt *P_pAESCCMctx,
                                const uint8_t *P_pKey,
                                const uint8_t *P_pNonce);
int32_t CA_AES_CCM_Decrypt_Append(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  const uint8_t *P_pInputBuffer,
                                  int32_t        P_inputSize,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
int32_t CA_AES_CCM_Decrypt_Finish(CA_AESCCMctx_stt *P_pAESCCMctx,
                                  uint8_t       *P_pOutputBuffer,
                                  int32_t       *P_pOutputSize);
#endif /* CA_ROUTE_AES_CCM & CA_ROUTE_AES_CFG_DECRYPT_ENABLE */
int32_t CA_AES_CCM_Header_Append(CA_AESCCMctx_stt *P_pAESCCMctx,
                                 const uint8_t *P_pInputBuffer,
                                 int32_t        P_inputSize);
#endif /* (CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_HAL */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CCM */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CMAC */
#if defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL)
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
  /* Additional information to maintain context to communicate with HAL */
  uint8_t mac[CA_CRL_AES_BLOCK];
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
#endif /* (CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CMAC */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES ECB */
#if defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_HAL)
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
  /* Additional information to maintain context to communicate with HAL */
  CRYP_HandleTypeDef     CrypHandle;
  uint8_t Iv_endian[16];
  uint8_t Key_endian[CA_CRL_AES256_KEY];
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
#endif /* (CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_HAL */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES ECB */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES GCM */
#if defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)
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
  /* Additional information to maintain context to communicate with HAL */
  CRYP_HandleTypeDef     CrypHandle;
  uint8_t Iv_endian[16];
  uint8_t Key_endian[CA_CRL_AES256_KEY];
  uint32_t flags;
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
#endif /* (CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_HAL */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES GCM */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_AES_HAL_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

