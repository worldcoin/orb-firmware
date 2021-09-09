/**
  ******************************************************************************
  * @file    kms_digest.c
  * @author  MCD Application Team
  * @brief   This file contains implementation for Key Management Services (KMS)
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"                /* PKCS11 definitions */
#if defined(KMS_ENABLED)
#include "kms_init.h"           /* KMS session services */
#include "kms_digest.h"         /* KMS digest services */
#include "kms_mem.h"            /* KMS memory utilities */
#include "CryptoApi/ca.h"       /* Crypto API services */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_DIGEST Digest services
  * @{
  */

/* Private types -------------------------------------------------------------*/
/** @addtogroup KMS_DIGEST_Private_Types Private Types
  * @{
  */
#if defined(KMS_SHA1)
/**
  * @brief  SHA1 computing context structure
  */
typedef struct
{
  CA_SHA1ctx_stt ca_ctx;            /*!< Used to store crypto library context */
} kms_sha1_ctx_t;
#endif /* KMS_SHA1 */
#if defined(KMS_SHA256)
/**
  * @brief  SHA256 computing context structure
  */
typedef struct
{
  CA_SHA256ctx_stt ca_ctx;          /*!< Used to store crypto library context */
} kms_sha256_ctx_t;
#endif /* KMS_SHA256 */
/**
  * @}
  */
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is called upon @ref C_DigestInit call
  * @note   Refer to @ref C_DigestInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pMechanism digest mechanism
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV     KMS_DigestInit(CK_SESSION_HANDLE hSession,  CK_MECHANISM_PTR pMechanism)
{
#if defined(KMS_DIGEST)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;

  if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  /* No processing already on going  */
  else if (KMS_GETSESSION(hSession).state != KMS_SESSION_IDLE)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  /* Check parameter */
  else if (pMechanism == NULL_PTR)
  {
    e_ret_status = CKR_ARGUMENTS_BAD;
  }
  else
  {
    switch (pMechanism->mechanism)
    {
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
      case CKM_SHA_1:
      {
        kms_sha1_ctx_t *phash_ctx;

        /* Allocate context for SHA1 */
        phash_ctx = KMS_Alloc(hSession, sizeof(kms_sha1_ctx_t));
        if (phash_ctx != NULL_PTR)
        {
          KMS_GETSESSION(hSession).pCtx = phash_ctx;
          KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;

          phash_ctx->ca_ctx.mFlags = CA_E_HASH_DEFAULT;
          phash_ctx->ca_ctx.mTagSize = (int32_t)CA_CRL_SHA1_SIZE;

          /* Initialize SHA1 processing */
          if (CA_SHA1_Init(&(phash_ctx->ca_ctx)) == CA_HASH_SUCCESS)
          {
            e_ret_status = CKR_OK ;
          }
          else
          {
            KMS_Free(hSession, phash_ctx);
            KMS_GETSESSION(hSession).pCtx = NULL_PTR;
          }
        }
        else
        {
          e_ret_status = CKR_DEVICE_MEMORY;
        }
        break;
      }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
      case CKM_SHA256:
      {
        kms_sha256_ctx_t *phash_ctx;

        /* Allocate context for SHA256 */
        phash_ctx = KMS_Alloc(hSession, sizeof(kms_sha256_ctx_t));
        if (phash_ctx != NULL_PTR)
        {
          KMS_GETSESSION(hSession).pCtx = phash_ctx;
          KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;

          phash_ctx->ca_ctx.mFlags = CA_E_HASH_DEFAULT;
          phash_ctx->ca_ctx.mTagSize = (int32_t)CA_CRL_SHA256_SIZE;

          /* Initialize SHA256 processing */
          if (CA_SHA256_Init(&(phash_ctx->ca_ctx)) == CA_HASH_SUCCESS)
          {
            e_ret_status = CKR_OK ;
          }
          else
          {
            KMS_Free(hSession, phash_ctx);
            KMS_GETSESSION(hSession).pCtx = NULL_PTR;
          }
        }
        else
        {
          e_ret_status = CKR_DEVICE_MEMORY;
        }
        break;
      }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
      default:
        e_ret_status = CKR_MECHANISM_INVALID;
        break;
    }
  }

  if (e_ret_status == CKR_OK)
  {
    KMS_GETSESSION(hSession).state = KMS_SESSION_DIGEST;
  }

  return e_ret_status;
#else /* KMS_DIGEST */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DIGEST */
}

/**
  * @brief  This function is called upon @ref C_Digest call
  * @note   Refer to @ref C_Digest function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pData data to digest
  * @param  ulDataLen length of data to digest
  * @param  pDigest location to store digest message
  * @param  pulDigestLen length of the digest message
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV   KMS_Digest(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                   CK_ULONG ulDataLen, CK_BYTE_PTR pDigest, CK_ULONG_PTR pulDigestLen)
{
#if defined(KMS_DIGEST)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  int32_t cryptolib_status;    /* CryptoLib error etatus */

  if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  else if (KMS_GETSESSION(hSession).state != KMS_SESSION_DIGEST)
  {
    e_ret_status = CKR_OPERATION_NOT_INITIALIZED;
  }
  else
  {
    switch (KMS_GETSESSION(hSession).Mechanism)
    {
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
      case CKM_SHA_1:
      {
        kms_sha1_ctx_t *phash_ctx;

        KMS_CHECK_BUFFER_SECTION5_2(pDigest, pulDigestLen, CA_CRL_SHA1_SIZE);

        /* Retrieve allocated context */
        phash_ctx = KMS_GETSESSION(hSession).pCtx;
        /* Add data to be hashed */
        cryptolib_status = CA_SHA1_Append(&(phash_ctx->ca_ctx), pData, (int32_t)ulDataLen);

        if (cryptolib_status == CA_HASH_SUCCESS)
        {
          /* retrieve */
          cryptolib_status = CA_SHA1_Finish(&(phash_ctx->ca_ctx), pDigest, (int32_t *)(uint32_t)pulDigestLen);
          if (cryptolib_status == CA_HASH_SUCCESS)
          {
            e_ret_status = CKR_OK;
          }
        }
        break;
      }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
      case CKM_SHA256:
      {
        kms_sha256_ctx_t *phash_ctx;

        KMS_CHECK_BUFFER_SECTION5_2(pDigest, pulDigestLen, CA_CRL_SHA256_SIZE);

        /* Retrieve allocated context */
        phash_ctx = KMS_GETSESSION(hSession).pCtx;
        /* Add data to be hashed */
        cryptolib_status = CA_SHA256_Append(&(phash_ctx->ca_ctx), pData, (int32_t)ulDataLen);

        if (cryptolib_status == CA_HASH_SUCCESS)
        {
          /* retrieve */
          cryptolib_status = CA_SHA256_Finish(&(phash_ctx->ca_ctx), pDigest, (int32_t *)(uint32_t)pulDigestLen);
          if (cryptolib_status == CA_HASH_SUCCESS)
          {
            e_ret_status = CKR_OK;
          }
        }
        break;
      }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
      default:
        e_ret_status = CKR_MECHANISM_INVALID;
        break;
    }
  }

  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }

  /* Termination of the Digest */
  KMS_SetStateIdle(hSession);

  return e_ret_status;
#else /* KMS_DIGEST */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DIGEST */
}

/**
  * @brief  This function is called upon @ref C_DigestUpdate call
  * @note   Refer to @ref C_DigestUpdate function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pPart data part to digest
  * @param  ulPartLen length of data part to digest
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV   KMS_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen)
{
#if defined(KMS_DIGEST)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  int32_t cryptolib_status;    /* CryptoLib error status */

  if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  else if (KMS_GETSESSION(hSession).state != KMS_SESSION_DIGEST)
  {
    e_ret_status = CKR_OPERATION_NOT_INITIALIZED;
  }
  else
  {
    switch (KMS_GETSESSION(hSession).Mechanism)
    {
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
      case CKM_SHA_1:
      {
        kms_sha1_ctx_t *phash_ctx;

        /* Retrieve allocated context */
        phash_ctx = KMS_GETSESSION(hSession).pCtx;
        /* Add data to be hashed */
        cryptolib_status = CA_SHA1_Append(&(phash_ctx->ca_ctx), pPart, (int32_t)ulPartLen);
        if (cryptolib_status == CA_HASH_SUCCESS)
        {
          e_ret_status = CKR_OK;
        }
        break;
      }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
      case CKM_SHA256:
      {
        kms_sha256_ctx_t *phash_ctx;

        /* Retrieve allocated context */
        phash_ctx = KMS_GETSESSION(hSession).pCtx;
        /* Add data to be hashed */
        cryptolib_status = CA_SHA256_Append(&(phash_ctx->ca_ctx), pPart, (int32_t)ulPartLen);
        if (cryptolib_status == CA_HASH_SUCCESS)
        {
          e_ret_status = CKR_OK;
        }
        break;
      }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
      default:
        e_ret_status = CKR_MECHANISM_INVALID;
        break;
    }
  }

  return e_ret_status;
#else /* KMS_DIGEST */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DIGEST */
}

/**
  * @brief  This function is called upon @ref C_DigestFinal call
  * @note   Refer to @ref C_DigestFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pDigest location to store digest message
  * @param  pulDigestLen length of the digest message
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV   KMS_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
                        CK_ULONG_PTR pulDigestLen)
{
#if defined(KMS_DIGEST)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  int32_t cryptolib_status;    /* CryptoLib error status */

  if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    e_ret_status = CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  else if (KMS_GETSESSION(hSession).state != KMS_SESSION_DIGEST)
  {
    e_ret_status = CKR_OPERATION_NOT_INITIALIZED;
  }
  else
  {
    switch (KMS_GETSESSION(hSession).Mechanism)
    {
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
      case CKM_SHA_1:
      {
        kms_sha1_ctx_t *phash_ctx;

        KMS_CHECK_BUFFER_SECTION5_2(pDigest, pulDigestLen, CA_CRL_SHA1_SIZE);

        /* Retrieve allocated context */
        phash_ctx = KMS_GETSESSION(hSession).pCtx;
        /* retrieve */
        cryptolib_status = CA_SHA1_Finish(&(phash_ctx->ca_ctx), pDigest, (int32_t *)(uint32_t)pulDigestLen);
        if (cryptolib_status == CA_HASH_SUCCESS)
        {
          e_ret_status = CKR_OK;
        }
        break;
      }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
      case CKM_SHA256:
      {
        kms_sha256_ctx_t *phash_ctx;

        KMS_CHECK_BUFFER_SECTION5_2(pDigest, pulDigestLen, CA_CRL_SHA256_SIZE);

        /* Retrieve allocated context */
        phash_ctx = KMS_GETSESSION(hSession).pCtx;
        /* retrieve */
        cryptolib_status = CA_SHA256_Finish(&(phash_ctx->ca_ctx), pDigest, (int32_t *)(uint32_t)pulDigestLen);
        if (cryptolib_status == CA_HASH_SUCCESS)
        {
          e_ret_status = CKR_OK;
        }
        break;
      }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
      default:
        e_ret_status = CKR_MECHANISM_INVALID;
        break;
    }
  }

  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }
  /* Termination of the Digest */
  KMS_SetStateIdle(hSession);

  return e_ret_status;
#else /* KMS_DIGEST */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DIGEST */
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
