/**
  ******************************************************************************
  * @file    se_interface_kms.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module access when securely enclaved into Secure Engine
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
#ifndef SE_INTERFACE_KMS_H
#define SE_INTERFACE_KMS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/** @addtogroup tKMS User interface to Key Management Services (tKMS)
  * @{
  */

/** @addtogroup KMS_TKMS_SE tKMS Isolated access (enclaved in Secure Engine) APIs (PKCS#11 Standard Compliant)
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

/** @addtogroup KMS_TKMS_SE_Exported_Functions Exported Functions
  * @{
  */

CK_RV SE_KMS_Initialize(CK_VOID_PTR pInitArgs);
CK_RV SE_KMS_Finalize(CK_VOID_PTR pReserved);
CK_RV SE_KMS_GetInfo(CK_INFO_PTR   pInfo);
CK_RV SE_KMS_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList);
CK_RV SE_KMS_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList,
                         CK_ULONG_PTR pulCount);
CK_RV SE_KMS_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo);
CK_RV SE_KMS_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo);
CK_RV SE_KMS_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR pInfo);
CK_RV SE_KMS_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
                         CK_VOID_PTR pApplication, CK_NOTIFY Notify,
                         CK_SESSION_HANDLE_PTR phSession);
CK_RV SE_KMS_CloseSession(CK_SESSION_HANDLE hSession);
CK_RV SE_KMS_CreateObject(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject);
CK_RV SE_KMS_DestroyObject(CK_SESSION_HANDLE hSession,
                           CK_OBJECT_HANDLE hObject);
CK_RV SE_KMS_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
                               CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
CK_RV SE_KMS_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
                               CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
CK_RV SE_KMS_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                             CK_ULONG ulCount);
CK_RV SE_KMS_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
                         CK_ULONG ulMaxObjectCount,  CK_ULONG_PTR pulObjectCount);
CK_RV SE_KMS_FindObjectsFinal(CK_SESSION_HANDLE hSession);

CK_RV SE_KMS_EncryptInit(CK_SESSION_HANDLE hSession,
                         CK_MECHANISM_PTR  pMechanism,
                         CK_OBJECT_HANDLE  hKey);

CK_RV SE_KMS_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR    pData,
                     CK_ULONG  ulDataLen, CK_BYTE_PTR  pEncryptedData,
                     CK_ULONG_PTR      pulEncryptedDataLen);

CK_RV SE_KMS_EncryptUpdate(CK_SESSION_HANDLE hSession,
                           CK_BYTE_PTR       pPart,
                           CK_ULONG          ulPartLen,
                           CK_BYTE_PTR       pEncryptedPart,
                           CK_ULONG_PTR      pulEncryptedPartLen);

CK_RV SE_KMS_EncryptFinal(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR       pLastEncryptedPart,
                          CK_ULONG_PTR      pulLastEncryptedPartLen);

CK_RV SE_KMS_DecryptInit(CK_SESSION_HANDLE hSession,
                         CK_MECHANISM_PTR  pMechanism,
                         CK_OBJECT_HANDLE  hKey);

CK_RV SE_KMS_Decrypt(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR       pEncryptedData,
                     CK_ULONG          ulEncryptedDataLen,
                     CK_BYTE_PTR       pData,
                     CK_ULONG_PTR      pulDataLen);

CK_RV SE_KMS_DecryptUpdate(CK_SESSION_HANDLE hSession,
                           CK_BYTE_PTR       pEncryptedPart,
                           CK_ULONG          ulEncryptedPartLen,
                           CK_BYTE_PTR       pPart,
                           CK_ULONG_PTR      pulPartLen);

CK_RV SE_KMS_DecryptFinal(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR       pLastPart,
                          CK_ULONG_PTR      pulLastPartLen);

CK_RV SE_KMS_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism);
CK_RV SE_KMS_Digest(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                    CK_ULONG ulDataLen, CK_BYTE_PTR pDigest,
                    CK_ULONG_PTR pulDigestLen);
CK_RV SE_KMS_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen);
CK_RV SE_KMS_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
                         CK_ULONG_PTR pulDigestLen);

CK_RV SE_KMS_SignInit(CK_SESSION_HANDLE hSession,
                      CK_MECHANISM_PTR  pMechanism,
                      CK_OBJECT_HANDLE  hKey);

CK_RV SE_KMS_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                  CK_ULONG  ulDataLen, CK_BYTE_PTR  pSignature,
                  CK_ULONG_PTR pulSignatureLen);

CK_RV SE_KMS_VerifyInit(CK_SESSION_HANDLE hSession,
                        CK_MECHANISM_PTR  pMechanism,
                        CK_OBJECT_HANDLE  hKey);

CK_RV SE_KMS_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                    CK_ULONG  ulDataLen, CK_BYTE_PTR  pSignature,
                    CK_ULONG  ulSignatureLen);

CK_RV SE_KMS_DeriveKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                       CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR  pTemplate,
                       CK_ULONG  ulAttributeCount, CK_OBJECT_HANDLE_PTR  phKey);
CK_RV SE_KMS_GenerateKeyPair(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                             CK_ATTRIBUTE_PTR pPublicKeyTemplate, CK_ULONG  ulPublicKeyAttributeCount,
                             CK_ATTRIBUTE_PTR pPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount,
                             CK_OBJECT_HANDLE_PTR phPublicKey, CK_OBJECT_HANDLE_PTR phPrivateKey);
CK_RV SE_KMS_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR  pRandomData,
                            CK_ULONG  ulRandomLen);

CK_RV SE_KMS_ImportBlob(CK_BYTE_PTR pHdr, CK_BYTE_PTR pFlash);
CK_RV SE_KMS_LockKeys(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount);
CK_RV SE_KMS_LockServices(CK_ULONG_PTR pServices, CK_ULONG ulCount);


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

#endif /* SE_INTERFACE_KMS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

