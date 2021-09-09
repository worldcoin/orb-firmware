/**
  ******************************************************************************
  * @file    kms_entry.c
  * @author  MCD Application Team
  * @brief   This file contains implementation for the entry point of Key Management
  *          Services module functionalities.
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
#include "kms_entry.h"

#include "kms_init.h"                   /* KMS session services */
#include "kms_enc_dec.h"                /* KMS encryption & decryption services */
#include "kms_digest.h"                 /* KMS digest services */
#include "kms_dyn_obj.h"                /* KMS dynamic objects services */
#include "kms_sign_verify.h"            /* KMS signature & verification services */
#include "kms_key_mgt.h"                /* KMS key management services */
#include "kms_objects.h"                /* KMS objects services */
#include "kms_low_level.h"
#ifdef KMS_EXT_TOKEN_ENABLED
#include "kms_ext_token.h"              /* KMS external token services */
#endif /* KMS_EXT_TOKEN_ENABLED */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_ENTRY Entry Point
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/** @addtogroup KMS_ENTRY_Private_Variables Private Variables
  * @{
  */

#ifdef KMS_EXT_TOKEN_ENABLED
/**
  * @brief External token functions list
  */
static CK_FUNCTION_LIST ExtToken_FunctionList;
/**
  * @brief Pointer to @ref ExtToken_FunctionList
  */
static CK_FUNCTION_LIST_PTR pExtToken_FunctionList = &ExtToken_FunctionList;

#endif /* KMS_EXT_TOKEN_ENABLED */

/**
  * @}
  */

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup KMS_ENTRY_Private_Functions Private Functions
  * @{
  */
#ifdef KMS_SE_CHECK_PARAMS
static CK_RV KMS_Entry_CheckMechanismContent(CK_MECHANISM_PTR  pMechanism);
#endif /* KMS_SE_CHECK_PARAMS */

/* Private function ----------------------------------------------------------*/

/**
  * @brief  Generic mechanism contents parsing function
  * @note   Depending on specified mechanism, the mechanism data are interpreted
  *         and verified versus secure enclave checks
  * @param  pMechanism Mechanism structure to analyze
  * @retval CKR_OK
  *         CKR_MECHANISM_INVALID
  */
#ifdef KMS_SE_CHECK_PARAMS
static CK_RV KMS_Entry_CheckMechanismContent(CK_MECHANISM_PTR  pMechanism)
{
  CK_RV status = CKR_MECHANISM_INVALID;
  if (pMechanism != NULL_PTR)
  {
    switch (pMechanism->mechanism)
    {
      case CKM_AES_CCM:
      {
        CK_CCM_PARAMS *pParam = (CK_CCM_PARAMS *)(pMechanism->pParameter);
        if (pParam != NULL_PTR)
        {
          KMS_LL_IsBufferInSecureEnclave((void *)(pParam->pNonce), pParam->ulNonceLen * sizeof(CK_BYTE));
          KMS_LL_IsBufferInSecureEnclave((void *)(pParam->pAAD), pParam->ulAADLen * sizeof(CK_BYTE));
        }
        status = CKR_OK;
        break;
      }
      case CKM_AES_GCM:
      {
        CK_GCM_PARAMS *pParam = (CK_GCM_PARAMS *)(pMechanism->pParameter);
        if (pParam != NULL_PTR)
        {
          KMS_LL_IsBufferInSecureEnclave((void *)(pParam->pIv), pParam->ulIvLen * sizeof(CK_BYTE));
          KMS_LL_IsBufferInSecureEnclave((void *)(pParam->pAAD), pParam->ulAADLen * sizeof(CK_BYTE));
        }
        status = CKR_OK;
        break;
      }
      case CKM_ECDH1_DERIVE:
      {
        CK_ECDH1_DERIVE_PARAMS *pParam = (CK_ECDH1_DERIVE_PARAMS *)(pMechanism->pParameter);
        if (pParam != NULL_PTR)
        {
          KMS_LL_IsBufferInSecureEnclave((void *)(pParam->pSharedData), pParam->ulSharedDataLen * sizeof(CK_BYTE));
          KMS_LL_IsBufferInSecureEnclave((void *)(pParam->pPublicData), pParam->ulPublicDataLen * sizeof(CK_BYTE));
        }
        status = CKR_OK;
        break;
      }
      case CKM_SHA_1:
      case CKM_SHA256:
      case CKM_AES_CBC:
      case CKM_AES_ECB:
      case CKM_AES_ECB_ENCRYPT_DATA:
      case CKM_AES_CMAC_GENERAL:
      case CKM_AES_CMAC:
      case CKM_EC_KEY_PAIR_GEN:
      case CKM_RSA_PKCS:
      case CKM_SHA1_RSA_PKCS:
      case CKM_SHA256_RSA_PKCS:
      case CKM_ECDSA:
      case CKM_ECDSA_SHA1:
      case CKM_ECDSA_SHA256:
        status = CKR_OK;
        break;
      default:
        status = CKR_MECHANISM_INVALID;
        break;
    }
  }
  return status;
}
#endif /* KMS_SE_CHECK_PARAMS */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_ENTRY_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  Entry point function
  * @param  eID KMS function ID
  * @param  arguments function ID dependent arguments
  * @return Operation status
  * @retval Any PKCS11 CK_RV values
  */
CK_RV KMS_Entry(KMS_FunctionID_t eID, va_list arguments)
{

  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
#ifdef KMS_EXT_TOKEN_ENABLED
  CK_RV e_ret_ext_status = CKR_FUNCTION_FAILED;
#endif /* KMS_EXT_TOKEN_ENABLED */

  /* Check that the Function ID match */
  if (((eID & (~KMS_CLUST_MASK)) < KMS_FIRST_ID) && ((eID & (~KMS_CLUST_MASK)) >= KMS_LAST_ID_CHECK))
  {
    return CKR_FUNCTION_FAILED;
  }

  /* Check that the Function ID is not locked */
  if (KMS_CheckServiceFctIdIsNotLocked(eID & (~KMS_CLUST_MASK)) != CKR_OK)
  {
    return CKR_FUNCTION_FAILED;
  }
  /* Double Check to avoid basic fault injection : Check that the Function ID is not locked */
  if (KMS_CheckServiceFctIdIsNotLocked(eID & (~KMS_CLUST_MASK)) != CKR_OK)
  {
    return CKR_FUNCTION_FAILED;
  }

  /**
    * @brief Provides import and storage of a single client certificate and
    * associated private key.
    */
  switch (eID & (~KMS_CLUST_MASK))
  {
    case KMS_INITIALIZE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_Initialize )( CK_VOID_PTR pInitArgs ) */
      CK_VOID_PTR *pInitArgs;
      pInitArgs = va_arg(arguments, void *);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* pInitArgs and its contents if not NULL_PTR is read-only. No check needed */
#endif /* KMS_SE_CHECK_PARAMS */

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_Initialize pC_Initialize;

        /* check if a Token is plugged in */

        /* Read the FunctionList supported by the Token */
        KMS_EXT_TOKEN_GetFunctionList(&pExtToken_FunctionList);

        /* If the Token support Initialize() function, call it */
        pC_Initialize = pExtToken_FunctionList->C_Initialize;

        if (pC_Initialize != NULL)
        {
          /* Call the C_Initialize function in the library */
          e_ret_ext_status = (*pC_Initialize)(pInitArgs);

          /* Call also the Initialize of the KMS */
          e_ret_status = KMS_Initialize(pInitArgs);

          /* Handle error on Ext_Token but not on KMS, to ensure error is */
          /* returned whether issue happens on KMS or Ext_Token           */
          if (e_ret_status == CKR_OK)
          {
            e_ret_status = e_ret_ext_status;
          }
        }
        else
        {
          /* Call also the Initialize of the KMS */
          e_ret_status = KMS_Initialize(pInitArgs);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_Initialize(pInitArgs);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }

    case KMS_FINALIZE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_Finalize )( CK_VOID_PTR pReserved ) */
      CK_VOID_PTR pReserved;
      pReserved = va_arg(arguments, CK_VOID_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* pReserved is only checked to be NULL_PTR, no check needed */
      /*KMS_LL_IsBufferInSecureEnclave(, );*/
#endif /* KMS_SE_CHECK_PARAMS */

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_Finalize pC_Finalize;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_Finalize != NULL))
        {
          /* If the Token support Finalize() function, call it */
          pC_Finalize = pExtToken_FunctionList -> C_Finalize;

          /* Call the C_Finalize function in the library */
          e_ret_ext_status = (*pC_Finalize)(pReserved);

          /* Call also the Finalize the KMS */
          e_ret_status = KMS_Finalize(pReserved);

          /* Handle error on Ext_Token but not on KMS, to ensure error is */
          /* returned whether issue happens on KMS or Ext_Token           */
          if (e_ret_status == CKR_OK)
          {
            e_ret_status = e_ret_ext_status;
          }
        }
        else
        {
          /* Call also the Finalize the KMS */
          e_ret_status = KMS_Finalize(pReserved);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_Finalize(pReserved);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }

#if defined(KMS_PKCS11_COMPLIANCE)
    case KMS_GET_INFO_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_GetInfo)(CK_INFO_PTR pInfo); */
      CK_INFO_PTR pInfo;
      pInfo = va_arg(arguments, CK_INFO_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pInfo, sizeof(CK_INFO));
#endif /* KMS_SE_CHECK_PARAMS */

      /* C_GetInfo returns general information about Cryptoki        */
      /* pInfo points to the location that receives the information  */
      if (pInfo != NULL_PTR)
      {
        pInfo->cryptokiVersion.major = CRYPTOKI_VERSION_MAJOR;
        pInfo->cryptokiVersion.minor = CRYPTOKI_VERSION_MINOR;
        (void)memcpy(&pInfo->manufacturerID[0], "ST Microelectronics             ", 32);
        pInfo->flags = 0;                         /* The spec say: MUST be Zero */
#ifdef KMS_EXT_TOKEN_ENABLED
        (void)memcpy(&pInfo->libraryDescription[0], "KMS-EXT-TOKEN                     ", 32);
#else /* KMS_EXT_TOKEN_ENABLED */
        (void)memcpy(&pInfo->libraryDescription[0], "KMS                               ", 32);
#endif /* KMS_EXT_TOKEN_ENABLED */

        pInfo->libraryVersion.minor = 0;
        pInfo->libraryVersion.major = 0;

        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_HOST_MEMORY;
      }
      break;
    }
#endif /* KMS_PKCS11_COMPLIANCE */

#if defined(KMS_PKCS11_COMPLIANCE)
    case KMS_GET_SLOT_LIST_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_GetSlotList)(CK_BBOOL tokenPresent,
                                                    CK_SLOT_ID_PTR pSlotList,
                                                    CK_ULONG_PTR pulCount);
        */
      CK_SLOT_ID_PTR pSlotList;
      CK_ULONG_PTR pulCount;

      /* tokenPresent parameter dropped has not used                                                                */
      /* 'int' used instead of 'CK_BBOOL' cause of va_arg promoting parameter smaller than an integer to an integer */
      (void)va_arg(arguments, int);
      pSlotList = va_arg(arguments, CK_SLOT_ID_PTR);
      pulCount = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pSlotList, sizeof(CK_SLOT_ID));
      KMS_LL_IsBufferInSecureEnclave((void *)pulCount, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */
      /* C_GetSlotList is used to obtain a list of slots in the system. tokenPresent
          indicates whether the list obtained includes only those slots with a token
          present (CK_TRUE), or all slots (CK_FALSE); pulCount points to the location
          that receives the number of slots.

        There are two ways for an application to call C_GetSlotList:
          1. If pSlotList is NULL_PTR, then all that C_GetSlotList does is return
            (in *pulCount) the number of slots, without actually returning a list of
            slots. The contents of the buffer pointed to by pulCount on entry to
            C_GetSlotList has no meaning in this case, and the call returns the value CKR_OK.
          2. If pSlotList is not NULL_PTR, then *pulCount MUST contain the size (in
            terms of CK_SLOT_ID elements) of the buffer pointed to by pSlotList.
            If that buffer is large enough to hold the list of slots, then the list
            is returned in it, and CKR_OK is returned. If not, then the call to
            C_GetSlotList returns the value CKR_BUFFER_TOO_SMALL. In either case,
            the value *pulCount is set to hold the number of slots.
        */

      if (pSlotList == NULL_PTR)
      {
        if (pulCount != NULL_PTR)
        {
          *pulCount = 1;
          e_ret_status = CKR_OK;
        }
        else
        {
          e_ret_status = CKR_HOST_MEMORY;
        }
      }
      else  /* pSlotList is not NULL */
      {
        if (pulCount == NULL_PTR)
        {
          e_ret_status = CKR_HOST_MEMORY;
        }
        else if (*pulCount >= 1UL)
        {
          pSlotList[0] = 0;
          e_ret_status = CKR_OK;

          *pulCount = 1;
        }
        else
        {
          e_ret_status = CKR_BUFFER_TOO_SMALL;
        }
      }
      break;
    }
#endif /* KMS_PKCS11_COMPLIANCE */

#if defined(KMS_PKCS11_COMPLIANCE)
    case KMS_GET_SLOT_INFO_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_GetSlotInfo)(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo); */
      CK_SLOT_ID slotID;
      CK_SLOT_INFO_PTR pInfo;

      slotID = va_arg(arguments, CK_SLOT_ID);
      pInfo = va_arg(arguments, CK_SLOT_INFO_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pInfo, sizeof(CK_SLOT_INFO));
#endif /* KMS_SE_CHECK_PARAMS */

      if (pInfo == NULL_PTR)
      {
        e_ret_status = CKR_HOST_MEMORY;
      }
      else if (slotID == 0UL)
      {
#ifdef KMS_EXT_TOKEN_ENABLED
        (void)memcpy(&pInfo->slotDescription[0],
                     "KMS FOR STM32 -WITH EXT-TOKEN                                   ",
                     64);
#else /* KMS_EXT_TOKEN_ENABLED */
        (void)memcpy(&pInfo->slotDescription[0],
                     "KMS FOR STM32                                                   ",
                     64);
#endif /* KMS_EXT_TOKEN_ENABLED */

        (void)memcpy(&pInfo->manufacturerID[0], "ST Microelectronics             ", 32);
        pInfo->flags = CKF_TOKEN_PRESENT;
        pInfo->hardwareVersion.minor = 0;
        pInfo->hardwareVersion.major = 0;
        pInfo->firmwareVersion.minor = 0;
        pInfo->firmwareVersion.major = 0;

        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_SLOT_ID_INVALID;
      }
      break;
    }
#endif /* KMS_PKCS11_COMPLIANCE */

#if defined(KMS_PKCS11_COMPLIANCE)
    case KMS_GET_TOKEN_INFO_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_GetTokenInfo)(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo); */
      CK_SLOT_ID        slotID;   /* ID of the token's slot */
      CK_TOKEN_INFO_PTR pInfo;    /* receives the token information */

      slotID      = va_arg(arguments, CK_SLOT_ID);
      pInfo       = va_arg(arguments, CK_TOKEN_INFO_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pInfo, sizeof(CK_TOKEN_INFO));
#endif /* KMS_SE_CHECK_PARAMS */

      e_ret_status = KMS_GetTokenInfo(slotID, pInfo);
      break;
    }
#endif /* KMS_PKCS11_COMPLIANCE */

#if defined(KMS_SEARCH) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_GET_MECHANISM_INFO_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_GetMechanismInfo)(CK_SLOT_ID slotID,
                                                        CK_MECHANISM_TYPE type,
                                                        CK_MECHANISM_INFO_PTR pInfo); */
      CK_SLOT_ID             slotID;  /* ID of the token's slot */
      CK_MECHANISM_TYPE      type;    /* type of mechanism */
      CK_MECHANISM_INFO_PTR  pInfo;   /* receives the mechanism  information */

      slotID      = va_arg(arguments, CK_SLOT_ID);
      type        = va_arg(arguments, CK_MECHANISM_TYPE);
      pInfo       = va_arg(arguments, CK_MECHANISM_INFO_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pInfo, sizeof(CK_MECHANISM_INFO));
#endif /* KMS_SE_CHECK_PARAMS */

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_GetMechanismInfo != NULL))
        {
          e_ret_status = pExtToken_FunctionList->C_GetMechanismInfo(slotID, type, pInfo);
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */

      if (e_ret_status != CKR_OK)
      {
        e_ret_status = KMS_GetMechanismInfo(slotID, type, pInfo);
      }
      break;
    }
#endif /* KMS_SEARCH || KMS_EXT_TOKEN_ENABLED*/

    case KMS_OPEN_SESSION_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_OpenSession)(CK_SLOT_ID slotID,
                                                   CK_FLAGS flags,
                                                   CK_VOID_PTR pApplication,
                                                   CK_NOTIFY Notify,
                                                   CK_SESSION_HANDLE_PTR phSession); */
      CK_SLOT_ID slotID;
      CK_FLAGS flags;
      CK_VOID_PTR pApplication;
      CK_NOTIFY Notify;
      CK_SESSION_HANDLE_PTR phSession;

      slotID = va_arg(arguments, uint32_t);
      flags = va_arg(arguments, uint32_t);
      pApplication = va_arg(arguments, void *);
      Notify = va_arg(arguments, CK_NOTIFY);
      phSession = va_arg(arguments, CK_SESSION_HANDLE_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pApplication, sizeof(CK_VOID_PTR));
      KMS_LL_IsBufferInSecureEnclave((void *)Notify, sizeof(CK_NOTIFY));
      KMS_LL_IsBufferInSecureEnclave((void *)phSession, sizeof(CK_SESSION_HANDLE));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }

      /* Call the C_OpenSession function of the middleware */
      e_ret_status = KMS_OpenSession(slotID,  flags, pApplication, Notify, phSession);

#ifdef KMS_EXT_TOKEN_ENABLED
      if (e_ret_status == CKR_OK)
      {
        CK_C_OpenSession pC_OpenSession;
        CK_SESSION_HANDLE hSession_ExtToken;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_OpenSession != NULL))
        {
          /* If the Token support OpenSession() function, call it */
          pC_OpenSession = pExtToken_FunctionList->C_OpenSession;

          /* Call the C_OpenSession function of the External Token  */
          /* if a Callback for Notify is to be handled, it should pass through KMS  */
          /* So we have to register the Notify to call KMS notif  */
          /* Then this notif function would forward to the application */
          if (Notify != NULL)
          {
            e_ret_status = (*pC_OpenSession)(slotID,
                                             flags,
                                             pApplication,
                                             KMS_CallbackFunction_ForExtToken,
                                             &hSession_ExtToken);
            if (e_ret_status == CKR_OK)
            {
              KMS_OpenSession_RegisterExtToken(*phSession, hSession_ExtToken);
            }
          }
          else
          {
            e_ret_status = (*pC_OpenSession)(slotID,  flags, NULL, NULL, &hSession_ExtToken);
            if (e_ret_status == CKR_OK)
            {
              KMS_OpenSession_RegisterExtToken(*phSession, hSession_ExtToken);
            }
          }
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }

    case KMS_CLOSE_SESSION_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_CloseSession )( CK_SESSION_HANDLE_PTR phSession ) */
      CK_SESSION_HANDLE hSession;

      hSession = va_arg(arguments, CK_SESSION_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* No parameter to check */
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_CloseSession pC_CloseSession;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_CloseSession != NULL))
        {
          /* If the Token support CloseSession() function, call it */
          pC_CloseSession = pExtToken_FunctionList -> C_CloseSession;

          /* Call the C_CloseSession function in the library */
          e_ret_ext_status = (*pC_CloseSession)(hSession);

          /* Call also the CloseSession the KMS */
          e_ret_status = KMS_CloseSession(hSession);

          /* Handle error on Ext_Token but not on KMS, to ensure error is */
          /* returned whether issue happens on KMS or Ext_Token           */
          if (e_ret_status == CKR_OK)
          {
            e_ret_status = e_ret_ext_status;
          }
        }
        else
        {
          /* Call also the CloseSession the KMS */
          e_ret_status = KMS_CloseSession(hSession);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */

      e_ret_status = KMS_CloseSession(hSession);

#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }

#if defined(KMS_OBJECTS) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_CREATE_OBJECT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_CreateObject )( CK_SESSION_HANDLE hSession,
                                          CK_ATTRIBUTE_PTR pTemplate,
                                          CK_ULONG ulCount,
                                          CK_OBJECT_HANDLE_PTR phObject ) */
      CK_SESSION_HANDLE hSession;
      CK_ATTRIBUTE_PTR pTemplate;
      CK_ULONG ulCount;
      CK_OBJECT_HANDLE_PTR phObject;

      hSession = va_arg(arguments, CK_SESSION_HANDLE);
      pTemplate = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulCount = va_arg(arguments, CK_ULONG);
      phObject = va_arg(arguments, CK_OBJECT_HANDLE_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      for (CK_ULONG i = 0; i < ulCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pTemplate[i].pValue), pTemplate[i].ulValueLen);
      }
      KMS_LL_IsBufferInSecureEnclave((void *)phObject, sizeof(CK_OBJECT_HANDLE));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_CreateObject pC_CreateObject;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_CreateObject != NULL))
        {
          /* If the Token support CreateObject() function, call it */
          pC_CreateObject = pExtToken_FunctionList -> C_CreateObject;

          /* Call the C_CreateObject function in the library */
          e_ret_status = (*pC_CreateObject)(hSession, pTemplate, ulCount, phObject);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_CreateObject(hSession, pTemplate, ulCount, phObject);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }
#endif /* KMS_OBJECTS || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_OBJECTS) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_DESTROY_OBJECT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_DestroyObject )( CK_SESSION_HANDLE hSession,
                                         CK_OBJECT_HANDLE hObject )         */
      CK_SESSION_HANDLE hSession;
      CK_OBJECT_HANDLE hObject;
      kms_obj_range_t  object_range;

      hSession = va_arg(arguments, CK_SESSION_HANDLE);
      hObject = va_arg(arguments, CK_OBJECT_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* Nothing to check */
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(hObject);

      /* Only NVM / VM dynamic IDs are destroyable */
      if ((object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_DestroyObject(hSession, hObject);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        /* The object is in EXT TOKEN Range */
        CK_C_DestroyObject pC_DestroyObject;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_DestroyObject != NULL))
        {
          /* If the Token support DestroyObject() function, call it */
          pC_DestroyObject = pExtToken_FunctionList -> C_DestroyObject;

          /* Call the C_DestroyObject function in the library */
          e_ret_status = (*pC_DestroyObject)(hSession, hObject);
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_OBJECTS || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_ATTRIBUTES) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_GET_ATTRIBUTE_VALUE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_GetAttributeValue)(CK_SESSION_HANDLE hSession,
                                                         CK_OBJECT_HANDLE hObject,
                                                         CK_ATTRIBUTE_PTR pTemplate,
                                                         CK_ULONG ulCount); */
      CK_SESSION_HANDLE hSession;   /* the session's handle */
      CK_OBJECT_HANDLE  hObject;    /* the object's handle */
      CK_ATTRIBUTE_PTR  pTemplate;  /* specifies attrs; gets vals */
      CK_ULONG          ulCount;    /* attributes in template */
      kms_obj_range_t   object_range;

      hSession       = va_arg(arguments, CK_SESSION_HANDLE);
      hObject        = va_arg(arguments, CK_OBJECT_HANDLE);
      pTemplate      = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulCount        = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      for (CK_ULONG i = 0; i < ulCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pTemplate[i].pValue), pTemplate[i].ulValueLen);
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(hObject);

      if ((object_range == KMS_OBJECT_RANGE_EMBEDDED) ||
          (object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_GetAttributeValue(hSession, hObject, pTemplate, ulCount);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        /* The object is in EXT TOKEN Range */
        CK_C_GetAttributeValue pC_GetAttributeValue;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_GetAttributeValue != NULL))
        {
          /* If the Token support GetAttribute() function, call it */
          pC_GetAttributeValue = pExtToken_FunctionList -> C_GetAttributeValue;

          /* Call the C_GetAttribute function of the External Token */
          e_ret_status = (*pC_GetAttributeValue)(hSession, hObject, pTemplate, ulCount);
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_ATTRIBUTES || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_ATTRIBUTES) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_SET_ATTRIBUTE_VALUE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_SetAttributeValue)(CK_SESSION_HANDLE hSession,
                                                         CK_OBJECT_HANDLE hObject,
                                                         CK_ATTRIBUTE_PTR pTemplate,
                                                         CK_ULONG ulCount); */
      CK_SESSION_HANDLE hSession;   /* the session's handle */
      CK_OBJECT_HANDLE  hObject;    /* the object's handle */
      CK_ATTRIBUTE_PTR  pTemplate;  /* specifies attrs and values */
      CK_ULONG          ulCount;     /* attributes in template */
      kms_obj_range_t   object_range;

      hSession       = va_arg(arguments, CK_SESSION_HANDLE);
      hObject        = va_arg(arguments, CK_OBJECT_HANDLE);
      pTemplate      = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulCount        = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      for (CK_ULONG i = 0; i < ulCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pTemplate[i].pValue), pTemplate[i].ulValueLen);
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(hObject);

      /* Only NVM objects allows modification */
      if ((object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        (void)pTemplate;
        (void)ulCount;
        e_ret_status = CKR_FUNCTION_NOT_SUPPORTED;
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        CK_C_SetAttributeValue pC_SetAttributeValue;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_SetAttributeValue != NULL))
        {
          /* If the Token support SetAttributeValue() function, call it */
          pC_SetAttributeValue = pExtToken_FunctionList -> C_SetAttributeValue;

          /* Call the C_SetAttributeValue function of the External Token */
          e_ret_status = (*pC_SetAttributeValue)(hSession, hObject, pTemplate, ulCount);
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_ATTRIBUTES || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_SEARCH) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_FIND_OBJECTS_INIT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_FindObjectsInit )( CK_SESSION_HANDLE hSession,
                                          CK_ATTRIBUTE_PTR pTemplate,
                                          CK_ULONG ulCount ) */
      CK_SESSION_HANDLE hSession;
      CK_ATTRIBUTE_PTR pTemplate;
      CK_ULONG ulCount;

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pTemplate      = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulCount         = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      for (CK_ULONG i = 0; i < ulCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pTemplate[i].pValue), pTemplate[i].ulValueLen);
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_FindObjectsInit pC_FindObjectsInit;
        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_FindObjectsInit != NULL))
        {
          /* If the Token support FindObjectsInit() function, call it */
          pC_FindObjectsInit = pExtToken_FunctionList -> C_FindObjectsInit;

          /* Call the C_FindObjectsInit function in the library */
          e_ret_status = (*pC_FindObjectsInit)(hSession, pTemplate, ulCount);

        }
        else
        {
          e_ret_status = KMS_FindObjectsInit(hSession, pTemplate, ulCount);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_FindObjectsInit(hSession, pTemplate, ulCount);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }
#endif /* KMS_SEARCH || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_SEARCH) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_FIND_OBJECTS_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_FindObjects )( CK_SESSION_HANDLE hSession,
                                        CK_OBJECT_HANDLE_PTR phObject,
                                        CK_ULONG ulMaxObjectCount,
                                        CK_ULONG_PTR pulObjectCount ) */
      CK_SESSION_HANDLE hSession;
      CK_OBJECT_HANDLE_PTR phObject;
      CK_ULONG ulMaxObjectCount;
      CK_ULONG_PTR pulObjectCount;

      hSession          = va_arg(arguments, CK_SESSION_HANDLE);
      phObject          = va_arg(arguments, CK_OBJECT_HANDLE_PTR);
      ulMaxObjectCount  = va_arg(arguments, CK_ULONG);
      pulObjectCount    = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)phObject, sizeof(CK_OBJECT_HANDLE)*ulMaxObjectCount);
      KMS_LL_IsBufferInSecureEnclave((void *)pulObjectCount, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_FindObjects pC_FindObjects;
        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_FindObjects != NULL))
        {
          /* If the Token support FindObjects() function, call it */
          pC_FindObjects = pExtToken_FunctionList -> C_FindObjects;

          /* Call the C_FindObjects function in the library */
          e_ret_status = (*pC_FindObjects)(hSession, phObject, ulMaxObjectCount, pulObjectCount);

        }
        else
        {
          e_ret_status = KMS_FindObjects(hSession, phObject, ulMaxObjectCount, pulObjectCount);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_FindObjects(hSession, phObject, ulMaxObjectCount, pulObjectCount);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }
#endif /* KMS_SEARCH || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_SEARCH) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_FIND_OBJECTS_FINAL_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION( CK_RV, C_FindObjectsFinal )( CK_SESSION_HANDLE hSession ) */
      CK_SESSION_HANDLE hSession;

      hSession          = va_arg(arguments, CK_SESSION_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* Nothing to check */
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_FindObjectsFinal pC_FindObjectsFinal;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_FindObjectsFinal != NULL))
        {
          /* If the Token support FindObjectsFinal() function, call it */
          pC_FindObjectsFinal = pExtToken_FunctionList -> C_FindObjectsFinal;

          /* Call the C_FindObjectsFinal function in the library */
          e_ret_status = (*pC_FindObjectsFinal)(hSession);

        }
        else
        {
          e_ret_status = KMS_FindObjectsFinal(hSession);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_FindObjectsFinal(hSession);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }
#endif /* KMS_SEARCH || KMS_EXT_TOKEN_ENABLED*/

#if defined(KMS_ENCRYPT)
    case KMS_ENCRYPT_INIT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_EncryptInit)(CK_SESSION_HANDLE hSession,
                                                   CK_MECHANISM_PTR pMechanism,
                                                   CK_OBJECT_HANDLE hKey); */
      CK_SESSION_HANDLE hSession;    /* the session's handle */
      CK_MECHANISM_PTR  pMechanism;  /* the encryption mechanism */
      CK_OBJECT_HANDLE  hKey;        /* handle of encryption key */

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism      = va_arg(arguments, CK_MECHANISM_PTR);
      hKey            = va_arg(arguments, CK_OBJECT_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }
      if (KMS_Objects_GetRange(hKey) == KMS_OBJECT_RANGE_UNKNOWN)
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_EncryptInit(hSession, pMechanism, hKey);
      break;
    }
#endif /* KMS_ENCRYPT */

#if defined(KMS_ENCRYPT)
    case KMS_ENCRYPT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_Encrypt)(CK_SESSION_HANDLE hSession,
                                               CK_BYTE_PTR pData,
                                               CK_ULONG ulDataLen,
                                               CK_BYTE_PTR pEncryptedData,
                                               CK_ULONG_PTR pulEncryptedDataLen); */
      CK_SESSION_HANDLE hSession;            /* session's handle */
      CK_BYTE_PTR       pData;               /* the plaintext data */
      CK_ULONG          ulDataLen;           /* bytes of plaintext */
      CK_BYTE_PTR       pEncryptedData;      /* gets ciphertext */
      CK_ULONG_PTR      pulEncryptedDataLen; /* gets c-text size */

      hSession              = va_arg(arguments, CK_SESSION_HANDLE);
      pData                 = va_arg(arguments, CK_BYTE_PTR);
      ulDataLen             = va_arg(arguments, CK_ULONG);
      pEncryptedData        = va_arg(arguments, CK_BYTE_PTR);
      pulEncryptedDataLen   = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pData, ulDataLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pEncryptedData, ulDataLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pulEncryptedDataLen, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_Encrypt(hSession, pData, ulDataLen, pEncryptedData, pulEncryptedDataLen);
      break;
    }
#endif /* KMS_ENCRYPT */

#if defined(KMS_ENCRYPT)
    case KMS_ENCRYPT_UPDATE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_EncryptUpdate)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR pPart,
                                                     CK_ULONG ulPartLen,
                                                     CK_BYTE_PTR pEncryptedPart,
                                                     CK_ULONG_PTR pulEncryptedPartLen); */
      CK_SESSION_HANDLE hSession;           /* session's handle */
      CK_BYTE_PTR       pPart;              /* the plaintext data */
      CK_ULONG          ulPartLen;          /* plaintext data len */
      CK_BYTE_PTR       pEncryptedPart;     /* gets ciphertext */
      CK_ULONG_PTR      pulEncryptedPartLen;/* gets c-text size */

      hSession              = va_arg(arguments, CK_SESSION_HANDLE);
      pPart                 = va_arg(arguments, CK_BYTE_PTR);
      ulPartLen             = va_arg(arguments, CK_ULONG);
      pEncryptedPart        = va_arg(arguments, CK_BYTE_PTR);
      pulEncryptedPartLen   = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pPart, ulPartLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pEncryptedPart, ulPartLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pulEncryptedPartLen, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_EncryptUpdate(hSession, pPart, ulPartLen, pEncryptedPart, pulEncryptedPartLen);
      break;
    }
#endif /* KMS_ENCRYPT */

#if defined(KMS_ENCRYPT)
    case KMS_ENCRYPT_FINAL_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_EncryptFinal)(CK_SESSION_HANDLE hSession,
                                                    CK_BYTE_PTR pLastEncryptedPart,
                                                    CK_ULONG_PTR pulLastEncryptedPartLen); */
      CK_SESSION_HANDLE hSession;                /* session handle */
      CK_BYTE_PTR       pLastEncryptedPart;      /* last c-text */
      CK_ULONG_PTR      pulLastEncryptedPartLen; /* gets last size */

      hSession                      = va_arg(arguments, CK_SESSION_HANDLE);
      pLastEncryptedPart            = va_arg(arguments, CK_BYTE_PTR);
      pulLastEncryptedPartLen       = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      if (pulLastEncryptedPartLen != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)pulLastEncryptedPartLen, sizeof(CK_ULONG));
        KMS_LL_IsBufferInSecureEnclave((void *)pLastEncryptedPart, (*pulLastEncryptedPartLen)*sizeof(CK_BYTE));
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_EncryptFinal(hSession, pLastEncryptedPart, pulLastEncryptedPartLen);
      break;
    }
#endif /* KMS_ENCRYPT */

#if defined(KMS_DECRYPT)
    case KMS_DECRYPT_INIT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DecryptInit)(CK_SESSION_HANDLE hSession,
                                                   CK_MECHANISM_PTR pMechanism,
                                                   CK_OBJECT_HANDLE hKey); */
      CK_SESSION_HANDLE hSession;    /* the session's handle */
      CK_MECHANISM_PTR  pMechanism;  /* the decryption mechanism */
      CK_OBJECT_HANDLE  hKey;        /* handle of decryption key */

      hSession      = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism    = va_arg(arguments, CK_MECHANISM_PTR);
      hKey          = va_arg(arguments, CK_OBJECT_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }
      if (KMS_Objects_GetRange(hKey) == KMS_OBJECT_RANGE_UNKNOWN)
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_DecryptInit(hSession, pMechanism, hKey);
      break;
    }
#endif /* KMS_DECRYPT */

#if defined(KMS_DECRYPT)
    case KMS_DECRYPT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_Encrypt)(CK_SESSION_HANDLE hSession,
                                               CK_BYTE_PTR pEncryptedData,
                                               CK_ULONG ulEncryptedDataLen,
                                               CK_BYTE_PTR pData,
                                               CK_ULONG_PTR pulDataLen); */
      CK_SESSION_HANDLE hSession;           /* session's handle */
      CK_BYTE_PTR       pEncryptedData;     /* ciphertext */
      CK_ULONG          ulEncryptedDataLen; /* ciphertext length */
      CK_BYTE_PTR       pData;              /* gets plaintext */
      CK_ULONG_PTR      pulDataLen;         /* gets p-text size */

      hSession              = va_arg(arguments, CK_SESSION_HANDLE);
      pEncryptedData        = va_arg(arguments, CK_BYTE_PTR);
      ulEncryptedDataLen    = va_arg(arguments, CK_ULONG);
      pData                 = va_arg(arguments, CK_BYTE_PTR);
      pulDataLen            = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pEncryptedData, ulEncryptedDataLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pData, ulEncryptedDataLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pulDataLen, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_Decrypt(hSession, pEncryptedData, ulEncryptedDataLen,
                                 pData, pulDataLen);
      break;
    }
#endif /* KMS_DECRYPT */

#if defined(KMS_DECRYPT)
    case KMS_DECRYPT_UPDATE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DecryptUpdate)(CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR pEncryptedPart,
                                                     CK_ULONG ulEncryptedPartLen,
                                                     CK_BYTE_PTR pPart,
                                                     CK_ULONG_PTR pulPartLen); */
      CK_SESSION_HANDLE hSession;            /* session's handle */
      CK_BYTE_PTR       pEncryptedPart;      /* encrypted data */
      CK_ULONG          ulEncryptedPartLen;  /* input length */
      CK_BYTE_PTR       pPart;               /* gets plaintext */
      CK_ULONG_PTR      pulPartLen;          /* p-text size */

      hSession              = va_arg(arguments, CK_SESSION_HANDLE);
      pEncryptedPart        = va_arg(arguments, CK_BYTE_PTR);
      ulEncryptedPartLen    = va_arg(arguments, CK_ULONG);
      pPart                 = va_arg(arguments, CK_BYTE_PTR);
      pulPartLen            = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pEncryptedPart, ulEncryptedPartLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pPart, ulEncryptedPartLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pulPartLen, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_DecryptUpdate(hSession, pEncryptedPart, ulEncryptedPartLen, pPart, pulPartLen);
      break;
    }
#endif /* KMS_DECRYPT */

#if defined(KMS_DECRYPT)
    case KMS_DECRYPT_FINAL_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DecryptFinal)(CK_SESSION_HANDLE hSession,
                                                    CK_BYTE_PTR pLastPart,
                                                    CK_ULONG_PTR pulLastPartLen); */
      CK_SESSION_HANDLE hSession;       /* the session's handle */
      CK_BYTE_PTR       pLastPart;      /* gets plaintext */
      CK_ULONG_PTR      pulLastPartLen; /* p-text size */

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pLastPart       = va_arg(arguments, CK_BYTE_PTR);
      pulLastPartLen  = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      if (pulLastPartLen != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)pulLastPartLen, sizeof(CK_ULONG));
        KMS_LL_IsBufferInSecureEnclave((void *)pLastPart, (*pulLastPartLen)*sizeof(CK_BYTE));
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_DecryptFinal(hSession, pLastPart, pulLastPartLen);
      break;
    }
#endif /* KMS_DECRYPT */

#if defined(KMS_DIGEST)
    case KMS_DIGEST_INIT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DigestInit)( CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism); */
      CK_SESSION_HANDLE hSession;       /* the session's handle */
      CK_MECHANISM_PTR pMechanism;      /* the digest mechanism */

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism       = va_arg(arguments, CK_MECHANISM_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_DigestInit(hSession, pMechanism);
      break;
    }
#endif /* KMS_DIGEST */

#if defined(KMS_DIGEST)
    case KMS_DIGEST_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_Digest)( CK_SESSION_HANDLE hSession,
                                               CK_BYTE_PTR pData,
                                               CK_ULONG ulDataLen,
                                               CK_BYTE_PTR pDigest,
                                               CK_ULONG_PTR pulDigestLen); */
      CK_SESSION_HANDLE hSession;       /* the session's handle */
      CK_BYTE_PTR pData;                /* the data */
      CK_ULONG ulDataLen;               /* the length of the data */
      CK_BYTE_PTR pDigest;              /* the message digest */
      CK_ULONG_PTR pulDigestLen;        /* the length of the message digest */

      hSession      = va_arg(arguments, CK_SESSION_HANDLE);
      pData         = va_arg(arguments, CK_BYTE_PTR);
      ulDataLen     = va_arg(arguments, CK_ULONG);
      pDigest       = va_arg(arguments, CK_BYTE_PTR);
      pulDigestLen  = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      if (pulDigestLen != NULL_PTR)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)pulDigestLen, sizeof(CK_ULONG));
        KMS_LL_IsBufferInSecureEnclave((void *)pDigest, (*pulDigestLen)*sizeof(CK_BYTE));
      }
      KMS_LL_IsBufferInSecureEnclave((void *)pData, ulDataLen * sizeof(CK_BYTE));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_Digest(hSession, pData, ulDataLen, pDigest, pulDigestLen);
      break;
    }
#endif /* KMS_DIGEST */

#if defined(KMS_DIGEST)
    case KMS_DIGEST_UPDATE_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DigestUpdate)( CK_SESSION_HANDLE hSession,
                                                     CK_BYTE_PTR pPart,
                                                     CK_ULONG ulPartLen); */
      CK_SESSION_HANDLE hSession;       /* the session's handle */
      CK_BYTE_PTR pPart;                /* to the data part */
      CK_ULONG ulPartLen;               /* the length of the data part */

      hSession      = va_arg(arguments, CK_SESSION_HANDLE);
      pPart         = va_arg(arguments, CK_BYTE_PTR);
      ulPartLen     = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pPart, ulPartLen * sizeof(CK_BYTE));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_DigestUpdate(hSession, pPart, ulPartLen);
      break;
    }
#endif /* KMS_DIGEST */

#if defined(KMS_DIGEST)
    case KMS_DIGEST_FINAL_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DigestFinal)( CK_SESSION_HANDLE hSession,
                                                    CK_BYTE_PTR pDigest,
                                                    CK_ULONG_PTR pulDigestLen); */
      CK_SESSION_HANDLE hSession;       /* the session's handle */
      CK_BYTE_PTR pDigest;              /* the message digest */
      CK_ULONG_PTR pulDigestLen;        /* the length of the message digest */

      hSession      = va_arg(arguments, CK_SESSION_HANDLE);
      pDigest       = va_arg(arguments, CK_BYTE_PTR);
      pulDigestLen  = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      if (pulDigestLen != NULL_PTR)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)pulDigestLen, sizeof(CK_ULONG));
        KMS_LL_IsBufferInSecureEnclave((void *)pDigest, (*pulDigestLen)*sizeof(CK_BYTE));
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      e_ret_status = KMS_DigestFinal(hSession, pDigest, pulDigestLen);
      break;
    }
#endif /* KMS_DIGEST */

#if defined(KMS_SIGN) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_SIGN_INIT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_SignInit)( CK_SESSION_HANDLE hSession,
                                                 CK_MECHANISM_PTR pMechanism,
                                                 CK_OBJECT_HANDLE hKey); */
      CK_SESSION_HANDLE hSession;    /* the session's handle */
      CK_MECHANISM_PTR  pMechanism;  /* the decryption mechanism */
      CK_OBJECT_HANDLE  hKey;        /* handle of key */
      kms_obj_range_t   object_range;

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism      = va_arg(arguments, CK_MECHANISM_PTR);
      hKey            = va_arg(arguments, CK_OBJECT_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(hKey);

      if ((object_range == KMS_OBJECT_RANGE_EMBEDDED) ||
          (object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_SignInit(hSession, pMechanism, hKey);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        CK_C_SignInit pC_SignInit;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_SignInit != NULL) &&
            /* Check also KMS session state cause will be used to store key */
            (KMS_GETSESSION(hSession).state == KMS_SESSION_IDLE))
        {
          /* If the Token support SignInit() function, call it */
          pC_SignInit = pExtToken_FunctionList -> C_SignInit;

          /* Call the C_SignInit function in the library */
          e_ret_status = (*pC_SignInit)(hSession, pMechanism, hKey);

          /* Store the key handle, to use it in the next calls */
          KMS_GETSESSION(hSession).hKey = hKey;

          /* Update state to reserve this session */
          KMS_GETSESSION(hSession).state = KMS_SESSION_SIGN;
        }
        else
        {
          e_ret_status = CKR_OBJECT_HANDLE_INVALID;
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_SIGN || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_SIGN) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_SIGN_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_Sign)(CK_SESSION_HANDLE hSession,
                                            CK_BYTE_PTR pData,
                                            CK_ULONG ulDataLen,
                                            CK_BYTE_PTR pSignature,
                                            CK_ULONG_PTR pulSignatureLen); */
      CK_SESSION_HANDLE     hSession;    /* the session's handle */
      CK_BYTE_PTR           pData;
      CK_ULONG              ulDataLen;
      CK_BYTE_PTR           pSignature;
      CK_ULONG_PTR          pulSignatureLen;
      kms_obj_range_t  object_range;

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pData           = va_arg(arguments, CK_BYTE_PTR);
      ulDataLen       = va_arg(arguments, CK_ULONG);
      pSignature      = va_arg(arguments, CK_BYTE_PTR);
      pulSignatureLen = va_arg(arguments, CK_ULONG_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pData, ulDataLen * sizeof(CK_BYTE));
      if (pulSignatureLen != NULL_PTR)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)pSignature, (*pulSignatureLen)*sizeof(CK_BYTE));
      }
      KMS_LL_IsBufferInSecureEnclave((void *)pulSignatureLen, sizeof(CK_ULONG));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(KMS_GETSESSION(hSession).hKey);

      if ((object_range == KMS_OBJECT_RANGE_EMBEDDED) ||
          (object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_Sign(hSession, pData, ulDataLen, pSignature, pulSignatureLen);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        CK_C_Sign pC_Sign;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_Sign != NULL) &&
            (KMS_GETSESSION(hSession).state == KMS_SESSION_SIGN))
        {
          /* If the Token support Sign() function, call it */
          pC_Sign = pExtToken_FunctionList -> C_Sign;

          /* Call the C_Sign function in the library */
          e_ret_status = (*pC_Sign)(hSession, pData, ulDataLen, pSignature, pulSignatureLen);

          if (!((e_ret_status == CKR_BUFFER_TOO_SMALL) || ((e_ret_status == CKR_OK) && (pSignature == NULL))))
          {
            /* A call to C_Sign always terminates the active signing       */
            /* operation unless it returns CKR_BUFFER_TOO_SMALL or is a    */
            /* successful call (i.e., one which returns CKR_OK) to         */
            /* determine the length of the buffer needed to hold the       */
            /* signature                                                   */
            KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;

            /* Reset state to release this session */
            KMS_GETSESSION(hSession).state = KMS_SESSION_IDLE;
          }
        }
        else
        {
          e_ret_status = CKR_OBJECT_HANDLE_INVALID;
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_SIGN || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_VERIFY) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_VERIFY_INIT_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_VerifyInit)(CK_SESSION_HANDLE hSession,
                                                  CK_MECHANISM_PTR pMechanism,
                                                  CK_OBJECT_HANDLE hKey); */
      CK_SESSION_HANDLE hSession;    /* the session's handle */
      CK_MECHANISM_PTR  pMechanism;  /* the decryption mechanism */
      CK_OBJECT_HANDLE  hKey;        /* handle of key */
      kms_obj_range_t   object_range;

      hSession        = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism      = va_arg(arguments, CK_MECHANISM_PTR);
      hKey            = va_arg(arguments, CK_OBJECT_HANDLE);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(hKey);

      if ((object_range == KMS_OBJECT_RANGE_EMBEDDED) ||
          (object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_VerifyInit(hSession, pMechanism, hKey);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        CK_C_VerifyInit pC_VerifyInit;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_VerifyInit != NULL) &&
            /* Check also KMS session state cause will be used to store key */
            (KMS_GETSESSION(hSession).state == KMS_SESSION_IDLE))
        {
          /* If the Token support VerifyInit() function, call it */
          pC_VerifyInit = pExtToken_FunctionList -> C_VerifyInit;

          /* Call the C_VerifyInit function in the library */
          e_ret_status = (*pC_VerifyInit)(hSession, pMechanism, hKey);

          /* Store the key handle, to use it in the next calls */
          KMS_GETSESSION(hSession).hKey = hKey;

          /* Update state to reserve this session */
          KMS_GETSESSION(hSession).state = KMS_SESSION_VERIFY;
        }
        else
        {
          e_ret_status = CKR_OBJECT_HANDLE_INVALID;
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_VERIFY || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_VERIFY) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_VERIFY_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_Verify)(CK_SESSION_HANDLE hSession,
                                              CK_BYTE_PTR pData,
                                              CK_ULONG ulDataLen,
                                              CK_BYTE_PTR pSignature,
                                              CK_ULONG ulSignatureLen); */
      CK_SESSION_HANDLE     hSession;    /* the session's handle */
      CK_BYTE_PTR           pData;
      CK_ULONG              ulDataLen;
      CK_BYTE_PTR           pSignature;
      CK_ULONG              ulSignatureLen;
      kms_obj_range_t       object_range;

      hSession       = va_arg(arguments, CK_SESSION_HANDLE);
      pData          = va_arg(arguments, CK_BYTE_PTR);
      ulDataLen      = va_arg(arguments, CK_ULONG);
      pSignature     = va_arg(arguments, CK_BYTE_PTR);
      ulSignatureLen = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pData, ulDataLen * sizeof(CK_BYTE));
      KMS_LL_IsBufferInSecureEnclave((void *)pSignature, ulSignatureLen * sizeof(CK_BYTE));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(KMS_GETSESSION(hSession).hKey);

      if ((object_range == KMS_OBJECT_RANGE_EMBEDDED) ||
          (object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_Verify(hSession, pData, ulDataLen, pSignature, ulSignatureLen);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        CK_C_Verify pC_Verify;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_Verify != NULL) &&
            (KMS_GETSESSION(hSession).state == KMS_SESSION_VERIFY))
        {
          /* If the Token support Verify() function, call it */
          pC_Verify = pExtToken_FunctionList -> C_Verify;

          /* Call the C_Verify function in the library */
          e_ret_status = (*pC_Verify)(hSession, pData, ulDataLen, pSignature, ulSignatureLen);

          /* As soon as the Verify has been called once, the operation is considered finished */
          KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;

          /* Reset state to release this session */
          KMS_GETSESSION(hSession).state = KMS_SESSION_IDLE;
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_VERIFY || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_DERIVE_KEY) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_DERIVE_KEY_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_DeriveKey)(CK_SESSION_HANDLE hSession,
                                                 CK_MECHANISM_PTR pMechanism,
                                                 CK_OBJECT_HANDLE hBaseKey,
                                                 CK_ATTRIBUTE_PTR pTemplate,
                                                 CK_ULONG ulAttributeCount,
                                                 CK_OBJECT_HANDLE_PTR phKey); */
      CK_SESSION_HANDLE    hSession;          /* session's handle */
      CK_MECHANISM_PTR     pMechanism;        /* key deriv. mech  */
      CK_OBJECT_HANDLE     hBaseKey;          /* base key */
      CK_ATTRIBUTE_PTR     pTemplate;         /* new key template */
      CK_ULONG             ulAttributeCount;  /* template length */
      CK_OBJECT_HANDLE_PTR phKey;             /* gets new handle */
      kms_obj_range_t      object_range;

      hSession          = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism        = va_arg(arguments, CK_MECHANISM_PTR);
      hBaseKey          = va_arg(arguments, CK_OBJECT_HANDLE);
      pTemplate         = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulAttributeCount  = va_arg(arguments, CK_ULONG);
      phKey             = va_arg(arguments, CK_OBJECT_HANDLE_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
      for (CK_ULONG i = 0; i < ulAttributeCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pTemplate[i].pValue), pTemplate[i].ulValueLen);
      }
      KMS_LL_IsBufferInSecureEnclave((void *)phKey, sizeof(void *));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

      object_range = KMS_Objects_GetRange(hBaseKey);

      if ((object_range == KMS_OBJECT_RANGE_EMBEDDED) ||
          (object_range == KMS_OBJECT_RANGE_NVM_STATIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_NVM_DYNAMIC_ID) ||
          (object_range == KMS_OBJECT_RANGE_VM_DYNAMIC_ID))
      {
        e_ret_status = KMS_DeriveKey(hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey);
      }
#ifdef KMS_EXT_TOKEN_ENABLED
      else if ((object_range == KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID) ||
               (object_range == KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID))
      {
        CK_C_DeriveKey pC_DeriveKey;

        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_DeriveKey != NULL))
        {
          /* If the Token support DeriveKey() function, call it */
          pC_DeriveKey = pExtToken_FunctionList -> C_DeriveKey;

          /* Call the C_DeriveKey function in the library */
          e_ret_status = (*pC_DeriveKey)(hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey);
        }
      }
#endif /* KMS_EXT_TOKEN_ENABLED */
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_DERIVE_KEY || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_GENERATE_KEYS) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_GENERATE_KEYPAIR_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV, C_GenerateKeyPair)(CK_SESSION_HANDLE hSession,
                                                       CK_MECHANISM_PTR pMechanism,
                                                       CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                                                       CK_ULONG ulPublicKeyAttributeCount,
                                                       CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                                                       CK_ULONG ulPrivateKeyAttributeCount,
                                                       CK_OBJECT_HANDLE_PTR phPublicKey,
                                                       CK_OBJECT_HANDLE_PTR phPrivateKey); */
      CK_SESSION_HANDLE    hSession;                    /* session handle */
      CK_MECHANISM_PTR     pMechanism;                  /* key-gen mech  */
      CK_ATTRIBUTE_PTR     pPublicKeyTemplate;          /* template for pub. key */
      CK_ULONG             ulPublicKeyAttributeCount;   /* # pub. attrs  */
      CK_ATTRIBUTE_PTR     pPrivateKeyTemplate;         /* template for priv. key */
      CK_ULONG             ulPrivateKeyAttributeCount;  /* # priv.  attrs  */
      CK_OBJECT_HANDLE_PTR phPublicKey;                 /* gets pub. key handle */
      CK_OBJECT_HANDLE_PTR phPrivateKey;                /* gets priv. key handle */

      hSession                   = va_arg(arguments, CK_SESSION_HANDLE);
      pMechanism                 = va_arg(arguments, CK_MECHANISM_PTR);
      pPublicKeyTemplate         = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulPublicKeyAttributeCount  = va_arg(arguments, CK_ULONG);
      pPrivateKeyTemplate        = va_arg(arguments, CK_ATTRIBUTE_PTR);
      ulPrivateKeyAttributeCount = va_arg(arguments, CK_ULONG);
      phPublicKey                = va_arg(arguments, CK_OBJECT_HANDLE_PTR);
      phPrivateKey               = va_arg(arguments, CK_OBJECT_HANDLE_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pMechanism, sizeof(CK_MECHANISM));
      if (pMechanism != NULL)
      {
        KMS_LL_IsBufferInSecureEnclave((void *)(pMechanism->pParameter), pMechanism->ulParameterLen * sizeof(CK_BYTE));
        if (KMS_Entry_CheckMechanismContent(pMechanism) != CKR_OK)
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
      for (CK_ULONG i = 0; i < ulPublicKeyAttributeCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pPublicKeyTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pPublicKeyTemplate[i].pValue), pPublicKeyTemplate[i].ulValueLen);
      }
      for (CK_ULONG i = 0; i < ulPrivateKeyAttributeCount; i++)
      {
        KMS_LL_IsBufferInSecureEnclave((void *) & (pPrivateKeyTemplate[i]), sizeof(CK_ATTRIBUTE));
        KMS_LL_IsBufferInSecureEnclave((void *)(pPrivateKeyTemplate[i].pValue), pPrivateKeyTemplate[i].ulValueLen);
      }
      KMS_LL_IsBufferInSecureEnclave((void *)phPublicKey, sizeof(CK_OBJECT_HANDLE));
      KMS_LL_IsBufferInSecureEnclave((void *)phPrivateKey, sizeof(CK_OBJECT_HANDLE));
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_GenerateKeyPair pC_GenerateKeyPair;
        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_GenerateKeyPair != NULL))
        {
          /* If the Token support FindObjectsFinal() function, call it */
          pC_GenerateKeyPair = pExtToken_FunctionList -> C_GenerateKeyPair;

          /* Call the C_GenerateKeyPair function in the library */
          e_ret_status = (*pC_GenerateKeyPair)(hSession, pMechanism, pPublicKeyTemplate,
                                               ulPublicKeyAttributeCount, pPrivateKeyTemplate,
                                               ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey);

        }
        else
        {
          e_ret_status = KMS_GenerateKeyPair(hSession, pMechanism, pPublicKeyTemplate,
                                             ulPublicKeyAttributeCount, pPrivateKeyTemplate,
                                             ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey);
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      e_ret_status = KMS_GenerateKeyPair(hSession, pMechanism, pPublicKeyTemplate,
                                         ulPublicKeyAttributeCount, pPrivateKeyTemplate,
                                         ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey);
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }
#endif /* KMS_GENERATE_KEYS || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_GENERATE_RANDOM) || defined(KMS_EXT_TOKEN_ENABLED)
    case KMS_GENERATE_RANDOM_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV,    C_GenerateRandom)(CK_SESSION_HANDLE hSession,
                                                         CK_BYTE_PTR       pRandomData,
                                                         CK_ULONG          ulRandomLen); */
      CK_SESSION_HANDLE hSession;    /* the session's handle */
      CK_BYTE_PTR       pRandomData;  /* receives the random data */
      CK_ULONG          ulRandomLen; /* # of bytes to generate */

      hSession      = va_arg(arguments, CK_SESSION_HANDLE);
      pRandomData   = va_arg(arguments, CK_BYTE_PTR);
      ulRandomLen   = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pRandomData, sizeof(CK_BYTE)*ulRandomLen);
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }
      if (KMS_CheckSessionHdle(hSession) != CKR_OK)
      {
        e_ret_status = CKR_SESSION_HANDLE_INVALID;
        break;
      }

#ifdef KMS_EXT_TOKEN_ENABLED
      {
        CK_C_GenerateRandom pC_GenerateRandom;
        if ((pExtToken_FunctionList != NULL) &&
            (pExtToken_FunctionList->C_GenerateRandom != NULL))
        {
          /* If the Token support C_GenerateRandom() function, call it */
          pC_GenerateRandom = pExtToken_FunctionList -> C_GenerateRandom;

          /* Call the C_GenerateRandom function in the library */
          e_ret_status = (*pC_GenerateRandom)(hSession, pRandomData, ulRandomLen);

        }
        else
        {
          /*  KMS_GenerateRandom(); not yet supported */
          e_ret_status = CKR_FUNCTION_NOT_SUPPORTED;
        }
      }
#else /* KMS_EXT_TOKEN_ENABLED */
      /*  KMS_GenerateKeyPair(); not yet supported */
      e_ret_status = CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_EXT_TOKEN_ENABLED */
      break;
    }
#endif /* KMS_GENERATE_RANDOM || KMS_EXT_TOKEN_ENABLED */

#if defined(KMS_IMPORT_BLOB)
    case KMS_IMPORT_BLOB_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV,    C_STM_ImportBlob)(CK_BYTE_PTR pData); */
      CK_BYTE_PTR           pHdr;
      CK_BYTE_PTR           pFlash;

      /* This function is only accessible when in SECURE BOOT */
      /* Check that the Secure Engine services are not locked, LOCK shall be called only once */

      pHdr             = va_arg(arguments, CK_BYTE_PTR);
      pFlash           = va_arg(arguments, CK_BYTE_PTR);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      KMS_LL_IsBufferInSecureEnclave((void *)pHdr, sizeof(KMS_BlobRawHeaderTypeDef));
      KMS_LL_IsBufferInSecureEnclave((void *)pFlash, ((KMS_BlobRawHeaderTypeDef *)(uint32_t)pHdr)->BlobSize);

#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }

      /* This function is called while one Received Blob is not fully taken into account  */
      /* If the Update has not been finalized, then we try again till the end of the installation  */

      e_ret_status = KMS_Objects_ImportBlob(pHdr, pFlash);
      break;
    }
#endif /* KMS_IMPORT_BLOB */

#if defined(KMS_SE_LOCK_KEYS)
    case KMS_LOCK_KEYS_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV,    C_STM_LockKeys)(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount); */
      CK_OBJECT_HANDLE_PTR  pKeys;
      CK_ULONG              ulCount;

      pKeys             = va_arg(arguments, CK_OBJECT_HANDLE_PTR);
      ulCount           = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* No check, allowing secure enclave to call this service */
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }

      e_ret_status = KMS_Objects_LockKeys(pKeys, ulCount);
      break;
    }
#endif /* KMS_SE_LOCK_KEYS */

#if defined(KMS_SE_LOCK_SERVICES)
    case KMS_LOCK_SERVICES_FCT_ID:
    {
      /* PKCS#11 API declaration leading to this case: */
      /* CK_DECLARE_FUNCTION(CK_RV,    C_STM_LockServices)(CK_ULONG_PTR pServices, CK_ULONG ulCount); */
      CK_ULONG_PTR          pServices;
      CK_ULONG              ulCount;

      pServices             = va_arg(arguments, CK_OBJECT_HANDLE_PTR);
      ulCount           = va_arg(arguments, CK_ULONG);

#ifdef KMS_SE_CHECK_PARAMS
      /* Check that pointers are outside Secure Engine               */
      /* If buffer point inside Firewall, then a NVIC_SystemReset is */
      /* called by the function                                      */

      /* No check, allowing secure enclave to call this service */
#endif /* KMS_SE_CHECK_PARAMS */

      if (!KMS_IS_INITIALIZED())
      {
        e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
        break;
      }

      e_ret_status = KMS_Objects_LockServices(pServices, ulCount);
      break;
    }
#endif /* KMS_SE_LOCK_SERVICES */

    default:
    {
      e_ret_status = CKR_FUNCTION_NOT_SUPPORTED;
      break;
    }

  }

  /* Clean up */
  va_end(arguments);

  return e_ret_status;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
