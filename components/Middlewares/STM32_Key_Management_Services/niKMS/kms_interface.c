/**
  ******************************************************************************
  * @file    kms_interface.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
  *          module access when called from the secure enclave
  *          or without any enclave
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
#include "kms.h"
#if defined(KMS_ENABLED)
#include <string.h>
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#include "kms_enc_dec.h"
#include "kms_digest.h"
#include "kms_dyn_obj.h"
#include "kms_init.h"
#include "kms_key_mgt.h"
#include "kms_sign_verify.h"
#include "kms_objects.h"
#else /* KMS_NIKMS_ROUTER_BYPASS */
#include "kms_entry.h"
#endif /* KMS_NIKMS_ROUTER_BYPASS */

#include "tkms.h"
#include "kms_interface.h"

/** @addtogroup tKMS User interface to Key Management Services (tKMS)
  * @{
  */

/** @addtogroup KMS_TKMS_ENTRY tKMS Entry Point Services access
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
static CK_RV KMS_EntryCallGate(KMS_FunctionID_t ulFctID, ...);
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/* Private function ----------------------------------------------------------*/

/**
  * @brief KMS_Entry call gate function.
  *        It is a variable argument function that makes it possible to build the
  *        va_list required by the function KMS_Entry.
  *    The tests are then embedded in the enclave.
  *    @param ulFctID Function ID
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
static CK_RV KMS_EntryCallGate(KMS_FunctionID_t ulFctID, ...)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;
  va_list arguments;

  /* Initialize arguments to store all values after ulFctID */
  va_start(arguments, ulFctID);

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_Entry(ulFctID, arguments);

  /* Clean up arguments list */
  va_end(arguments);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_TKMS_NI tKMS Non-Isolated access APIs (PKCS#11 Standard Compliant)
  * @{
  */

/**
  * @brief  This function is called upon tKMS @ref C_Initialize call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Initialize function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pInitArgs
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Initialize(CK_VOID_PTR pInitArgs)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_INITIALIZE_FCT_ID, pInitArgs);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_Finalize call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Finalize function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pReserved
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Finalize(CK_VOID_PTR pReserved)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_FINALIZE_FCT_ID, pReserved);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GetInfo call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetInfo function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pInfo
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GetInfo(CK_INFO_PTR   pInfo)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GET_INFO_FCT_ID, pInfo);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GetFunctionList call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetFunctionList function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  ppFunctionList
  * @retval Operation status
  */
CK_RV KMS_IF_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;
  CK_FUNCTION_LIST_PTR pFunctionList = *ppFunctionList;

  pFunctionList->version.major = 2;
  pFunctionList->version.minor = 40;

#if defined(KMS_NIKMS_ROUTER_BYPASS)
  pFunctionList->C_Initialize = KMS_Initialize;
  pFunctionList->C_Finalize = KMS_Finalize;
  pFunctionList->C_GetInfo = NULL;
  pFunctionList->C_GetFunctionList = NULL;
  pFunctionList->C_GetSlotList = NULL;
  pFunctionList->C_GetSlotInfo = NULL;
  pFunctionList->C_GetTokenInfo = KMS_GetTokenInfo;
  pFunctionList->C_GetMechanismList = NULL;
  pFunctionList->C_GetMechanismInfo = NULL;
  pFunctionList->C_InitToken = NULL;
  pFunctionList->C_InitPIN = NULL;
  pFunctionList->C_SetPIN = NULL;
  pFunctionList->C_OpenSession = KMS_OpenSession;
  pFunctionList->C_CloseSession = KMS_CloseSession;
  pFunctionList->C_CloseAllSessions = NULL;
  pFunctionList->C_GetSessionInfo = NULL;
  pFunctionList->C_GetOperationState = NULL;
  pFunctionList->C_SetOperationState = NULL;
  pFunctionList->C_Login = NULL;
  pFunctionList->C_Logout = NULL;
  pFunctionList->C_CreateObject = KMS_CreateObject;
  pFunctionList->C_CopyObject = NULL;
  pFunctionList->C_DestroyObject = KMS_DestroyObject;
  pFunctionList->C_GetObjectSize = NULL;
  pFunctionList->C_GetAttributeValue = KMS_GetAttributeValue;
  pFunctionList->C_SetAttributeValue = NULL;
  pFunctionList->C_FindObjectsInit = KMS_FindObjectsInit ;
  pFunctionList->C_FindObjects = KMS_FindObjects ;
  pFunctionList->C_FindObjectsFinal = KMS_FindObjectsFinal ;
  pFunctionList->C_EncryptInit = KMS_EncryptInit;
  pFunctionList->C_Encrypt = KMS_Encrypt;
  pFunctionList->C_EncryptUpdate = KMS_EncryptUpdate;
  pFunctionList->C_EncryptFinal = KMS_EncryptFinal;
  pFunctionList->C_DecryptInit = KMS_DecryptInit;
  pFunctionList->C_Decrypt = KMS_Decrypt;
  pFunctionList->C_DecryptUpdate = KMS_DecryptUpdate;
  pFunctionList->C_DecryptFinal = KMS_DecryptFinal;
  pFunctionList->C_DigestInit = KMS_DigestInit;
  pFunctionList->C_Digest = KMS_Digest;
  pFunctionList->C_DigestUpdate = KMS_DigestUpdate;
  pFunctionList->C_DigestKey = NULL;
  pFunctionList->C_DigestFinal = KMS_DigestFinal;
  pFunctionList->C_SignInit = KMS_SignInit;
  pFunctionList->C_Sign = KMS_Sign;
  pFunctionList->C_SignUpdate = NULL;
  pFunctionList->C_SignFinal = NULL;
  pFunctionList->C_SignRecoverInit = NULL;
  pFunctionList->C_SignRecover = NULL;
  pFunctionList->C_VerifyInit = KMS_VerifyInit;
  pFunctionList->C_Verify = KMS_Verify;
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
  pFunctionList->C_GenerateKeyPair = KMS_GenerateKeyPair;
  pFunctionList->C_WrapKey = NULL;
  pFunctionList->C_UnwrapKey = NULL;
  pFunctionList->C_DeriveKey = KMS_DeriveKey;
  pFunctionList->C_SeedRandom = NULL;
  pFunctionList->C_GenerateRandom = NULL;
  pFunctionList->C_GetFunctionStatus = NULL;
  pFunctionList->C_CancelFunction = NULL;
  pFunctionList->C_WaitForSlotEvent = NULL;
#else /* KMS_NIKMS_ROUTER_BYPASS */
  pFunctionList->C_Initialize = KMS_IF_Initialize;
  pFunctionList->C_Finalize = KMS_IF_Finalize;
  pFunctionList->C_GetInfo = KMS_IF_GetInfo;
  pFunctionList->C_GetFunctionList = KMS_IF_GetFunctionList;
  pFunctionList->C_GetSlotList = KMS_IF_GetSlotList;
  pFunctionList->C_GetSlotInfo = NULL;
  pFunctionList->C_GetTokenInfo = KMS_IF_GetTokenInfo;
  pFunctionList->C_GetMechanismList = NULL;
  pFunctionList->C_GetMechanismInfo = NULL;
  pFunctionList->C_InitToken = NULL;
  pFunctionList->C_InitPIN = NULL;
  pFunctionList->C_SetPIN = NULL;
  pFunctionList->C_OpenSession = KMS_IF_OpenSession;
  pFunctionList->C_CloseSession = KMS_IF_CloseSession;
  pFunctionList->C_CloseAllSessions = NULL;
  pFunctionList->C_GetSessionInfo = NULL;
  pFunctionList->C_GetOperationState = NULL;
  pFunctionList->C_SetOperationState = NULL;
  pFunctionList->C_Login = NULL;
  pFunctionList->C_Logout = NULL;
  pFunctionList->C_CreateObject = KMS_IF_CreateObject;
  pFunctionList->C_CopyObject = NULL;
  pFunctionList->C_DestroyObject = KMS_IF_DestroyObject;
  pFunctionList->C_GetObjectSize = NULL;
  pFunctionList->C_GetAttributeValue = KMS_IF_GetAttributeValue;
  pFunctionList->C_SetAttributeValue = KMS_IF_SetAttributeValue;
  pFunctionList->C_FindObjectsInit = KMS_IF_FindObjectsInit ;
  pFunctionList->C_FindObjects = KMS_IF_FindObjects ;
  pFunctionList->C_FindObjectsFinal = KMS_IF_FindObjectsFinal ;
  pFunctionList->C_EncryptInit = KMS_IF_EncryptInit;
  pFunctionList->C_Encrypt = KMS_IF_Encrypt;
  pFunctionList->C_EncryptUpdate = KMS_IF_EncryptUpdate;
  pFunctionList->C_EncryptFinal = KMS_IF_EncryptFinal;
  pFunctionList->C_DecryptInit = KMS_IF_DecryptInit;
  pFunctionList->C_Decrypt = KMS_IF_Decrypt;
  pFunctionList->C_DecryptUpdate = KMS_IF_DecryptUpdate;
  pFunctionList->C_DecryptFinal = KMS_IF_DecryptFinal;
  pFunctionList->C_DigestInit = KMS_IF_DigestInit;
  pFunctionList->C_Digest = KMS_IF_Digest;
  pFunctionList->C_DigestUpdate = KMS_IF_DigestUpdate;
  pFunctionList->C_DigestKey = NULL;
  pFunctionList->C_DigestFinal = KMS_IF_DigestFinal;
  pFunctionList->C_SignInit = KMS_IF_SignInit;
  pFunctionList->C_Sign = KMS_IF_Sign;
  pFunctionList->C_SignUpdate = NULL;
  pFunctionList->C_SignFinal = NULL;
  pFunctionList->C_SignRecoverInit = NULL;
  pFunctionList->C_SignRecover = NULL;
  pFunctionList->C_VerifyInit = KMS_IF_VerifyInit;
  pFunctionList->C_Verify = KMS_IF_Verify;
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
  pFunctionList->C_GenerateKeyPair = KMS_IF_GenerateKeyPair;
  pFunctionList->C_WrapKey = NULL;
  pFunctionList->C_UnwrapKey = NULL;
  pFunctionList->C_DeriveKey = KMS_IF_DeriveKey;
  pFunctionList->C_SeedRandom = NULL;
  pFunctionList->C_GenerateRandom = KMS_IF_GenerateRandom;
  pFunctionList->C_GetFunctionStatus = NULL;
  pFunctionList->C_CancelFunction = NULL;
  pFunctionList->C_WaitForSlotEvent = NULL;
#endif /* KMS_NIKMS_ROUTER_BYPASS */

  return ck_rv_ret_status;
}

/**
  * @brief  This function is called upon tKMS @ref C_GetSlotList call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetSlotList function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  tokenPresent
  * @param  pSlotList
  * @param  pulCount
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList,
                         CK_ULONG_PTR pulCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GET_SLOT_LIST_FCT_ID,
                                       tokenPresent, pSlotList, pulCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GetSlotInfo call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetSlotInfo function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID
  * @param  pInfo
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GET_SLOT_INFO_FCT_ID, slotID, pInfo);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GetTokenInfo call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetTokenInfo function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID
  * @param  pInfo
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GET_TOKEN_INFO_FCT_ID, slotID, pInfo);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GetMechanismInfo call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetMechanismInfo function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID
  * @param  type
  * @param  pInfo
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR pInfo)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GET_MECHANISM_INFO_FCT_ID, slotID, type, pInfo);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_OpenSession call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_OpenSession function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID
  * @param  flags
  * @param  pApplication
  * @param  Notify
  * @param  phSession
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
                         CK_VOID_PTR pApplication, CK_NOTIFY Notify,
                         CK_SESSION_HANDLE_PTR phSession)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_OPEN_SESSION_FCT_ID,
                                       slotID, flags, pApplication, Notify, phSession);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_CloseSession call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_CloseSession function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_CloseSession(CK_SESSION_HANDLE hSession)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_CLOSE_SESSION_FCT_ID, hSession);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_CreateObject call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_CreateObject function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pTemplate
  * @param  ulCount
  * @param  phObject
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_CreateObject(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_CREATE_OBJECT_FCT_ID,
                                       hSession, pTemplate, ulCount, phObject);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DestroyObject call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DestroyObject function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  hObject
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DestroyObject(CK_SESSION_HANDLE hSession,
                           CK_OBJECT_HANDLE hObject)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DESTROY_OBJECT_FCT_ID, hSession, hObject);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GetAttributeValue call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GetAttributeValue function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  hObject
  * @param  pTemplate
  * @param  ulCount
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
                               CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GET_ATTRIBUTE_VALUE_FCT_ID,
                                       hSession, hObject, pTemplate, ulCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_SetAttributeValue call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_SetAttributeValue function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  hObject
  * @param  pTemplate
  * @param  ulCount
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
                               CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_SET_ATTRIBUTE_VALUE_FCT_ID,
                                       hSession, hObject, pTemplate, ulCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_FindObjectsInit call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_FindObjectsInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pTemplate
  * @param  ulCount
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                             CK_ULONG ulCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_FIND_OBJECTS_INIT_FCT_ID,
                                       hSession, pTemplate, ulCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_FindObjects call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_FindObjects function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  phObject
  * @param  ulMaxObjectCount
  * @param  pulObjectCount
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
                         CK_ULONG ulMaxObjectCount,  CK_ULONG_PTR pulObjectCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_FIND_OBJECTS_FCT_ID,
                                       hSession, phObject, ulMaxObjectCount, pulObjectCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_FindObjectsFinal call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_FindObjectsFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_FIND_OBJECTS_FINAL_FCT_ID, hSession);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_EncryptInit call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_EncryptInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hKey
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_EncryptInit(CK_SESSION_HANDLE hSession,
                         CK_MECHANISM_PTR  pMechanism, CK_OBJECT_HANDLE  hKey)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_ENCRYPT_INIT_FCT_ID,
                                       hSession, pMechanism, hKey);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_Encrypt call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Encrypt function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pData
  * @param  ulDataLen
  * @param  pEncryptedData
  * @param  pulEncryptedDataLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                     CK_ULONG  ulDataLen, CK_BYTE_PTR  pEncryptedData,
                     CK_ULONG_PTR      pulEncryptedDataLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_ENCRYPT_FCT_ID,
                                       hSession, pData, ulDataLen, pEncryptedData,
                                       pulEncryptedDataLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_EncryptUpdate call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_EncryptUpdate function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pPart
  * @param  ulPartLen
  * @param  pEncryptedPart
  * @param  pulEncryptedPartLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_EncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
                           CK_ULONG          ulPartLen,
                           CK_BYTE_PTR       pEncryptedPart,
                           CK_ULONG_PTR      pulEncryptedPartLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_ENCRYPT_UPDATE_FCT_ID,
                                       hSession, pPart, ulPartLen, pEncryptedPart,
                                       pulEncryptedPartLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_EncryptFinal call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_EncryptFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pLastEncryptedPart
  * @param  pulLastEncryptedPartLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_EncryptFinal(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR       pLastEncryptedPart,
                          CK_ULONG_PTR      pulLastEncryptedPartLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_ENCRYPT_FINAL_FCT_ID,
                                       hSession, pLastEncryptedPart, pulLastEncryptedPartLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DecryptInit call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DecryptInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hKey
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DecryptInit(CK_SESSION_HANDLE hSession,
                         CK_MECHANISM_PTR  pMechanism,
                         CK_OBJECT_HANDLE  hKey)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DECRYPT_INIT_FCT_ID,
                                       hSession, pMechanism, hKey);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_Decrypt call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Decrypt function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pEncryptedData
  * @param  ulEncryptedDataLen
  * @param  pData
  * @param  pulDataLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Decrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedData,
                     CK_ULONG  ulEncryptedDataLen, CK_BYTE_PTR  pData,
                     CK_ULONG_PTR pulDataLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DECRYPT_FCT_ID,
                                       hSession, pEncryptedData, ulEncryptedDataLen,
                                       pData, pulDataLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DecryptUpdate call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DecryptUpdate function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pEncryptedPart
  * @param  ulEncryptedPartLen
  * @param  pPart
  * @param  pulPartLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DecryptUpdate(CK_SESSION_HANDLE hSession,
                           CK_BYTE_PTR       pEncryptedPart,
                           CK_ULONG          ulEncryptedPartLen,
                           CK_BYTE_PTR       pPart,
                           CK_ULONG_PTR      pulPartLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DECRYPT_UPDATE_FCT_ID,
                                       hSession, pEncryptedPart, ulEncryptedPartLen,
                                       pPart, pulPartLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DecryptFinal call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DecryptFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pLastPart
  * @param  pulLastPartLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DecryptFinal(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR       pLastPart,
                          CK_ULONG_PTR      pulLastPartLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DECRYPT_FINAL_FCT_ID,
                                       hSession, pLastPart, pulLastPartLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DigestInit call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DigestInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DIGEST_INIT_FCT_ID,
                                       hSession, pMechanism);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_Digest call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Digest function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pData
  * @param  ulDataLen
  * @param  pDigest
  * @param  pulDigestLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Digest(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                    CK_ULONG ulDataLen, CK_BYTE_PTR pDigest,
                    CK_ULONG_PTR pulDigestLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DIGEST_FCT_ID,
                                       hSession, pData, ulDataLen, pDigest, pulDigestLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DigestUpdate call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DigestUpdate function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pPart
  * @param  ulPartLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DIGEST_UPDATE_FCT_ID,
                                       hSession, pPart, ulPartLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DigestFinal call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_DigestFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pDigest
  * @param  pulDigestLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
                         CK_ULONG_PTR pulDigestLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DIGEST_FINAL_FCT_ID,
                                       hSession, pDigest, pulDigestLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_SignInit call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_SignInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hKey
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_SignInit(CK_SESSION_HANDLE hSession,
                      CK_MECHANISM_PTR  pMechanism,
                      CK_OBJECT_HANDLE  hKey)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_SIGN_INIT_FCT_ID,
                                       hSession, pMechanism, hKey);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_Sign call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Sign function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pData
  * @param  ulDataLen
  * @param  pSignature
  * @param  pulSignatureLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                  CK_ULONG  ulDataLen, CK_BYTE_PTR  pSignature,
                  CK_ULONG_PTR pulSignatureLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_SIGN_FCT_ID,
                                       hSession, pData, ulDataLen,
                                       pSignature, pulSignatureLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_VerifyInit call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_VerifyInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pMechanism
  * @param  hKey
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_VerifyInit(CK_SESSION_HANDLE hSession,
                        CK_MECHANISM_PTR  pMechanism,
                        CK_OBJECT_HANDLE  hKey)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_VERIFY_INIT_FCT_ID,
                                       hSession, pMechanism, hKey);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_Verify call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_Verify function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pData
  * @param  ulDataLen
  * @param  pSignature
  * @param  ulSignatureLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                    CK_ULONG  ulDataLen, CK_BYTE_PTR  pSignature,
                    CK_ULONG  ulSignatureLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_VERIFY_FCT_ID,
                                       hSession, pData, ulDataLen,
                                       pSignature, ulSignatureLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_DeriveKey call
  *         to call appropriate KMS service through Secure Engine Firewall
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
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_DeriveKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                       CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR  pTemplate,
                       CK_ULONG  ulAttributeCount, CK_OBJECT_HANDLE_PTR  phKey)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_DERIVE_KEY_FCT_ID,
                                       hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GenerateKeyPair call
  *         to call appropriate KMS service through Secure Engine Firewall
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
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GenerateKeyPair(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                             CK_ATTRIBUTE_PTR pPublicKeyTemplate, CK_ULONG  ulPublicKeyAttributeCount,
                             CK_ATTRIBUTE_PTR pPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount,
                             CK_OBJECT_HANDLE_PTR phPublicKey, CK_OBJECT_HANDLE_PTR phPrivateKey)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GENERATE_KEYPAIR_FCT_ID, hSession,  pMechanism,
                                       pPublicKeyTemplate,   ulPublicKeyAttributeCount,  pPrivateKeyTemplate,
                                       ulPrivateKeyAttributeCount, phPublicKey,  phPrivateKey);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called upon tKMS @ref C_GenerateRandom call
  *         to call appropriate KMS service through Secure Engine Firewall
  * @note   Refer to @ref C_GenerateRandom function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession
  * @param  pRandomData
  * @param  ulRandomLen
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR  pRandomData,
                            CK_ULONG  ulRandomLen)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_GENERATE_RANDOM_FCT_ID,
                                       hSession, pRandomData, ulRandomLen);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called to call KMS service that will authenticate,
  *         verify and decrypt a blob to update NVM static ID keys
  * @param  pHdr is the pointer to the encrypted blob header
  * @param  pFlash is the pointer to the blob location in flash
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_ImportBlob(CK_BYTE_PTR pHdr, CK_BYTE_PTR pFlash)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_IMPORT_BLOB_FCT_ID,
                                       pHdr, pFlash);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called to call KMS service that will lock the specified keys
  * @param  pKeys Pointer to key handles to be locked
  * @param  ulCount Number of keys to lock
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_LockKeys(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_LOCK_KEYS_FCT_ID,
                                       pKeys, ulCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  This function is called to call KMS service that will lock the specified services
  * @param  pServices Pointer to services function identifier to be locked
  * @param  ulCount Number of services to lock
  * @retval Operation status
  */
#if !defined(KMS_NIKMS_ROUTER_BYPASS)
CK_RV KMS_IF_LockServices(CK_ULONG_PTR pServices, CK_ULONG ulCount)
{
  CK_RV ck_rv_ret_status = CKR_FUNCTION_FAILED;

  /* KMS_Entry call */
  ck_rv_ret_status = KMS_EntryCallGate(tKMS_GetCluster() | KMS_LOCK_SERVICES_FCT_ID,
                                       pServices, ulCount);

  return ck_rv_ret_status;
}
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
