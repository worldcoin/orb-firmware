/**
  ******************************************************************************
  * @file    kms_digest.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module digest functionalities.
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
#ifndef KMS_DIGEST_H
#define KMS_DIGEST_H

#ifdef __cplusplus
extern "C" {
#endif


/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_DIGEST Digest services
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
CK_RV   KMS_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism);
CK_RV   KMS_Digest(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                   CK_ULONG ulDataLen, CK_BYTE_PTR pDigest, CK_ULONG_PTR pulDigestLen);
CK_RV   KMS_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen);
CK_RV   KMS_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
                        CK_ULONG_PTR pulDigestLen);

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* KMS_DIGEST_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

