/**
  ******************************************************************************
  * @file    pkcs11f.h
  * @author  MCD Application Team
  * @brief   Header for KMS module that implements the PKCS #11 Cryptographic
  *          Token Interface Base Specification Version 2.40 Plus Errata 01
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

/**
  * This file is part of the implementation of PKCS #11 Cryptographic Token
  * Interface Base Specification Version 2.40 Plus Errata 01 available here:
  * docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/errata01/os/pkcs11-base-v2.40-errata01-os-complete.html
  * See About_PKCS11.txt file in this directory to get more information on licensing.
  */

#ifndef PKCS11F_H
#define PKCS11F_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup KMS_PKCS11 PKCS11 Standard Definitions
  * @brief Definitions from
  * <a href=docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/errata01/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *        PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a>
  * @{
  */

/*
 * General-purpose functions
 */

/* C_Initialize initializes the Cryptoki library  */
CK_DECLARE_FUNCTION(CK_RV,    C_Initialize)(CK_VOID_PTR pInitArgs);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Initialize)(CK_VOID_PTR pInitArgs);

/* C_Finalize is called to indicate that an application is finished with the Cryptoki library */
CK_DECLARE_FUNCTION(CK_RV,    C_Finalize)(CK_VOID_PTR pReserved);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Finalize)(CK_VOID_PTR pReserved);

/* C_GetInfo returns general information about Cryptoki  */
CK_DECLARE_FUNCTION(CK_RV,    C_GetInfo)(CK_INFO_PTR pInfo);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetInfo)(CK_INFO_PTR pInfo);

/* C_GetFunctionList obtains a pointer to the Cryptoki library list of function pointers */
CK_DECLARE_FUNCTION(CK_RV,    C_GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR ppFunctionList);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR ppFunctionList);

/*
 * Slot and token management functions
 */

/* C_GetSlotList is used to obtain a list of slots in the system */
CK_DECLARE_FUNCTION(CK_RV,    C_GetSlotList)(CK_BBOOL       tokenPresent,
                                             CK_SLOT_ID_PTR pSlotList,
                                             CK_ULONG_PTR   pulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetSlotList)(CK_BBOOL       tokenPresent,
                                                     CK_SLOT_ID_PTR pSlotList,
                                                     CK_ULONG_PTR   pulCount);

/* C_GetSlotInfo obtains information about a particular slot in the system */
CK_DECLARE_FUNCTION(CK_RV,    C_GetSlotInfo)(CK_SLOT_ID       slotID,
                                             CK_SLOT_INFO_PTR pInfo);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetSlotInfo)(CK_SLOT_ID       slotID,
                                                     CK_SLOT_INFO_PTR pInfo);

/* C_GetTokenInfo obtains information about a particular token in the system */
CK_DECLARE_FUNCTION(CK_RV,    C_GetTokenInfo)(CK_SLOT_ID        slotID,
                                              CK_TOKEN_INFO_PTR pInfo);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetTokenInfo)(CK_SLOT_ID        slotID,
                                                      CK_TOKEN_INFO_PTR pInfo);

/* C_WaitForSlotEvent waits for a slot event, such as token insertion or token removal, to occur  */
CK_DECLARE_FUNCTION(CK_RV,    C_WaitForSlotEvent)(CK_FLAGS       flags,
                                                  CK_SLOT_ID_PTR pSlot,
                                                  CK_VOID_PTR pReserved);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_WaitForSlotEvent)(CK_FLAGS       flags,
                                                          CK_SLOT_ID_PTR pSlot,
                                                          CK_VOID_PTR pReserved);

/* C_GetMechanismList is used to obtain a list of mechanism types supported by a token */
CK_DECLARE_FUNCTION(CK_RV,    C_GetMechanismList)(CK_SLOT_ID            slotID,
                                                  CK_MECHANISM_TYPE_PTR pMechanismList,
                                                  CK_ULONG_PTR          pulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetMechanismList)(CK_SLOT_ID            slotID,
                                                          CK_MECHANISM_TYPE_PTR pMechanismList,
                                                          CK_ULONG_PTR          pulCount);

/* C_GetMechanismInfo obtains information about a particular mechanism possibly supported by a token */
CK_DECLARE_FUNCTION(CK_RV,    C_GetMechanismInfo)(CK_SLOT_ID            slotID,
                                                  CK_MECHANISM_TYPE     type,
                                                  CK_MECHANISM_INFO_PTR pInfo);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetMechanismInfo)(CK_SLOT_ID            slotID,
                                                          CK_MECHANISM_TYPE     type,
                                                          CK_MECHANISM_INFO_PTR pInfo);

/* C_InitToken initializes a token */
CK_DECLARE_FUNCTION(CK_RV,    C_InitToken)(CK_SLOT_ID      slotID,
                                           CK_UTF8CHAR_PTR pPin,
                                           CK_ULONG        ulPinLen,
                                           CK_UTF8CHAR_PTR pLabel);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_InitToken)(CK_SLOT_ID      slotID,
                                                   CK_UTF8CHAR_PTR pPin,
                                                   CK_ULONG        ulPinLen,
                                                   CK_UTF8CHAR_PTR pLabel);

/* C_InitPIN initializes the normal user PIN */
CK_DECLARE_FUNCTION(CK_RV,    C_InitPIN)(CK_SESSION_HANDLE hSession,
                                         CK_UTF8CHAR_PTR   pPin,
                                         CK_ULONG          ulPinLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_InitPIN)(CK_SESSION_HANDLE hSession,
                                                 CK_UTF8CHAR_PTR   pPin,
                                                 CK_ULONG          ulPinLen);

/* C_SetPIN modifies the PIN of the user that is currently logged in, */
/* or the CKU_USER PIN if the session is not logged in                */
CK_DECLARE_FUNCTION(CK_RV,    C_SetPIN)(CK_SESSION_HANDLE hSession,
                                        CK_UTF8CHAR_PTR   pOldPin,
                                        CK_ULONG          ulOldLen,
                                        CK_UTF8CHAR_PTR   pNewPin,
                                        CK_ULONG          ulNewLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SetPIN)(CK_SESSION_HANDLE hSession,
                                                CK_UTF8CHAR_PTR   pOldPin,
                                                CK_ULONG          ulOldLen,
                                                CK_UTF8CHAR_PTR   pNewPin,
                                                CK_ULONG          ulNewLen);

/*
 * Session management functions
 */

/* C_OpenSession opens a session between an application and a token in a particular slot */
CK_DECLARE_FUNCTION(CK_RV,    C_OpenSession)(CK_SLOT_ID            slotID,
                                             CK_FLAGS              flags,
                                             CK_VOID_PTR           pApplication,
                                             CK_NOTIFY             Notify,
                                             CK_SESSION_HANDLE_PTR phSession);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_OpenSession)(CK_SLOT_ID            slotID,
                                                     CK_FLAGS              flags,
                                                     CK_VOID_PTR           pApplication,
                                                     CK_NOTIFY             Notify,
                                                     CK_SESSION_HANDLE_PTR phSession);

/* C_CloseSession closes a session between an application and a token */
CK_DECLARE_FUNCTION(CK_RV,    C_CloseSession)(CK_SESSION_HANDLE hSession);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_CloseSession)(CK_SESSION_HANDLE hSession);

/* C_CloseAllSessions closes all sessions an application has with a token */
CK_DECLARE_FUNCTION(CK_RV,    C_CloseAllSessions)(CK_SLOT_ID     slotID);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_CloseAllSessions)(CK_SLOT_ID     slotID);

/* C_GetSessionInfo obtains information about a session */
CK_DECLARE_FUNCTION(CK_RV,    C_GetSessionInfo)(CK_SESSION_HANDLE   hSession,
                                                CK_SESSION_INFO_PTR pInfo);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetSessionInfo)(CK_SESSION_HANDLE   hSession,
                                                        CK_SESSION_INFO_PTR pInfo);

/* C_GetOperationState obtains a copy of the cryptographic operations state of a session, */
/* encoded as a string of bytes                                                           */
CK_DECLARE_FUNCTION(CK_RV,    C_GetOperationState)(CK_SESSION_HANDLE hSession,
                                                   CK_BYTE_PTR       pOperationState,
                                                   CK_ULONG_PTR      pulOperationStateLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetOperationState)(CK_SESSION_HANDLE hSession,
                                                           CK_BYTE_PTR       pOperationState,
                                                           CK_ULONG_PTR      pulOperationStateLen);

/* C_SetOperationState restores the cryptographic operations state of a session from a string */
/* of bytes obtained with C_GetOperationState                                                 */
CK_DECLARE_FUNCTION(CK_RV,    C_SetOperationState)(CK_SESSION_HANDLE hSession,
                                                   CK_BYTE_PTR       pOperationState,
                                                   CK_ULONG          ulOperationStateLen,
                                                   CK_OBJECT_HANDLE hEncryptionKey,
                                                   CK_OBJECT_HANDLE hAuthenticationKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SetOperationState)(CK_SESSION_HANDLE hSession,
                                                           CK_BYTE_PTR       pOperationState,
                                                           CK_ULONG          ulOperationStateLen,
                                                           CK_OBJECT_HANDLE hEncryptionKey,
                                                           CK_OBJECT_HANDLE hAuthenticationKey);

/* C_Login logs a user into a token */
CK_DECLARE_FUNCTION(CK_RV,    C_Login)(CK_SESSION_HANDLE hSession,
                                       CK_USER_TYPE      userType,
                                       CK_UTF8CHAR_PTR   pPin,
                                       CK_ULONG          ulPinLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Login)(CK_SESSION_HANDLE hSession,
                                               CK_USER_TYPE      userType,
                                               CK_UTF8CHAR_PTR   pPin,
                                               CK_ULONG          ulPinLen);

/* C_Logout logs a user out from a token */
CK_DECLARE_FUNCTION(CK_RV,    C_Logout)(CK_SESSION_HANDLE hSession);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Logout)(CK_SESSION_HANDLE hSession);

/*
 * Object management functions
 */

/* C_CreateObject creates a new object */
CK_DECLARE_FUNCTION(CK_RV,    C_CreateObject)(CK_SESSION_HANDLE hSession,
                                              CK_ATTRIBUTE_PTR  pTemplate,
                                              CK_ULONG          ulCount,
                                              CK_OBJECT_HANDLE_PTR phObject);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_CreateObject)(CK_SESSION_HANDLE hSession,
                                                      CK_ATTRIBUTE_PTR  pTemplate,
                                                      CK_ULONG          ulCount,
                                                      CK_OBJECT_HANDLE_PTR phObject);

/* C_CopyObject copies an object, creating a new object for the copy */
CK_DECLARE_FUNCTION(CK_RV,    C_CopyObject)(CK_SESSION_HANDLE    hSession,
                                            CK_OBJECT_HANDLE     hObject,
                                            CK_ATTRIBUTE_PTR     pTemplate,
                                            CK_ULONG             ulCount,
                                            CK_OBJECT_HANDLE_PTR phNewObject);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_CopyObject)(CK_SESSION_HANDLE    hSession,
                                                    CK_OBJECT_HANDLE     hObject,
                                                    CK_ATTRIBUTE_PTR     pTemplate,
                                                    CK_ULONG             ulCount,
                                                    CK_OBJECT_HANDLE_PTR phNewObject);

/* C_DestroyObject destroys an object */
CK_DECLARE_FUNCTION(CK_RV,    C_DestroyObject)(CK_SESSION_HANDLE hSession,
                                               CK_OBJECT_HANDLE  hObject);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DestroyObject)(CK_SESSION_HANDLE hSession,
                                                       CK_OBJECT_HANDLE  hObject);

/* C_GetObjectSize gets the size of an object in bytes */
CK_DECLARE_FUNCTION(CK_RV,    C_GetObjectSize)(CK_SESSION_HANDLE hSession,
                                               CK_OBJECT_HANDLE  hObject,
                                               CK_ULONG_PTR      pulSize);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetObjectSize)(CK_SESSION_HANDLE hSession,
                                                       CK_OBJECT_HANDLE  hObject,
                                                       CK_ULONG_PTR      pulSize);

/* C_GetAttributeValue obtains the value of one or more attributes of an object */
CK_DECLARE_FUNCTION(CK_RV,    C_GetAttributeValue)(CK_SESSION_HANDLE hSession,
                                                   CK_OBJECT_HANDLE  hObject,
                                                   CK_ATTRIBUTE_PTR  pTemplate,
                                                   CK_ULONG          ulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetAttributeValue)(CK_SESSION_HANDLE hSession,
                                                           CK_OBJECT_HANDLE  hObject,
                                                           CK_ATTRIBUTE_PTR  pTemplate,
                                                           CK_ULONG          ulCount);

/* C_SetAttributeValue modifies the value of one or more attributes of an object */
CK_DECLARE_FUNCTION(CK_RV,    C_SetAttributeValue)(CK_SESSION_HANDLE hSession,
                                                   CK_OBJECT_HANDLE  hObject,
                                                   CK_ATTRIBUTE_PTR  pTemplate,
                                                   CK_ULONG          ulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SetAttributeValue)(CK_SESSION_HANDLE hSession,
                                                           CK_OBJECT_HANDLE  hObject,
                                                           CK_ATTRIBUTE_PTR  pTemplate,
                                                           CK_ULONG          ulCount);

/* C_FindObjectsInit initializes a search for token and session objects that match a template */
CK_DECLARE_FUNCTION(CK_RV,    C_FindObjectsInit)(CK_SESSION_HANDLE hSession,
                                                 CK_ATTRIBUTE_PTR  pTemplate,
                                                 CK_ULONG          ulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_FindObjectsInit)(CK_SESSION_HANDLE hSession,
                                                         CK_ATTRIBUTE_PTR  pTemplate,
                                                         CK_ULONG          ulCount);

/* C_FindObjects continues a search for token and session objects that match a template, */
/* obtaining additional object handles                                                   */
CK_DECLARE_FUNCTION(CK_RV,    C_FindObjects)(CK_SESSION_HANDLE    hSession,
                                             CK_OBJECT_HANDLE_PTR phObject,
                                             CK_ULONG             ulMaxObjectCount,
                                             CK_ULONG_PTR         pulObjectCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_FindObjects)(CK_SESSION_HANDLE    hSession,
                                                     CK_OBJECT_HANDLE_PTR phObject,
                                                     CK_ULONG             ulMaxObjectCount,
                                                     CK_ULONG_PTR         pulObjectCount);

/* C_FindObjectsFinal terminates a search for token and session objects */
CK_DECLARE_FUNCTION(CK_RV,    C_FindObjectsFinal)(CK_SESSION_HANDLE hSession);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_FindObjectsFinal)(CK_SESSION_HANDLE hSession);

/*
 * Encryption functions
 */

/* C_EncryptInit initializes an encryption operation */
CK_DECLARE_FUNCTION(CK_RV,    C_EncryptInit)(CK_SESSION_HANDLE hSession,
                                             CK_MECHANISM_PTR  pMechanism,
                                             CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_EncryptInit)(CK_SESSION_HANDLE hSession,
                                                     CK_MECHANISM_PTR  pMechanism,
                                                     CK_OBJECT_HANDLE  hKey);

/* C_Encrypt encrypts single-part data */
CK_DECLARE_FUNCTION(CK_RV,    C_Encrypt)(CK_SESSION_HANDLE hSession,
                                         CK_BYTE_PTR       pData,
                                         CK_ULONG          ulDataLen,
                                         CK_BYTE_PTR       pEncryptedData,
                                         CK_ULONG_PTR      pulEncryptedDataLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Encrypt)(CK_SESSION_HANDLE hSession,
                                                 CK_BYTE_PTR       pData,
                                                 CK_ULONG          ulDataLen,
                                                 CK_BYTE_PTR       pEncryptedData,
                                                 CK_ULONG_PTR      pulEncryptedDataLen);

/* C_EncryptUpdate continues a multiple-part encryption operation, processing another data part */
CK_DECLARE_FUNCTION(CK_RV,    C_EncryptUpdate)(CK_SESSION_HANDLE hSession,
                                               CK_BYTE_PTR       pPart,
                                               CK_ULONG          ulPartLen,
                                               CK_BYTE_PTR       pEncryptedPart,
                                               CK_ULONG_PTR      pulEncryptedPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_EncryptUpdate)(CK_SESSION_HANDLE hSession,
                                                       CK_BYTE_PTR       pPart,
                                                       CK_ULONG          ulPartLen,
                                                       CK_BYTE_PTR       pEncryptedPart,
                                                       CK_ULONG_PTR      pulEncryptedPartLen);

/* C_EncryptFinal finishes a multiple-part encryption operation */
CK_DECLARE_FUNCTION(CK_RV,    C_EncryptFinal)(CK_SESSION_HANDLE hSession,
                                              CK_BYTE_PTR       pLastEncryptedPart,
                                              CK_ULONG_PTR      pulLastEncryptedPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_EncryptFinal)(CK_SESSION_HANDLE hSession,
                                                      CK_BYTE_PTR       pLastEncryptedPart,
                                                      CK_ULONG_PTR      pulLastEncryptedPartLen);

/*
 * Decryption functions
 */

/* C_DecryptInit initializes a decryption operation */
CK_DECLARE_FUNCTION(CK_RV,    C_DecryptInit)(CK_SESSION_HANDLE hSession,
                                             CK_MECHANISM_PTR  pMechanism,
                                             CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DecryptInit)(CK_SESSION_HANDLE hSession,
                                                     CK_MECHANISM_PTR  pMechanism,
                                                     CK_OBJECT_HANDLE  hKey);

/* C_Decrypt decrypts encrypted data in a single part */
CK_DECLARE_FUNCTION(CK_RV,    C_Decrypt)(CK_SESSION_HANDLE hSession,
                                         CK_BYTE_PTR       pEncryptedData,
                                         CK_ULONG          ulEncryptedDataLen,
                                         CK_BYTE_PTR       pData,
                                         CK_ULONG_PTR      pulDataLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Decrypt)(CK_SESSION_HANDLE hSession,
                                                 CK_BYTE_PTR       pEncryptedData,
                                                 CK_ULONG          ulEncryptedDataLen,
                                                 CK_BYTE_PTR       pData,
                                                 CK_ULONG_PTR      pulDataLen);

/* C_DecryptUpdate continues a multiple-part decryption operation, processing another encrypted data part */
CK_DECLARE_FUNCTION(CK_RV,    C_DecryptUpdate)(CK_SESSION_HANDLE hSession,
                                               CK_BYTE_PTR       pEncryptedPart,
                                               CK_ULONG          ulEncryptedPartLen,
                                               CK_BYTE_PTR       pPart,
                                               CK_ULONG_PTR      pulPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DecryptUpdate)(CK_SESSION_HANDLE hSession,
                                                       CK_BYTE_PTR       pEncryptedPart,
                                                       CK_ULONG          ulEncryptedPartLen,
                                                       CK_BYTE_PTR       pPart,
                                                       CK_ULONG_PTR      pulPartLen);

/* C_DecryptFinal finishes a multiple-part decryption operation */
CK_DECLARE_FUNCTION(CK_RV,    C_DecryptFinal)(CK_SESSION_HANDLE hSession,
                                              CK_BYTE_PTR       pLastPart,
                                              CK_ULONG_PTR      pulLastPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DecryptFinal)(CK_SESSION_HANDLE hSession,
                                                      CK_BYTE_PTR       pLastPart,
                                                      CK_ULONG_PTR      pulLastPartLen);

/*
 * Message digesting functions
 */

/* C_DigestInit initializes a message-digesting operation */
CK_DECLARE_FUNCTION(CK_RV,    C_DigestInit)(CK_SESSION_HANDLE hSession,
                                            CK_MECHANISM_PTR  pMechanism);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DigestInit)(CK_SESSION_HANDLE hSession,
                                                    CK_MECHANISM_PTR  pMechanism);

/* C_Digest digests data in a single part */
CK_DECLARE_FUNCTION(CK_RV,    C_Digest)(CK_SESSION_HANDLE hSession,
                                        CK_BYTE_PTR       pData,
                                        CK_ULONG          ulDataLen,
                                        CK_BYTE_PTR       pDigest,
                                        CK_ULONG_PTR      pulDigestLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Digest)(CK_SESSION_HANDLE hSession,
                                                CK_BYTE_PTR       pData,
                                                CK_ULONG          ulDataLen,
                                                CK_BYTE_PTR       pDigest,
                                                CK_ULONG_PTR      pulDigestLen);

/* C_DigestUpdate continues a multiple-part message-digesting operation, processing another data part */
CK_DECLARE_FUNCTION(CK_RV,    C_DigestUpdate)(CK_SESSION_HANDLE hSession,
                                              CK_BYTE_PTR       pPart,
                                              CK_ULONG          ulPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DigestUpdate)(CK_SESSION_HANDLE hSession,
                                                      CK_BYTE_PTR       pPart,
                                                      CK_ULONG          ulPartLen);

/* C_DigestKey continues a multiple-part message-digesting operation by digesting the value of a secret key */
CK_DECLARE_FUNCTION(CK_RV,    C_DigestKey)(CK_SESSION_HANDLE hSession,
                                           CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DigestKey)(CK_SESSION_HANDLE hSession,
                                                   CK_OBJECT_HANDLE  hKey);

/* C_DigestFinal finishes a multiple-part message-digesting operation, returning the message digest */
CK_DECLARE_FUNCTION(CK_RV,    C_DigestFinal)(CK_SESSION_HANDLE hSession,
                                             CK_BYTE_PTR       pDigest,
                                             CK_ULONG_PTR      pulDigestLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DigestFinal)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR       pDigest,
                                                     CK_ULONG_PTR      pulDigestLen);

/*
 * Signing and MACing functions
 */

/* C_SignInit initializes a signature operation, where the signature is an appendix to the data */
CK_DECLARE_FUNCTION(CK_RV,    C_SignInit)(CK_SESSION_HANDLE hSession,
                                          CK_MECHANISM_PTR  pMechanism,
                                          CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SignInit)(CK_SESSION_HANDLE hSession,
                                                  CK_MECHANISM_PTR  pMechanism,
                                                  CK_OBJECT_HANDLE  hKey);

/* C_Sign signs data in a single part, where the signature is an appendix to the data */
CK_DECLARE_FUNCTION(CK_RV,    C_Sign)(CK_SESSION_HANDLE hSession,
                                      CK_BYTE_PTR       pData,
                                      CK_ULONG          ulDataLen,
                                      CK_BYTE_PTR       pSignature,
                                      CK_ULONG_PTR      pulSignatureLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Sign)(CK_SESSION_HANDLE hSession,
                                              CK_BYTE_PTR       pData,
                                              CK_ULONG          ulDataLen,
                                              CK_BYTE_PTR       pSignature,
                                              CK_ULONG_PTR      pulSignatureLen);

/* C_SignUpdate continues a multiple-part signature operation, processing another data part */
CK_DECLARE_FUNCTION(CK_RV,    C_SignUpdate)(CK_SESSION_HANDLE hSession,
                                            CK_BYTE_PTR       pPart,
                                            CK_ULONG          ulPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SignUpdate)(CK_SESSION_HANDLE hSession,
                                                    CK_BYTE_PTR       pPart,
                                                    CK_ULONG          ulPartLen);

/* C_SignFinal finishes a multiple-part signature operation, returning the signature */
CK_DECLARE_FUNCTION(CK_RV,    C_SignFinal)(CK_SESSION_HANDLE hSession,
                                           CK_BYTE_PTR       pSignature,
                                           CK_ULONG_PTR      pulSignatureLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SignFinal)(CK_SESSION_HANDLE hSession,
                                                   CK_BYTE_PTR       pSignature,
                                                   CK_ULONG_PTR      pulSignatureLen);

/* C_SignRecoverInit initializes a signature operation, where the data can be recovered from the signature */
CK_DECLARE_FUNCTION(CK_RV,    C_SignRecoverInit)(CK_SESSION_HANDLE hSession,
                                                 CK_MECHANISM_PTR  pMechanism,
                                                 CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SignRecoverInit)(CK_SESSION_HANDLE hSession,
                                                         CK_MECHANISM_PTR  pMechanism,
                                                         CK_OBJECT_HANDLE  hKey);

/* C_SignRecover signs data in a single operation, where the data can be recovered from the signature */
CK_DECLARE_FUNCTION(CK_RV,    C_SignRecover)(CK_SESSION_HANDLE hSession,
                                             CK_BYTE_PTR       pData,
                                             CK_ULONG          ulDataLen,
                                             CK_BYTE_PTR       pSignature,
                                             CK_ULONG_PTR      pulSignatureLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SignRecover)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR       pData,
                                                     CK_ULONG          ulDataLen,
                                                     CK_BYTE_PTR       pSignature,
                                                     CK_ULONG_PTR      pulSignatureLen);

/* C_VerifyInit initializes a verification operation, where the signature is an appendix to the data */
CK_DECLARE_FUNCTION(CK_RV,    C_VerifyInit)(CK_SESSION_HANDLE hSession,
                                            CK_MECHANISM_PTR  pMechanism,
                                            CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_VerifyInit)(CK_SESSION_HANDLE hSession,
                                                    CK_MECHANISM_PTR  pMechanism,
                                                    CK_OBJECT_HANDLE  hKey);

/* C_Verify verifies a signature in a single-part operation, where the signature is an appendix to the data */
CK_DECLARE_FUNCTION(CK_RV,    C_Verify)(CK_SESSION_HANDLE hSession,
                                        CK_BYTE_PTR       pData,
                                        CK_ULONG          ulDataLen,
                                        CK_BYTE_PTR       pSignature,
                                        CK_ULONG          ulSignatureLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_Verify)(CK_SESSION_HANDLE hSession,
                                                CK_BYTE_PTR       pData,
                                                CK_ULONG          ulDataLen,
                                                CK_BYTE_PTR       pSignature,
                                                CK_ULONG          ulSignatureLen);

/* C_VerifyUpdate continues a multiple-part verification operation, processing another data part */
CK_DECLARE_FUNCTION(CK_RV,    C_VerifyUpdate)(CK_SESSION_HANDLE hSession,
                                              CK_BYTE_PTR       pPart,
                                              CK_ULONG          ulPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_VerifyUpdate)(CK_SESSION_HANDLE hSession,
                                                      CK_BYTE_PTR       pPart,
                                                      CK_ULONG          ulPartLen);

/* C_VerifyFinal finishes a multiple-part verification operation, checking the signature */
CK_DECLARE_FUNCTION(CK_RV,    C_VerifyFinal)(CK_SESSION_HANDLE hSession,
                                             CK_BYTE_PTR       pSignature,
                                             CK_ULONG          ulSignatureLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_VerifyFinal)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR       pSignature,
                                                     CK_ULONG          ulSignatureLen);

/* C_VerifyRecoverInit initializes a signature verification operation, where the data is recovered from the signature */
CK_DECLARE_FUNCTION(CK_RV,    C_VerifyRecoverInit)(CK_SESSION_HANDLE hSession,
                                                   CK_MECHANISM_PTR  pMechanism,
                                                   CK_OBJECT_HANDLE  hKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_VerifyRecoverInit)(CK_SESSION_HANDLE hSession,
                                                           CK_MECHANISM_PTR  pMechanism,
                                                           CK_OBJECT_HANDLE  hKey);

/* C_VerifyRecover verifies a signature in a single-part operation, where the data is recovered from the signature */
CK_DECLARE_FUNCTION(CK_RV,    C_VerifyRecover)(CK_SESSION_HANDLE hSession,
                                               CK_BYTE_PTR       pSignature,
                                               CK_ULONG          ulSignatureLen,
                                               CK_BYTE_PTR       pData,
                                               CK_ULONG_PTR      pulDataLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_VerifyRecover)(CK_SESSION_HANDLE hSession,
                                                       CK_BYTE_PTR       pSignature,
                                                       CK_ULONG          ulSignatureLen,
                                                       CK_BYTE_PTR       pData,
                                                       CK_ULONG_PTR      pulDataLen);

/*
 * Dual-function cryptographic functions
 */

/* C_DigestEncryptUpdate continues multiple-part digest and encryption operations, processing another data part */
CK_DECLARE_FUNCTION(CK_RV,    C_DigestEncryptUpdate)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR       pPart,
                                                     CK_ULONG          ulPartLen,
                                                     CK_BYTE_PTR       pEncryptedPart,
                                                     CK_ULONG_PTR      pulEncryptedPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DigestEncryptUpdate)(CK_SESSION_HANDLE hSession,
                                                             CK_BYTE_PTR       pPart,
                                                             CK_ULONG          ulPartLen,
                                                             CK_BYTE_PTR       pEncryptedPart,
                                                             CK_ULONG_PTR      pulEncryptedPartLen);

/* C_DecryptDigestUpdate continues a multiple-part combined decryption and digest operation, */
/* processing another data part                                                              */
CK_DECLARE_FUNCTION(CK_RV,    C_DecryptDigestUpdate)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR       pEncryptedPart,
                                                     CK_ULONG          ulEncryptedPartLen,
                                                     CK_BYTE_PTR       pPart,
                                                     CK_ULONG_PTR      pulPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DecryptDigestUpdate)(CK_SESSION_HANDLE hSession,
                                                             CK_BYTE_PTR       pEncryptedPart,
                                                             CK_ULONG          ulEncryptedPartLen,
                                                             CK_BYTE_PTR       pPart,
                                                             CK_ULONG_PTR      pulPartLen);

/* C_SignEncryptUpdate continues a multiple-part combined signature and encryption operation, */
/* processing another data part                                                               */
CK_DECLARE_FUNCTION(CK_RV,    C_SignEncryptUpdate)(CK_SESSION_HANDLE hSession,
                                                   CK_BYTE_PTR       pPart,
                                                   CK_ULONG          ulPartLen,
                                                   CK_BYTE_PTR       pEncryptedPart,
                                                   CK_ULONG_PTR      pulEncryptedPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SignEncryptUpdate)(CK_SESSION_HANDLE hSession,
                                                           CK_BYTE_PTR       pPart,
                                                           CK_ULONG          ulPartLen,
                                                           CK_BYTE_PTR       pEncryptedPart,
                                                           CK_ULONG_PTR      pulEncryptedPartLen);

/* C_DecryptVerifyUpdate continues a multiple-part combined decryption and verification operation, */
/* processing another data part                                                                    */
CK_DECLARE_FUNCTION(CK_RV,    C_DecryptVerifyUpdate)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR       pEncryptedPart,
                                                     CK_ULONG          ulEncryptedPartLen,
                                                     CK_BYTE_PTR       pPart,
                                                     CK_ULONG_PTR      pulPartLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DecryptVerifyUpdate)(CK_SESSION_HANDLE hSession,
                                                             CK_BYTE_PTR       pEncryptedPart,
                                                             CK_ULONG          ulEncryptedPartLen,
                                                             CK_BYTE_PTR       pPart,
                                                             CK_ULONG_PTR      pulPartLen);

/*
 * Key management functions
 */

/* C_GenerateKey generates a secret key or set of domain parameters, creating a new object */
CK_DECLARE_FUNCTION(CK_RV,    C_GenerateKey)(CK_SESSION_HANDLE    hSession,
                                             CK_MECHANISM_PTR     pMechanism,
                                             CK_ATTRIBUTE_PTR     pTemplate,
                                             CK_ULONG             ulCount,
                                             CK_OBJECT_HANDLE_PTR phKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GenerateKey)(CK_SESSION_HANDLE    hSession,
                                                     CK_MECHANISM_PTR     pMechanism,
                                                     CK_ATTRIBUTE_PTR     pTemplate,
                                                     CK_ULONG             ulCount,
                                                     CK_OBJECT_HANDLE_PTR phKey);

/* C_GenerateKeyPair generates a public/private key pair, creating new key objects */
CK_DECLARE_FUNCTION(CK_RV,    C_GenerateKeyPair)(CK_SESSION_HANDLE    hSession,
                                                 CK_MECHANISM_PTR     pMechanism,
                                                 CK_ATTRIBUTE_PTR     pPublicKeyTemplate,
                                                 CK_ULONG             ulPublicKeyAttributeCount,
                                                 CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,
                                                 CK_ULONG             ulPrivateKeyAttributeCount,
                                                 CK_OBJECT_HANDLE_PTR phPublicKey,
                                                 CK_OBJECT_HANDLE_PTR phPrivateKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GenerateKeyPair)(CK_SESSION_HANDLE    hSession,
                                                         CK_MECHANISM_PTR     pMechanism,
                                                         CK_ATTRIBUTE_PTR     pPublicKeyTemplate,
                                                         CK_ULONG             ulPublicKeyAttributeCount,
                                                         CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,
                                                         CK_ULONG             ulPrivateKeyAttributeCount,
                                                         CK_OBJECT_HANDLE_PTR phPublicKey,
                                                         CK_OBJECT_HANDLE_PTR phPrivateKey);

/* C_WrapKey wraps (i.e., encrypts) a private or secret key */
CK_DECLARE_FUNCTION(CK_RV,    C_WrapKey)(CK_SESSION_HANDLE hSession,
                                         CK_MECHANISM_PTR  pMechanism,
                                         CK_OBJECT_HANDLE  hWrappingKey,
                                         CK_OBJECT_HANDLE  hKey,
                                         CK_BYTE_PTR       pWrappedKey,
                                         CK_ULONG_PTR      pulWrappedKeyLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_WrapKey)(CK_SESSION_HANDLE hSession,
                                                 CK_MECHANISM_PTR  pMechanism,
                                                 CK_OBJECT_HANDLE  hWrappingKey,
                                                 CK_OBJECT_HANDLE  hKey,
                                                 CK_BYTE_PTR       pWrappedKey,
                                                 CK_ULONG_PTR      pulWrappedKeyLen);

/* C_UnwrapKey unwraps (i.e. decrypts) a wrapped key, creating a new private key or secret key object */
CK_DECLARE_FUNCTION(CK_RV,    C_UnwrapKey)(CK_SESSION_HANDLE    hSession,
                                           CK_MECHANISM_PTR     pMechanism,
                                           CK_OBJECT_HANDLE     hUnwrappingKey,
                                           CK_BYTE_PTR          pWrappedKey,
                                           CK_ULONG             ulWrappedKeyLen,
                                           CK_ATTRIBUTE_PTR     pTemplate,
                                           CK_ULONG             ulAttributeCount,
                                           CK_OBJECT_HANDLE_PTR phKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_UnwrapKey)(CK_SESSION_HANDLE    hSession,
                                                   CK_MECHANISM_PTR     pMechanism,
                                                   CK_OBJECT_HANDLE     hUnwrappingKey,
                                                   CK_BYTE_PTR          pWrappedKey,
                                                   CK_ULONG             ulWrappedKeyLen,
                                                   CK_ATTRIBUTE_PTR     pTemplate,
                                                   CK_ULONG             ulAttributeCount,
                                                   CK_OBJECT_HANDLE_PTR phKey);

/* C_DeriveKey derives a key from a base key, creating a new key object */
CK_DECLARE_FUNCTION(CK_RV,    C_DeriveKey)(CK_SESSION_HANDLE    hSession,
                                           CK_MECHANISM_PTR     pMechanism,
                                           CK_OBJECT_HANDLE     hBaseKey,
                                           CK_ATTRIBUTE_PTR     pTemplate,
                                           CK_ULONG             ulAttributeCount,
                                           CK_OBJECT_HANDLE_PTR phKey);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_DeriveKey)(CK_SESSION_HANDLE    hSession,
                                                   CK_MECHANISM_PTR     pMechanism,
                                                   CK_OBJECT_HANDLE     hBaseKey,
                                                   CK_ATTRIBUTE_PTR     pTemplate,
                                                   CK_ULONG             ulAttributeCount,
                                                   CK_OBJECT_HANDLE_PTR phKey);

/*
 * Random number generation functions
 */

/* C_SeedRandom mixes additional seed material into the token random number generator */
CK_DECLARE_FUNCTION(CK_RV,    C_SeedRandom)(CK_SESSION_HANDLE hSession,
                                            CK_BYTE_PTR       pSeed,
                                            CK_ULONG          ulSeedLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_SeedRandom)(CK_SESSION_HANDLE hSession,
                                                    CK_BYTE_PTR       pSeed,
                                                    CK_ULONG          ulSeedLen);

/* C_GenerateRandom generates random or pseudo-random data */
CK_DECLARE_FUNCTION(CK_RV,    C_GenerateRandom)(CK_SESSION_HANDLE hSession,
                                                CK_BYTE_PTR       pRandomData,
                                                CK_ULONG          ulRandomLen);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GenerateRandom)(CK_SESSION_HANDLE hSession,
                                                        CK_BYTE_PTR       pRandomData,
                                                        CK_ULONG          ulRandomLen);

/*
 * Parallel function management functions
 */

/* C_GetFunctionStatus is a legacy function which should simply return the value CKR_FUNCTION_NOT_PARALLEL */
CK_DECLARE_FUNCTION(CK_RV,    C_GetFunctionStatus)(CK_SESSION_HANDLE hSession);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_GetFunctionStatus)(CK_SESSION_HANDLE hSession);

/* C_CancelFunction is a legacy function which should simply return the value CKR_FUNCTION_NOT_PARALLEL */
CK_DECLARE_FUNCTION(CK_RV,    C_CancelFunction)(CK_SESSION_HANDLE hSession);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_CancelFunction)(CK_SESSION_HANDLE hSession);

/*
 * Vendor defined specific functions
 */

/* C_STM_ImportBlob import encrypted image containing object blobs */
CK_DECLARE_FUNCTION(CK_RV,    C_STM_ImportBlob)(CK_BYTE_PTR pData);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_STM_ImportBlob)(CK_BYTE_PTR pData);
/* C_STM_LockKeys lock key usage */
CK_DECLARE_FUNCTION(CK_RV,    C_STM_LockKeys)(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_STM_LockKeys)(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount);
/* C_STM_LockServices lock services usage */
CK_DECLARE_FUNCTION(CK_RV,    C_STM_LockServices)(CK_ULONG_PTR pServices, CK_ULONG ulCount);
CK_DECLARE_FUNCTION_POINTER(CK_RV, CK_C_STM_LockServices)(CK_ULONG_PTR pServices, CK_ULONG ulCount);

struct CK_FUNCTION_LIST
{
  CK_VERSION version;

  CK_C_Initialize C_Initialize;
  CK_C_Finalize C_Finalize;
  CK_C_GetInfo C_GetInfo;
  CK_C_GetFunctionList C_GetFunctionList;
  CK_C_GetSlotList C_GetSlotList;
  CK_C_GetSlotInfo C_GetSlotInfo;
  CK_C_GetTokenInfo C_GetTokenInfo;
  CK_C_GetMechanismList C_GetMechanismList;
  CK_C_GetMechanismInfo C_GetMechanismInfo;
  CK_C_InitToken C_InitToken;
  CK_C_InitPIN C_InitPIN;
  CK_C_SetPIN C_SetPIN;
  CK_C_OpenSession C_OpenSession;
  CK_C_CloseSession C_CloseSession;
  CK_C_CloseAllSessions C_CloseAllSessions;
  CK_C_GetSessionInfo C_GetSessionInfo;
  CK_C_GetOperationState C_GetOperationState;
  CK_C_SetOperationState C_SetOperationState;
  CK_C_Login C_Login;
  CK_C_Logout C_Logout;
  CK_C_CreateObject C_CreateObject;
  CK_C_CopyObject C_CopyObject;
  CK_C_DestroyObject C_DestroyObject;
  CK_C_GetObjectSize C_GetObjectSize;
  CK_C_GetAttributeValue C_GetAttributeValue;
  CK_C_SetAttributeValue C_SetAttributeValue;
  CK_C_FindObjectsInit C_FindObjectsInit;
  CK_C_FindObjects C_FindObjects;
  CK_C_FindObjectsFinal C_FindObjectsFinal;
  CK_C_EncryptInit C_EncryptInit;
  CK_C_Encrypt C_Encrypt;
  CK_C_EncryptUpdate C_EncryptUpdate;
  CK_C_EncryptFinal C_EncryptFinal;
  CK_C_DecryptInit C_DecryptInit;
  CK_C_Decrypt C_Decrypt;
  CK_C_DecryptUpdate C_DecryptUpdate;
  CK_C_DecryptFinal C_DecryptFinal;
  CK_C_DigestInit C_DigestInit;
  CK_C_Digest C_Digest;
  CK_C_DigestUpdate C_DigestUpdate;
  CK_C_DigestKey C_DigestKey;
  CK_C_DigestFinal C_DigestFinal;
  CK_C_SignInit C_SignInit;
  CK_C_Sign C_Sign;
  CK_C_SignUpdate C_SignUpdate;
  CK_C_SignFinal C_SignFinal;
  CK_C_SignRecoverInit C_SignRecoverInit;
  CK_C_SignRecover C_SignRecover;
  CK_C_VerifyInit C_VerifyInit;
  CK_C_Verify C_Verify;
  CK_C_VerifyUpdate C_VerifyUpdate;
  CK_C_VerifyFinal C_VerifyFinal;
  CK_C_VerifyRecoverInit C_VerifyRecoverInit;
  CK_C_VerifyRecover C_VerifyRecover;
  CK_C_DigestEncryptUpdate C_DigestEncryptUpdate;
  CK_C_DecryptDigestUpdate C_DecryptDigestUpdate;
  CK_C_SignEncryptUpdate C_SignEncryptUpdate;
  CK_C_DecryptVerifyUpdate C_DecryptVerifyUpdate;
  CK_C_GenerateKey C_GenerateKey;
  CK_C_GenerateKeyPair C_GenerateKeyPair;
  CK_C_WrapKey C_WrapKey;
  CK_C_UnwrapKey C_UnwrapKey;
  CK_C_DeriveKey C_DeriveKey;
  CK_C_SeedRandom C_SeedRandom;
  CK_C_GenerateRandom C_GenerateRandom;
  CK_C_GetFunctionStatus C_GetFunctionStatus;
  CK_C_CancelFunction C_CancelFunction;
  CK_C_WaitForSlotEvent C_WaitForSlotEvent;
  /* Vendor specific */
  CK_C_STM_ImportBlob C_STM_ImportBlob;
  CK_C_STM_LockKeys C_STM_LockKeys;
  CK_C_STM_LockServices C_STM_LockServices;
};

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* PKCS11F_H */
