/**
  ******************************************************************************
  * @file    kms_init.c
  * @author  MCD Application Team
  * @brief   Secure Engine CRYPTO module.
  *          This file provides the initiatisation function of the Key
  *            Management Services functionalities.
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
#include "kms.h"                    /* PKCS11 definitions */
#if defined(KMS_ENABLED)
#include "kms_mem.h"                /* KMS memory utilities */
#include "kms_init.h"               /* KMS session services */

#include "kms_enc_dec.h"            /* KMS encryption & decryption services */
#include "kms_nvm_storage.h"        /* KMS NVM storage services */
#include "kms_vm_storage.h"         /* KMS VM storage services */
#include "kms_platf_objects.h"      /* KMS platform objects services */
#include "kms_low_level.h"          /* CRC configuration for Init */
#include "CryptoApi/ca.h"           /* Crypto API services */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_INIT Initialization And Session handling
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
/** @addtogroup KMS_INIT_Exported_Variables Exported Variables
  * @{
  */
kms_manager_t  KMS_Manager = {0, 0, {{0}}};  /*!< KMS global manager variable */
/**
  * @brief KMS session management variable
  */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_INIT_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is called upon @ref C_Initialize call
  * @note   Refer to @ref C_Initialize function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pInitArgs either has the value NULL_PTR or points to a
  *         CK_C_INITIALIZE_ARGS structure containing information on how the
  *         library should deal with multi-threaded access
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CANT_LOCK
  *         CKR_CRYPTOKI_ALREADY_INITIALIZED
  *         CKR_FUNCTION_FAILED
  */
CK_RV  KMS_Initialize(CK_VOID_PTR pInitArgs)
{
  CK_RV e_ret_status;

  /* Check parameters */
  if (pInitArgs != NULL)
  {
#if defined(KMS_PKCS11_COMPLIANCE)
    CK_C_INITIALIZE_ARGS_PTR  ptr = (CK_C_INITIALIZE_ARGS_PTR)pInitArgs;
    /* pReserved must be NULL */
    if (ptr->pReserved != NULL)
    {
      return CKR_ARGUMENTS_BAD;
    }
    if (
      /* if there is at least one pointer set */
      ((ptr->CreateMutex != NULL) || (ptr->DestroyMutex != NULL) ||
       (ptr->LockMutex != NULL) || (ptr->UnlockMutex != NULL))
      &&
      /* and at least one pointer not set */
      ((ptr->CreateMutex == NULL) || (ptr->DestroyMutex == NULL) ||
       (ptr->LockMutex == NULL) || (ptr->UnlockMutex == NULL))
    )
    {
      return CKR_ARGUMENTS_BAD;
    }
    if ((ptr->flags == 0UL)
        &&
        ((ptr->CreateMutex == NULL) && (ptr->DestroyMutex == NULL) &&
         (ptr->LockMutex == NULL) && (ptr->UnlockMutex == NULL))
       )
    {
      /* = the application will not be accessing the Cryptoki library from multiple threads simultaneously */
      /* => Supported case */
    }
    else
    {
      return CKR_CANT_LOCK;
    }
#else /* KMS_PKCS11_COMPLIANCE */
    /* Not fully compliant, do no support non NULL parameter */
    return CKR_ARGUMENTS_BAD;
#endif /* KMS_PKCS11_COMPLIANCE */
  }

  /* Check if PKCS11 module has already been initialized */
  if (KMS_Manager.initialized == 0xFFU) /* Ensure C_Initialize is not called too many times */
  {
    e_ret_status = CKR_FUNCTION_FAILED;
  }
  else if (KMS_Manager.initialized > 0U)
  {
    KMS_Manager.initialized++;  /* Increase initialization counter to reflect number of C_Initialize calls */
    e_ret_status = CKR_CRYPTOKI_ALREADY_INITIALIZED;
  }
  else
  {
    /* Initialize KMS */
    KMS_Manager.sessionNb = 0;

    /* Initialize SessionList */
    for (uint32_t i = 1 ; i <= KMS_NB_SESSIONS_MAX; i++)
    {
      (void)memset((void *) & (KMS_GETSESSION(i)), 0, sizeof(kms_session_desc_t));
      KMS_GETSESSION(i).state = KMS_SESSION_NOT_USED;
      KMS_GETSESSION(i).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
    }

    /* This is to initialize the memory manager */
    KMS_MemInit();
    /* This is to initialize the crypto api */
    (void)CA_Init();

    /* Call the Platform Init function */
    KMS_PlatfObjects_Init();

    /* Marking module as initialized */
    KMS_Manager.initialized = 1U;

    e_ret_status = CKR_OK;
  }

  return e_ret_status;
}

/**
  * @brief  This function is called upon @ref C_Finalize call
  * @note   Refer to @ref C_Finalize function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  pReserved reserved for future versions
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED,
  */
CK_RV KMS_Finalize(CK_VOID_PTR pReserved)
{
  CK_RV e_ret_status;

  /* To fulfill the PKCS11 spec the input parameter is expected to be NULL */
  if (pReserved != NULL_PTR)
  {
    e_ret_status = CKR_ARGUMENTS_BAD;
  }
  /* Check if PKCS11 module has not been initialized */
  else if (KMS_Manager.initialized == 0U)
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else if (KMS_Manager.initialized > 1U)  /* If there are still some C_Finalize calls to come, simply return OK */
  {
    KMS_Manager.initialized--;    /* Decrease initialization counter to reflect number of C_Finalize calls */
    e_ret_status = CKR_OK;
  }
  else
  {
    /* Call the Platform Finalize function */
    KMS_PlatfObjects_Finalize();

    /* Marking module as not initialized */
    KMS_Manager.initialized = 0U;

    /* This is to initialize the crypto api */
    (void)CA_DeInit();

    e_ret_status = CKR_OK;
  }

  return e_ret_status;
}

/**
  * @brief  This function is called upon @ref C_GetTokenInfo call
  * @note   Refer to @ref C_GetTokenInfo function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID token slot ID
  * @param  pInfo token information
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  */
CK_RV KMS_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
#if defined(KMS_PKCS11_COMPLIANCE)
  const CK_TOKEN_INFO token_desc_template =
  {
    "KMS on STM32",                       /* label[32]; */

    "ST Microelectronics",                /* manufacturerID[32]; */
#ifdef KMS_EXT_TOKEN_ENABLED
    "KMS + Ext.Token",                    /* model[16]; */
#else
    "KMS Foundations",                    /* model[16]; */
#endif  /* KMS_EXT_TOKEN_ENABLED */
    "",                                   /* serialNumber[16]; */
    0,                                    /* flags; CKF_RNG, CKF_WRITE_PROTECTED, ... */

    KMS_NB_SESSIONS_MAX,                  /* ulMaxSessionCount */
    0,                                    /* ulSessionCount */
    KMS_NB_SESSIONS_MAX,                  /* ulMaxRwSessionCount */
    0,                                    /* ulRwSessionCount */

    0,                                    /* ulMaxPinLen */
    0,                                    /* ulMinPinLen */
    0,                                    /* ulTotalPublicMemory*/
    0,                                    /* ulFreePublicMemory */
    0,                                    /* ulTotalPrivateMemory */
    0,                                    /* ulFreePrivateMemory */
    {0, 0},                               /* hardwareVersion */
    {0, 1},                               /* firmwareVersion */
    {0}                                   /* utcTime[16] */
  };

  (void)(slotID);

  /* Setup the structure with the default values */
  (void)memcpy(pInfo, &token_desc_template, sizeof(token_desc_template));

  /* Pass the Flag */
  pInfo->flags = CKF_WRITE_PROTECTED;

  return CKR_OK;
#else /* KMS_PKCS11_COMPLIANCE */
  (void)(slotID);
  (void)(pInfo);
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_PKCS11_COMPLIANCE */
}

/**
  * @brief  This function is called upon @ref C_OpenSession call
  * @note   Refer to @ref C_OpenSession function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID slot ID
  * @param  flags session type
  * @param  pApplication application-defined pointer to be passed to the notification callback
  * @param  Notify notification callback function
  * @param  phSession handle for the newly opened session
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_SESSION_COUNT
  *         CKR_SESSION_PARALLEL_NOT_SUPPORTED
  */
CK_RV KMS_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
                      CK_VOID_PTR pApplication, CK_NOTIFY Notify,
                      CK_SESSION_HANDLE_PTR phSession)
{
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  uint32_t session_index;

  /* As defined in PKCS11 spec: For legacy reasons, the CKF_SERIAL_SESSION bit MUST
     always be set; if a call to C_OpenSession does not have this bit set, the call
     should return unsuccessfully with the error code CKR_SESSION_PARALLEL_NOT_SUPPORTED.
  */
  if ((flags & CKF_SERIAL_SESSION) == 0UL)
  {
    e_ret_status = CKR_SESSION_PARALLEL_NOT_SUPPORTED;
  }
  else if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  /* We reach the max number of opened sessions */
  else if (KMS_Manager.sessionNb >=  KMS_NB_SESSIONS_MAX)
  {
    e_ret_status = CKR_SESSION_COUNT;
  }
  else
  {
    /* Find a slot for a Session */
    session_index = 1;
    do
    {
      if (KMS_GETSESSION(session_index).state == KMS_SESSION_NOT_USED)
      {
        *phSession = session_index;
        KMS_GETSESSION(session_index).slotID = slotID;
        KMS_GETSESSION(session_index).state = KMS_SESSION_IDLE;      /* Initialized */
        KMS_GETSESSION(session_index).flags = flags;
        KMS_GETSESSION(session_index).pApplication = pApplication;
        KMS_GETSESSION(session_index).Notify = Notify;
        /* A session can have only one crypto mechanism on going at a time  */
        KMS_GETSESSION(session_index).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
        KMS_GETSESSION(session_index).Mechanism = CKM_VENDOR_DEFINED;
#ifdef KMS_EXT_TOKEN_ENABLED
        KMS_GETSESSION(session_index).hSession_ExtToken = 0xFFFF;
#endif /* KMS_EXT_TOKEN_ENABLED      */

        /* Increment the session counter */
        KMS_Manager.sessionNb++;

        e_ret_status = CKR_OK;
      }
      session_index++;
    } while ((e_ret_status != CKR_OK)
             && (session_index <= KMS_NB_SESSIONS_MAX)); /* Session index are going from 1 to KMS_NB_SESSIONS_MAX */
  }

  /* All sessions are in used */
  return e_ret_status;
}

#ifdef KMS_EXT_TOKEN_ENABLED
/**
  * @brief  Notification callback for external token
  * @param  hSession handle of the session performing the callback
  * @param  event type of notification callback
  * @param  pApplication application-defined value (same value as
  *         @ref KMS_OpenSession pApplication parameter)
  * @retval CKR_OK
  */
CK_RV  KMS_CallbackFunction_ForExtToken(CK_SESSION_HANDLE hSession,
                                        CK_NOTIFICATION event,
                                        CK_VOID_PTR pApplication)
{
  uint32_t session_index;

  /* This Callback is in the KMS to handle all exchanges with applications */
  /* Search corresponding session ID in KMS domain */
  /* Find a slot for a Session */
  session_index = 1;
  do
  {
    if (KMS_GETSESSION(session_index).hSession_ExtToken == hSession)
    {
      break;
    }
    session_index++;
  } while (session_index <= KMS_NB_SESSIONS_MAX);

  if (session_index <= KMS_NB_SESSIONS_MAX)
  {
    /* Check that the pointer to the callback is valid: in non secure & != NULL */
    if (KMS_GETSESSION(hSession).Notify != NULL)
    {
      /* Call the Notify Callback() */
      (KMS_GETSESSION(hSession).Notify)(session_index, event, KMS_GETSESSION(hSession).pApplication);
    }
  }
  return CKR_OK;
}
#endif /* KMS_EXT_TOKEN_ENABLED */

#ifdef KMS_EXT_TOKEN_ENABLED
/**
  * @brief  Used to link External Token session handle to internal session handle
  * @param  hSession internal session handle
  * @param  hSession_ExtToken external token session handle
  * @retval CKR_OK
  */
CK_RV  KMS_OpenSession_RegisterExtToken(CK_SESSION_HANDLE hSession,
                                        CK_SESSION_HANDLE hSession_ExtToken)
{
  /* Check that the session correspond to a waiting session */
  if ((KMS_GETSESSION(hSession).state == KMS_SESSION_IDLE) &&
      (KMS_GETSESSION(hSession).hSession_ExtToken == 0xFFFF) &&
      (hSession <= KMS_NB_SESSIONS_MAX))
  {
    /* Record the  corresponding SessionNumber of the External Token */
    KMS_GETSESSION(hSession).hSession_ExtToken = hSession_ExtToken;
  }

  return CKR_OK;
}
#endif /* KMS_EXT_TOKEN_ENABLED */

/**
  * @brief  This function is called upon @ref C_CloseSession call
  * @note   Refer to @ref C_CloseSession function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV KMS_CloseSession(CK_SESSION_HANDLE hSession)
{
  CK_RV e_ret_status = CKR_SESSION_HANDLE_INVALID;

  if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  /* No processing ongoing  */
  else if (KMS_GETSESSION(hSession).state != KMS_SESSION_IDLE)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  else
  {
    /* Reset session descriptor */
    (void)memset((void *) & (KMS_GETSESSION(hSession)), 0, sizeof(kms_session_desc_t));
    KMS_GETSESSION(hSession).state = KMS_SESSION_NOT_USED;
    KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;

#ifdef KMS_EXT_TOKEN_ENABLED
    KMS_GETSESSION(hSession).hSession_ExtToken = 0xFFFF;
#endif /* KMS_EXT_TOKEN_ENABLED      */

    /* Decrement the session counter */
    KMS_Manager.sessionNb--;

    e_ret_status = CKR_OK;
  }
  return e_ret_status;
}

/**
  * @brief  Allows to check that the given handle corresponds to an opened session
  * @note   Can be called after @ref KMS_OpenSession and before @ref KMS_CloseSession
  * @param  hSession session handle
  * @retval CKR_OK
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV     KMS_CheckSessionHdle(CK_SESSION_HANDLE hSession)
{
  CK_RV e_ret_status = CKR_SESSION_HANDLE_INVALID;

  if ((hSession >= 1UL) &&
      (hSession <= KMS_NB_SESSIONS_MAX) &&
      (KMS_GETSESSION(hSession).state != KMS_SESSION_NOT_USED))
  {
    e_ret_status = CKR_OK;
  }
  return e_ret_status;
}

/**
  * @brief  Set session state to IDLE and perform associated actions
  * @param  hSession session handle
  * @retval None
  */
void     KMS_SetStateIdle(CK_SESSION_HANDLE hSession)
{
  KMS_GETSESSION(hSession).state = KMS_SESSION_IDLE;
}

/**
  * @brief  This function is called upon @ref C_GetMechanismInfo call
  * @note   Refer to @ref C_GetMechanismInfo function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  slotID token slot ID
  * @param  type type of mechanism
  * @param  pInfo mechanism information
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  */
CK_RV     KMS_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR pInfo)
{
#if defined(KMS_SEARCH)
  CK_RV ret_status = CKR_MECHANISM_INVALID;

  (void)slotID;

#if defined(KMS_ECDSA)
#if defined(KMS_EC_SECP384)
#define MECHANISM_ECC_MAX_KEYSIZE   (CA_CRL_ECC_P384_SIZE*8UL)
#elif defined(KMS_EC_SECP256)
#define MECHANISM_ECC_MAX_KEYSIZE   (CA_CRL_ECC_P256_SIZE*8UL)
#elif defined(KMS_EC_SECP192)
#define MECHANISM_ECC_MAX_KEYSIZE   (CA_CRL_ECC_P192_SIZE*8UL)
#else /* KMS_EC_SECxxxx */
#error "No EC curve enabled"
#endif /* KMS_EC_SECxxxx */
#if defined(KMS_EC_SECP192)
#define MECHANISM_ECC_MIN_KEYSIZE   (CA_CRL_ECC_P192_SIZE*8UL)
#elif defined(KMS_EC_SECP256)
#define MECHANISM_ECC_MIN_KEYSIZE   (CA_CRL_ECC_P256_SIZE*8UL)
#elif defined(KMS_EC_SECP384)
#define MECHANISM_ECC_MIN_KEYSIZE   (CA_CRL_ECC_P384_SIZE*8UL)
#else /* KMS_EC_SECxxxx */
#error "No EC curve enabled"
#endif /* KMS_EC_SECxxxx */
#endif /* KMS_ECDSA */

#if defined(KMS_RSA)
#if defined(KMS_RSA_2048)
#define MECHANISM_RSA_MAX_KEYSIZE         (CA_CRL_RSA2048_MOD_SIZE*8UL)
#elif defined(KMS_RSA_1024)
#define MECHANISM_RSA_MAX_KEYSIZE         (CA_CRL_RSA1024_MOD_SIZE*8UL)
#else /* KMS_RSA_xxxx */
#error "No RSA modulus size specified"
#endif /* KMS_RSA_xxxx */
#if defined(KMS_RSA_1024)
#define MECHANISM_RSA_MIN_KEYSIZE         (CA_CRL_RSA1024_MOD_SIZE*8UL)
#elif defined(KMS_RSA_2048)
#define MECHANISM_RSA_MIN_KEYSIZE         (CA_CRL_RSA2048_MOD_SIZE*8UL)
#else /* KMS_RSA_xxxx */
#error "No RSA modulus size specified"
#endif /* KMS_RSA_xxxx */
#endif /* KMS_RSA */

  switch (type)
  {
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
    case CKM_SHA_1:
    {
      pInfo->flags = 0U
#if (KMS_SHA1 & KMS_FCT_DIGEST)
                     | CKF_DIGEST
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
                     ;
      pInfo->ulMaxKeySize = 0U;
      pInfo->ulMinKeySize = 0U;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */

#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
    case CKM_SHA256:
    {
      pInfo->flags = 0U
#if (KMS_SHA256 & KMS_FCT_DIGEST)
                     | CKF_DIGEST
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
                     ;
      pInfo->ulMaxKeySize = 0U;
      pInfo->ulMinKeySize = 0U;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */

#if defined(KMS_AES_CBC) && ((KMS_AES_CBC & KMS_FCT_ENCRYPT) || (KMS_AES_CBC & KMS_FCT_DECRYPT))
    case CKM_AES_CBC:
    {
      pInfo->flags = 0U
#if (KMS_AES_CBC & KMS_FCT_ENCRYPT)
                     | CKF_ENCRYPT
#endif /* KMS_AES_CBC & KMS_FCT_ENCRYPT */
#if (KMS_AES_CBC & KMS_FCT_DECRYPT)
                     | CKF_DECRYPT
#endif /* KMS_AES_CBC & KMS_FCT_DECRYPT */
                     ;
      pInfo->ulMaxKeySize = CA_CRL_AES256_KEY;
      pInfo->ulMinKeySize = CA_CRL_AES128_KEY;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_CBC & KMS_FCT_ENCRYPT || KMS_AES_CBC & KMS_FCT_DECRYPT */

#if defined(KMS_AES_CCM) && ((KMS_AES_CCM & KMS_FCT_ENCRYPT) || (KMS_AES_CCM & KMS_FCT_DECRYPT))
    case CKM_AES_CCM:
    {
      pInfo->flags = 0U
#if (KMS_AES_CCM & KMS_FCT_ENCRYPT)
                     | CKF_ENCRYPT
#endif /* KMS_AES_CCM & KMS_FCT_ENCRYPT */
#if (KMS_AES_CCM & KMS_FCT_DECRYPT)
                     | CKF_DECRYPT
#endif /* KMS_AES_CCM & KMS_FCT_DECRYPT */
                     ;
      pInfo->ulMaxKeySize = CA_CRL_AES256_KEY;
      pInfo->ulMinKeySize = CA_CRL_AES128_KEY;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_CCM & KMS_FCT_ENCRYPT || KMS_AES_CCM & KMS_FCT_DECRYPT */

#if defined(KMS_AES_ECB) && ((KMS_AES_ECB & KMS_FCT_ENCRYPT) || (KMS_AES_ECB & KMS_FCT_DECRYPT))
    case CKM_AES_ECB:
    {
      pInfo->flags = 0U
#if (KMS_AES_ECB & KMS_FCT_ENCRYPT)
                     | CKF_ENCRYPT
#endif /* KMS_AES_ECB & KMS_FCT_ENCRYPT */
#if (KMS_AES_ECB & KMS_FCT_DECRYPT)
                     | CKF_DECRYPT
#endif /* KMS_AES_ECB & KMS_FCT_DECRYPT */
                     ;
      pInfo->ulMaxKeySize = CA_CRL_AES256_KEY;
      pInfo->ulMinKeySize = CA_CRL_AES128_KEY;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_ECB & KMS_FCT_ENCRYPT || KMS_AES_ECB & KMS_FCT_DECRYPT */

#if defined(KMS_AES_GCM) && ((KMS_AES_GCM & KMS_FCT_ENCRYPT) || (KMS_AES_GCM & KMS_FCT_DECRYPT))
    case CKM_AES_GCM:
    {
      pInfo->flags = 0U
#if (KMS_AES_GCM & KMS_FCT_ENCRYPT)
                     | CKF_ENCRYPT
#endif /* KMS_AES_GCM & KMS_FCT_ENCRYPT */
#if (KMS_AES_GCM & KMS_FCT_DECRYPT)
                     | CKF_DECRYPT
#endif /* KMS_AES_GCM & KMS_FCT_DECRYPT */
                     ;
      pInfo->ulMaxKeySize = CA_CRL_AES256_KEY;
      pInfo->ulMinKeySize = CA_CRL_AES128_KEY;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_GCM & KMS_FCT_ENCRYPT || KMS_AES_GCM & KMS_FCT_DECRYPT */

#if defined(KMS_DERIVE_KEY) && defined(KMS_AES_ECB) && (KMS_AES_ECB & KMS_FCT_DERIVE_KEY)
    case CKM_AES_ECB_ENCRYPT_DATA:
    {
      pInfo->flags = CKF_DERIVE;
      pInfo->ulMaxKeySize = CA_CRL_AES256_KEY;
      pInfo->ulMinKeySize = CA_CRL_AES128_KEY;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_DERIVE_KEY && KMS_AES_ECB & KMS_FCT_DERIVE_KEY */

#if defined(KMS_AES_CMAC) && ((KMS_AES_CMAC & KMS_FCT_SIGN) || (KMS_AES_CMAC & KMS_FCT_VERIFY))
    case CKM_AES_CMAC:
    case CKM_AES_CMAC_GENERAL:
    {
      pInfo->flags = 0U
#if (KMS_AES_CMAC & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_AES_CMAC & KMS_FCT_SIGN */
#if (KMS_AES_CMAC & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_AES_CMAC & KMS_FCT_VERIFY */
                     ;
      pInfo->ulMaxKeySize = CA_CRL_AES256_KEY;
      pInfo->ulMinKeySize = CA_CRL_AES128_KEY;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_CMAC & KMS_FCT_SIGN || KMS_AES_CMAC & KMS_FCT_VERIFY */

#if defined(KMS_DERIVE_KEY) && defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_DERIVE_KEY)
    case CKM_ECDH1_DERIVE:
    {
      pInfo->flags = 0U
                     | CKF_DERIVE
                     | CKF_EC_F_P                      /* F2P curves support */
                     | CKF_EC_ECPARAMETERS             /* CKA_EC_PARAM to specify working curve */
                     | CKF_EC_UNCOMPRESS               /* X9.62 uncompressed format support */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_ECC_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_ECC_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_DERIVE_KEY && KMS_ECDSA & KMS_FCT_DERIVE_KEY */

#if defined(KMS_GENERATE_KEYS) && defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_GENERATE_KEYS)
    case CKM_EC_KEY_PAIR_GEN:
    {
      pInfo->flags = 0U
                     | CKF_GENERATE_KEY_PAIR
                     | CKF_EC_F_P                      /* F2P curves support */
                     | CKF_EC_ECPARAMETERS             /* CKA_EC_PARAM to specify working curve */
                     | CKF_EC_UNCOMPRESS               /* X9.62 uncompressed format support */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_ECC_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_ECC_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_GENERATE_KEYS && KMS_ECDSA & KMS_FCT_GENERATE_KEYS */

#if defined(KMS_RSA) && ((KMS_RSA & KMS_FCT_SIGN) || (KMS_RSA & KMS_FCT_VERIFY))
    case CKM_RSA_PKCS:
    {
      pInfo->flags = 0U
#if (KMS_RSA & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_RSA & KMS_FCT_SIGN */
#if (KMS_RSA & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_RSA & KMS_FCT_VERIFY */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_RSA_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_RSA_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_RSA & KMS_FCT_SIGN || KMS_RSA & KMS_FCT_VERIFY */

#if defined(KMS_RSA) && defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST) \
&& ((KMS_RSA & KMS_FCT_SIGN) || (KMS_RSA & KMS_FCT_VERIFY))
    case CKM_SHA1_RSA_PKCS:
    {
      pInfo->flags = 0U
#if (KMS_RSA & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_RSA & KMS_FCT_SIGN */
#if (KMS_RSA & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_RSA & KMS_FCT_VERIFY */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_RSA_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_RSA_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_RSA & KMS_FCT_SIGN || KMS_RSA & KMS_FCT_VERIFY && KMS_SHA1 & KMS_FCT_DIGEST */

#if defined(KMS_RSA) && defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST) \
&& ((KMS_RSA & KMS_FCT_SIGN) || (KMS_RSA & KMS_FCT_VERIFY))
    case CKM_SHA256_RSA_PKCS:
    {
      pInfo->flags = 0U
#if (KMS_RSA & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_RSA & KMS_FCT_SIGN */
#if (KMS_RSA & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_RSA & KMS_FCT_VERIFY */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_RSA_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_RSA_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_RSA & KMS_FCT_SIGN || KMS_RSA & KMS_FCT_VERIFY && KMS_SHA256 & KMS_FCT_DIGEST */

#if defined(KMS_ECDSA) && ((KMS_ECDSA & KMS_FCT_SIGN) || (KMS_ECDSA & KMS_FCT_VERIFY))
    case CKM_ECDSA:
    {
      pInfo->flags = 0U
#if (KMS_ECDSA & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_ECDSA & KMS_FCT_SIGN */
#if (KMS_ECDSA & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
                     | CKF_EC_F_P                      /* F2P curves support */
                     | CKF_EC_ECPARAMETERS             /* CKA_EC_PARAM to specify working curve */
                     | CKF_EC_UNCOMPRESS               /* X9.62 uncompressed format support */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_ECC_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_ECC_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_ECDSA & KMS_FCT_SIGN || KMS_ECDSA & KMS_FCT_VERIFY */

#if defined(KMS_ECDSA) && defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST) \
&& ((KMS_ECDSA & KMS_FCT_SIGN) || (KMS_ECDSA & KMS_FCT_VERIFY))
    case CKM_ECDSA_SHA1:
    {
      pInfo->flags = 0U
#if (KMS_ECDSA & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_ECDSA & KMS_FCT_SIGN */
#if (KMS_ECDSA & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
                     | CKF_EC_F_P                      /* F2P curves support */
                     | CKF_EC_ECPARAMETERS             /* CKA_EC_PARAM to specify working curve */
                     | CKF_EC_UNCOMPRESS               /* X9.62 uncompressed format support */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_ECC_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_ECC_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_ECDSA & KMS_FCT_SIGN || KMS_ECDSA & KMS_FCT_VERIFY && KMS_SHA1 & KMS_FCT_DIGEST */

#if defined(KMS_ECDSA) && defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST) \
&& ((KMS_ECDSA & KMS_FCT_SIGN) || (KMS_ECDSA & KMS_FCT_VERIFY))
    case CKM_ECDSA_SHA256:
    {
      pInfo->flags = 0U
#if (KMS_ECDSA & KMS_FCT_SIGN)
                     | CKF_SIGN
#endif /* KMS_ECDSA & KMS_FCT_SIGN */
#if (KMS_ECDSA & KMS_FCT_VERIFY)
                     | CKF_VERIFY
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
                     | CKF_EC_F_P                      /* F2P curves support */
                     | CKF_EC_ECPARAMETERS             /* CKA_EC_PARAM to specify working curve */
                     | CKF_EC_UNCOMPRESS               /* X9.62 uncompressed format support */
                     ;
      pInfo->ulMaxKeySize = MECHANISM_ECC_MAX_KEYSIZE;
      pInfo->ulMinKeySize = MECHANISM_ECC_MIN_KEYSIZE;
      ret_status = CKR_OK;
      break;
    }
#endif /* KMS_ECDSA & KMS_FCT_SIGN || KMS_ECDSA & KMS_FCT_VERIFY && KMS_SHA256 & KMS_FCT_DIGEST */

    default:
    {
      pInfo->flags = 0;
      pInfo->ulMaxKeySize = 0;
      pInfo->ulMinKeySize = 0;
      break;
    }
  }

  return ret_status;
#else /* KMS_SEARCH */
  (void)slotID;
  (void)type;
  (void)pInfo;
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SEARCH */
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
