/**
  ******************************************************************************
  * @file    kms_sign_verify.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module sign and verify functionalities.
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
#ifndef KMS_SIGN_VERIFY_H
#define KMS_SIGN_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_SIGN Sign and Verify services
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

CK_RV   KMS_SignInit(CK_SESSION_HANDLE hSession,
                     CK_MECHANISM_PTR  pMechanism,
                     CK_OBJECT_HANDLE  hKey);


CK_RV   KMS_Sign(CK_SESSION_HANDLE hSession,     CK_BYTE_PTR       pData,
                 CK_ULONG          ulDataLen,    CK_BYTE_PTR       pSignature,
                 CK_ULONG_PTR      pulSignatureLen);

CK_RV   KMS_VerifyInit(CK_SESSION_HANDLE hSession,
                       CK_MECHANISM_PTR  pMechanism,
                       CK_OBJECT_HANDLE  hKey);

CK_RV   KMS_Verify(CK_SESSION_HANDLE hSession,  CK_BYTE_PTR       pData,
                   CK_ULONG          ulDataLen, CK_BYTE_PTR       pSignature,
                   CK_ULONG          ulSignatureLen);

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* KMS_SIGN_VERIFY_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

