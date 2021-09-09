/**
  ******************************************************************************
  * @file    kms_enc_dec.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module encryption & decryption functionalities.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef KMS_ENC_DEC_H
#define KMS_ENC_DEC_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_ENC_DEC Encryption & Decryption services
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/


/**
  * @}
  */


/** @addtogroup KMS_API KMS APIs (PKCS#11 Standard Compliant)
  * @{
  */

CK_RV          KMS_EncryptInit(CK_SESSION_HANDLE hSession,
                               CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey);

CK_RV          KMS_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                           CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData,
                           CK_ULONG_PTR pulEncryptedDataLen);

CK_RV          KMS_EncryptUpdate(CK_SESSION_HANDLE hSession,
                                 CK_BYTE_PTR pPart, CK_ULONG ulPartLen,
                                 CK_BYTE_PTR pEncryptedPart,
                                 CK_ULONG_PTR pulEncryptedPartLen);

CK_RV          KMS_EncryptFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pLastEncryptedPart,
                                CK_ULONG_PTR pulLastEncryptedPartLen);

CK_RV          KMS_DecryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                               CK_OBJECT_HANDLE hKey);

CK_RV          KMS_Decrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedData,
                           CK_ULONG ulEncryptedDataLen,
                           CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen);

CK_RV          KMS_DecryptUpdate(CK_SESSION_HANDLE hSession,
                                 CK_BYTE_PTR pEncryptedPart,
                                 CK_ULONG ulEncryptedPartLen,
                                 CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen);


CK_RV          KMS_DecryptFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pLastPart, CK_ULONG_PTR pulLastPartLen);


/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* KMS_ENC_DEC_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

