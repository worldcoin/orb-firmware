/**
  ******************************************************************************
  * @file    kms_ext_token.c
  * @author  MCD Application Team
  * @brief   This file contains implementation for the External Token services
  *          to Key Management Services.
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"                /* PKCS11 definitions */
#include "kms_init.h"           /* KMS session services */
#include "kms_ext_token.h"      /* KMS External Token services */


/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

#ifdef KMS_EXT_TOKEN_ENABLED

/** @addtogroup KMS_EXTTOKEN External Token services
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/** @addtogroup KMS_EXTTOKEN_Private_Functions Private Functions
  * @{
  */

CK_RV KMS_EXT_TOKEN_Initialize(CK_VOID_PTR pInitArgs);
CK_RV KMS_EXT_TOKEN_Finalize(CK_VOID_PTR pReserved);
CK_RV KMS_EXT_TOKEN_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
                                CK_VOID_PTR pApplication,  CK_NOTIFY,
                                CK_SESSION_HANDLE_PTR phSession);
CK_RV KMS_EXT_TOKEN_CloseSession(CK_SESSION_HANDLE hSession);
CK_RV KMS_EXT_TOKEN_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE  hObject,
                                      CK_ATTRIBUTE_PTR  pTemplate, CK_ULONG  ulCount);
CK_RV KMS_EXT_TOKEN_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE  hObject,
                                      CK_ATTRIBUTE_PTR  pTemplate, CK_ULONG          ulCount);
CK_RV KMS_EXT_TOKEN_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                                    CK_ULONG ulCount);
CK_RV KMS_EXT_TOKEN_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
                                CK_ULONG ulMaxObjectCount,  CK_ULONG_PTR pulObjectCount);
CK_RV KMS_EXT_TOKEN_FindObjectsFinal(CK_SESSION_HANDLE hSession);
CK_RV KMS_EXT_TOKEN_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR  pMechanism,
                             CK_OBJECT_HANDLE  hKey);
CK_RV KMS_EXT_TOKEN_Sign(CK_SESSION_HANDLE     hSession, CK_BYTE_PTR pData,
                         CK_ULONG  ulDataLen, CK_BYTE_PTR  pSignature, CK_ULONG_PTR pulSignatureLen);
CK_RV KMS_EXT_TOKEN_VerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR  pMechanism,
                               CK_OBJECT_HANDLE  hKey);
CK_RV KMS_EXT_TOKEN_Verify(CK_SESSION_HANDLE     hSession,  CK_BYTE_PTR   pData,
                           CK_ULONG ulDataLen, CK_BYTE_PTR  pSignature,
                           CK_ULONG ulSignatureLen);
CK_RV KMS_EXT_TOKEN_GenerateKeyPair(CK_SESSION_HANDLE    hSession,
                                    CK_MECHANISM_PTR     pMechanism,
                                    CK_ATTRIBUTE_PTR     pPublicKeyTemplate,
                                    CK_ULONG             ulPublicKeyAttributeCount,
                                    CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,
                                    CK_ULONG             ulPrivateKeyAttributeCount,
                                    CK_OBJECT_HANDLE_PTR phPublicKey,
                                    CK_OBJECT_HANDLE_PTR phPrivateKey);
CK_RV KMS_EXT_TOKEN_DeriveKey(CK_SESSION_HANDLE  hSession, CK_MECHANISM_PTR  pMechanism,
                              CK_OBJECT_HANDLE   hBaseKey, CK_ATTRIBUTE_PTR     pTemplate,
                              CK_ULONG   ulAttributeCount, CK_OBJECT_HANDLE_PTR phKey);

CK_RV KMS_EXT_TOKEN_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR  pRandomData,
                                   CK_ULONG          ulRandomLen);

/* Private function ----------------------------------------------------------*/

/**
  * @brief  This function is called upon @ref C_Initialize call on External Token
  * @note   Refer to @ref C_Initialize function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pInitArgs
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_Initialize(CK_VOID_PTR pInitArgs)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_Finalize call on External Token
  * @note   Refer to @ref C_Finalize function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pReserved
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_Finalize(CK_VOID_PTR pReserved)
{
  return CKR_OK;
}


/**
  * @brief  This function is called upon @ref C_OpenSession call on External Token
  * @note   Refer to @ref C_OpenSession function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID
  * @param  flags
  * @param  pApplication
  * @param  Notify
  * @param  phSession
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
                                CK_VOID_PTR pApplication,  CK_NOTIFY Notify,
                                CK_SESSION_HANDLE_PTR phSession)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_CloseSession call on External Token
  * @note   Refer to @ref C_CloseSession function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_CloseSession(CK_SESSION_HANDLE hSession)
{
  return CKR_OK;
}


/**
  * @brief  This function is called upon @ref C_GetAttributeValue call on External Token
  * @note   Refer to @ref C_GetAttributeValue function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  hObject
  * @param  pTemplate
  * @param  ulCount
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE  hObject,
                                      CK_ATTRIBUTE_PTR  pTemplate, CK_ULONG  ulCount)
{
  return CKR_OK;
}


/**
  * @brief  This function is called upon @ref C_SetAttributeValue call on External Token
  * @note   Refer to @ref C_SetAttributeValue function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  hObject
  * @param  pTemplate
  * @param  ulCount
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE  hObject,
                                      CK_ATTRIBUTE_PTR  pTemplate, CK_ULONG          ulCount)
{
  return CKR_OK;
}


/**
  * @brief  This function is called upon @ref C_FindObjectsInit call on External Token
  * @note   Refer to @ref C_FindObjectsInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pTemplate
  * @param  ulCount
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                                    CK_ULONG ulCount)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_FindObjects call on External Token
  * @note   Refer to @ref C_FindObjects function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  phObject
  * @param  ulMaxObjectCount
  * @param  pulObjectCount
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
                                CK_ULONG ulMaxObjectCount,  CK_ULONG_PTR pulObjectCount)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_FindObjectsFinal call on External Token
  * @note   Refer to @ref C_FindObjectsFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_SignInit call on External Token
  * @note   Refer to @ref C_SignInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hKey
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR  pMechanism,
                             CK_OBJECT_HANDLE  hKey)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_Sign call on External Token
  * @note   Refer to @ref C_Sign function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pData
  * @param  ulDataLen
  * @param  pSignature
  * @param  pulSignatureLen
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_Sign(CK_SESSION_HANDLE     hSession, CK_BYTE_PTR pData,
                         CK_ULONG  ulDataLen, CK_BYTE_PTR  pSignature, CK_ULONG_PTR pulSignatureLen)
{
  return CKR_OK;
}


/**
  * @brief  This function is called upon @ref C_VerifyInit call on External Token
  * @note   Refer to @ref C_VerifyInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hKey
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_VerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR  pMechanism,
                               CK_OBJECT_HANDLE  hKey)

{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_Verify call on External Token
  * @note   Refer to @ref C_Verify function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pData
  * @param  ulDataLen
  * @param  pSignature
  * @param  ulSignatureLen
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_Verify(CK_SESSION_HANDLE     hSession,  CK_BYTE_PTR   pData,
                           CK_ULONG ulDataLen, CK_BYTE_PTR  pSignature,
                           CK_ULONG ulSignatureLen)
{
  return CKR_OK;
}


/**
  * @brief  This function is called upon @ref C_GenerateKeyPair call on External Token
  * @note   Refer to @ref C_GenerateKeyPair function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  pPublicKeyTemplate
  * @param  ulPublicKeyAttributeCount
  * @param  pPrivateKeyTemplate
  * @param  ulPrivateKeyAttributeCount
  * @param  phPublicKey
  * @param  phPrivateKey
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_GenerateKeyPair(CK_SESSION_HANDLE    hSession,
                                    CK_MECHANISM_PTR     pMechanism,
                                    CK_ATTRIBUTE_PTR     pPublicKeyTemplate,
                                    CK_ULONG             ulPublicKeyAttributeCount,
                                    CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,
                                    CK_ULONG             ulPrivateKeyAttributeCount,
                                    CK_OBJECT_HANDLE_PTR phPublicKey,
                                    CK_OBJECT_HANDLE_PTR phPrivateKey)
{
  return CKR_OK;
}



/**
  * @brief  This function is called upon @ref C_DeriveKey call on External Token
  * @note   Refer to @ref C_DeriveKey function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hBaseKey
  * @param  pTemplate
  * @param  ulAttributeCount
  * @param  phKey
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_DeriveKey(CK_SESSION_HANDLE  hSession, CK_MECHANISM_PTR  pMechanism,
                              CK_OBJECT_HANDLE   hBaseKey, CK_ATTRIBUTE_PTR     pTemplate,
                              CK_ULONG   ulAttributeCount, CK_OBJECT_HANDLE_PTR phKey)
{
  return CKR_OK;
}

/**
  * @brief  This function is called upon @ref C_GenerateRandom call on External Token
  * @note   Refer to @ref C_GenerateRandom function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pRandomData
  * @param  ulRandomLen
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR  pRandomData,
                                   CK_ULONG          ulRandomLen)
{
  return CKR_OK;
}

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @addtogroup KMS_EXTTOKEN_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  Return the function list supported by the external token
  * @param  ppFunctionList point to structure listing the functions
  * @retval Operation status
  */
CK_RV KMS_EXT_TOKEN_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList)
{
  CK_RV ck_rv_ret_status = CKR_OK;
  CK_FUNCTION_LIST_PTR pFunctionList = *ppFunctionList;

  pFunctionList->version.major = 2;
  pFunctionList->version.minor = 40;

  pFunctionList->C_Initialize = KMS_EXT_TOKEN_Initialize;
  pFunctionList->C_Finalize = KMS_EXT_TOKEN_Finalize;
  pFunctionList->C_GetInfo = NULL;
  pFunctionList->C_GetFunctionList = KMS_EXT_TOKEN_GetFunctionList;
  pFunctionList->C_GetSlotList = NULL;
  pFunctionList->C_GetSlotInfo = NULL;
  pFunctionList->C_GetTokenInfo = NULL;
  pFunctionList->C_GetMechanismList = NULL;
  pFunctionList->C_GetMechanismInfo = NULL;
  pFunctionList->C_InitToken = NULL;
  pFunctionList->C_InitPIN = NULL;
  pFunctionList->C_SetPIN = NULL;
  pFunctionList->C_OpenSession = KMS_EXT_TOKEN_OpenSession;
  pFunctionList->C_CloseSession = KMS_EXT_TOKEN_CloseSession;
  pFunctionList->C_CloseAllSessions = NULL;
  pFunctionList->C_GetSessionInfo = NULL;
  pFunctionList->C_GetOperationState = NULL;
  pFunctionList->C_SetOperationState = NULL;
  pFunctionList->C_Login = NULL;
  pFunctionList->C_Logout = NULL;
  pFunctionList->C_CreateObject = NULL;
  pFunctionList->C_CopyObject = NULL;
  pFunctionList->C_DestroyObject = NULL;
  pFunctionList->C_GetObjectSize = NULL;
  pFunctionList->C_GetAttributeValue = KMS_EXT_TOKEN_GetAttributeValue;
  pFunctionList->C_SetAttributeValue = KMS_EXT_TOKEN_SetAttributeValue;
  pFunctionList->C_FindObjectsInit = KMS_EXT_TOKEN_FindObjectsInit;
  pFunctionList->C_FindObjects = KMS_EXT_TOKEN_FindObjects;
  pFunctionList->C_FindObjectsFinal = KMS_EXT_TOKEN_FindObjectsFinal;
  pFunctionList->C_EncryptInit = NULL;
  pFunctionList->C_Encrypt = NULL;
  pFunctionList->C_EncryptUpdate = NULL;
  pFunctionList->C_EncryptFinal = NULL;
  pFunctionList->C_DecryptInit = NULL;
  pFunctionList->C_Decrypt = NULL;
  pFunctionList->C_DecryptUpdate = NULL;
  pFunctionList->C_DecryptFinal = NULL;
  pFunctionList->C_DigestInit = NULL;
  pFunctionList->C_Digest = NULL;
  pFunctionList->C_DigestUpdate = NULL;
  pFunctionList->C_DigestKey = NULL;
  pFunctionList->C_DigestFinal = NULL;
  pFunctionList->C_SignInit = KMS_EXT_TOKEN_SignInit;
  pFunctionList->C_Sign = KMS_EXT_TOKEN_Sign;
  pFunctionList->C_SignUpdate = NULL;
  pFunctionList->C_SignFinal = NULL;
  pFunctionList->C_SignRecoverInit = NULL;
  pFunctionList->C_SignRecover = NULL;
  pFunctionList->C_VerifyInit = KMS_EXT_TOKEN_VerifyInit;
  pFunctionList->C_Verify = KMS_EXT_TOKEN_Verify;
  pFunctionList->C_VerifyUpdate = NULL;
  pFunctionList->C_VerifyFinal = NULL;
  pFunctionList->C_VerifyFinal = NULL;
  pFunctionList->C_VerifyRecoverInit = NULL;
  pFunctionList->C_VerifyRecover = NULL;
  pFunctionList->C_DigestEncryptUpdate = NULL;
  pFunctionList->C_DecryptDigestUpdate = NULL;
  pFunctionList->C_SignEncryptUpdate = NULL;
  pFunctionList->C_DecryptVerifyUpdate = NULL;
  pFunctionList->C_GenerateKey = NULL;
  pFunctionList->C_GenerateKeyPair = KMS_EXT_TOKEN_GenerateKeyPair;
  pFunctionList->C_WrapKey = NULL;
  pFunctionList->C_UnwrapKey = NULL;
  pFunctionList->C_DeriveKey = KMS_EXT_TOKEN_DeriveKey;
  pFunctionList->C_SeedRandom = NULL;
  pFunctionList->C_GenerateRandom = KMS_EXT_TOKEN_GenerateRandom;
  pFunctionList->C_GetFunctionStatus = NULL;
  pFunctionList->C_CancelFunction = NULL;
  pFunctionList->C_WaitForSlotEvent = NULL;

  return ck_rv_ret_status;
}

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_EXT_TOKEN_ENABLED */

/**
  * @}
  */

