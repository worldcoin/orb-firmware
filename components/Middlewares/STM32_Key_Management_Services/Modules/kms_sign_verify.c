/**
  ******************************************************************************
  * @file    kms_sign_verify.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"                /* PKCS11 definitions */
#if defined(KMS_ENABLED)
#include "kms_init.h"           /* KMS session services */
#include "kms_mem.h"            /* KMS memory utilities */
#include "kms_sign_verify.h"    /* KMS Signature & Verification services */
#include "kms_objects.h"        /* KMS object management services */
#include "CryptoApi/ca.h"       /* Crypto API services */
#include "kms_ecc.h"            /* KMS elliptic curves utils */
#include "kms_der_x962.h"       /* KMS DER & X962 utilities */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_SIGN Sign and Verify services
  * @{
  */

/* Private types -------------------------------------------------------------*/
/** @addtogroup KMS_SIGN_Private_Types Private Types
  * @{
  */
#if defined(KMS_RSA) || defined(KMS_ECDSA)
/**
  * @brief Generic signature/verification processing structure used has header of
  *        asymmetric hash based sign/verify algorithms context structures
  */
typedef struct
{
#ifdef KMS_SHA256
  uint8_t hash[CA_CRL_SHA256_SIZE];       /*!< Used to store hash to use during processing */
#else /* KMS_SHA256 */
  uint8_t hash[CA_CRL_SHA1_SIZE];         /*!< Used to store hash to use during processing */
#endif /* KMS_SHA256 */
  int32_t hashSize;                       /*!< Size of the stored hash */
  CA_hashType_et hashMethod;              /*!< Hash method used to compute the stored hash */
} kms_asym_sv_ctx_t;
#endif /* KMS_RSA || KMS_ECDSA */

#if defined(KMS_RSA)
/**
  * @brief RSA signature verification context
  */
typedef struct
{
  /* Struct start is same as kms_asym_sv_ctx_t */
#ifdef KMS_SHA256
  uint8_t hash[CA_CRL_SHA256_SIZE];       /*!< Used to store hash to use during processing */
#else /* KMS_SHA256 */
  uint8_t hash[CA_CRL_SHA1_SIZE];         /*!< Used to store hash to use during processing */
#endif /* KMS_SHA256 */
  int32_t hashSize;                       /*!< Size of the stored hash */
  CA_hashType_et hashMethod;              /*!< Hash method used to compute the stored hash */
  /* Structure differentiation */
  uint8_t tmpbuffer[CA_RSA_REQUIRED_WORKING_BUFFER] __attribute__((aligned(4)));    /*!< Crypto lib working buffer */
  /* Used as a working buffer for the crypto library */
#if defined(KMS_RSA_2048)
  uint8_t privExp[CA_CRL_RSA2048_PRIV_SIZE];          /*!< Used to store processing key private exponent */
  uint8_t pubExp[CA_CRL_RSA2048_PUB_SIZE];            /*!< Used to store processing key public exponent */
  uint8_t modulus[CA_CRL_RSA2048_MOD_SIZE];           /*!< Used to store processing key modulus */
#else /* KMS_RSA_2048 */
  uint8_t privExp[CA_CRL_RSA1024_PRIV_SIZE];          /*!< Used to store processing key private exponent */
  uint8_t pubExp[CA_CRL_RSA1024_PUB_SIZE];            /*!< Used to store processing key public exponent */
  uint8_t modulus[CA_CRL_RSA1024_MOD_SIZE];           /*!< Used to store processing key modulus */
#endif /* KMS_RSA_2048 */
} kms_rsa_sv_ctx_t;
#endif /* KMS_RSA */

#if defined(KMS_ECDSA)
/**
  * @brief ECDSA signature verification context
  */
typedef struct
{
  /* Struct start is same as kms_asym_sv_ctx_t */
#ifdef KMS_SHA256
  uint8_t hash[CA_CRL_SHA256_SIZE];       /*!< Used to store hash to use during processing */
#else /* KMS_SHA256 */
  uint8_t hash[CA_CRL_SHA1_SIZE];         /*!< Used to store hash to use during processing */
#endif /* KMS_SHA256 */
  int32_t hashSize;                       /*!< Size of the stored hash */
  CA_hashType_et hashMethod;              /*!< Hash method used to compute the stored hash */
  /* Structure differentiation */
  uint8_t tmpbuffer[CA_ECDSA_REQUIRED_WORKING_BUFFER] __attribute__((aligned(4)));  /*!< Crypto lib working buffer */
  /* Used as a working buffer for the crypto library */
#if defined(KMS_EC_SECP384)
  uint8_t der_pub[2 * CA_CRL_ECC_P384_SIZE + 4 /* To include DER & X9.62 header */]; /*!< Used to store processing
                                                                                  public key in DER +X9.62 format */
  uint8_t pub_x[CA_CRL_ECC_P384_SIZE];            /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P384_SIZE];            /*!< Used to store processing public key y coordinate */
#elif defined(KMS_EC_SECP256)
  uint8_t der_pub[2 * CA_CRL_ECC_P256_SIZE + 4 /* To include DER & X9.62 header */]; /*!< Used to store processing
                                                                                  public key in DER +X9.62 format */
  uint8_t pub_x[CA_CRL_ECC_P256_SIZE];            /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P256_SIZE];            /*!< Used to store processing public key y coordinate */
#else /* KMS_EC_SECP384 || KMS_EC_SECP256 */
  uint8_t der_pub[2 * CA_CRL_ECC_P192_SIZE + 4 /* To include DER & X9.62 header */]; /*!< Used to store processing
                                                                                  public key in DER +X9.62 format */
  uint8_t pub_x[CA_CRL_ECC_P192_SIZE];            /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P192_SIZE];            /*!< Used to store processing public key y coordinate */
#endif /* KMS_EC_SECP384 || KMS_EC_SECP256 */
  uint32_t pub_size;                              /*!< Used to store processing public key size */
} kms_ecdsa_sv_ctx_t;
#endif /* KMS_ECDSA */

#ifdef KMS_AES_CMAC
/**
  * @brief AES CMAC signature verification context
  */
typedef struct
{
  uint8_t key[CA_CRL_AES256_KEY];        /*!< Used to store key to use during processing */
  uint8_t tag[CA_CRL_AES_BLOCK];         /*!< Used to store tag at the end of processing */
  uint32_t tagLength;                    /*!< Host stored tag length */
  CA_AESCMACctx_stt ca_ctx;              /*!< Used to store crypto library context */
} kms_aes_cmac_sv_ctx_t;
#endif /* KMS_AES_CMAC */
/**
  * @}
  */
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/** @addtogroup KMS_SIGN_Private_Functions Private Functions
  * @{
  */
#if defined(KMS_SIGN) || defined(KMS_VERIFY)
/* Common function used to process signature & verification initialization */
#define KMS_FLAG_SIGN   0U    /*!< Signature requested */
#define KMS_FLAG_VERIFY 1U    /*!< Verification requested */
static CK_RV  sign_verify_init(CK_SESSION_HANDLE hSession,
                               CK_MECHANISM_PTR  pMechanism,
                               CK_OBJECT_HANDLE  hKey, uint32_t sigver_flag);
#endif /* KMS_SIGN || KMS_VERIFY */
/**
  * @}
  */

/* Private function ----------------------------------------------------------*/
/** @addtogroup KMS_SIGN_Private_Functions Private Functions
  * @{
  */

#if defined(KMS_SIGN) || defined(KMS_VERIFY)
/**
  * @brief  This function is called upon @ref C_SignInit and @ref C_VerifyInit call
  * @note   Refer to @ref C_SignInit and @ref C_VerifyInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pMechanism signature mechanism
  * @param  hKey signature key
  * @param  sigver_flag action to perform: sign or verify
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_FAILED
  *         CKR_MECHANISM_INVALID
  *         CKR_OBJECT_HANDLE_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  *         @ref KMS_Objects_SearchAttributes returned values
  */
CK_RV     sign_verify_init(CK_SESSION_HANDLE hSession,
                           CK_MECHANISM_PTR  pMechanism,
                           CK_OBJECT_HANDLE  hKey,
                           uint32_t sigver_flag)
{
  CK_RV e_ret_status = CKR_MECHANISM_INVALID;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* No processing already on going  */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_IDLE)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }

  /* Check that we support the expected mechanism  */
  if (pMechanism == NULL_PTR)
  {
    return CKR_ARGUMENTS_BAD;
  }
  switch (pMechanism->mechanism)
  {
#if defined(KMS_RSA) && ((KMS_RSA & KMS_FCT_SIGN) | (KMS_RSA & KMS_FCT_VERIFY))
    case CKM_RSA_PKCS:
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
    case CKM_SHA1_RSA_PKCS:
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
    case CKM_SHA256_RSA_PKCS:
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
    {
      kms_obj_keyhead_t *pkms_object;
      kms_rsa_sv_ctx_t *p_ctx;

      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* Allocate context */
        p_ctx = KMS_Alloc(hSession, sizeof(kms_rsa_sv_ctx_t));
        if (p_ctx == NULL_PTR)
        {
          e_ret_status = CKR_DEVICE_MEMORY;
          break;
        }
        /* Store information in session structure for later use */
        KMS_GETSESSION(hSession).hKey = hKey;
        KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
        KMS_GETSESSION(hSession).pCtx = p_ctx;
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_RSA && (KMS_RSA & KMS_FCT_SIGN | KMS_RSA & KMS_FCT_VERIFY)*/

#if defined(KMS_ECDSA) && ((KMS_ECDSA & KMS_FCT_SIGN) | (KMS_ECDSA & KMS_FCT_VERIFY))
    case CKM_ECDSA:
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
    case CKM_ECDSA_SHA1:
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
    case CKM_ECDSA_SHA256:
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
    {
      kms_obj_keyhead_t *pkms_object;
      kms_ecdsa_sv_ctx_t *p_ctx;

      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* Allocate context */
        p_ctx = KMS_Alloc(hSession, sizeof(kms_ecdsa_sv_ctx_t));
        if (p_ctx == NULL_PTR)
        {
          e_ret_status = CKR_DEVICE_MEMORY;
          break;
        }
        /* Store information in session structure for later use */
        KMS_GETSESSION(hSession).hKey = hKey;
        KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
        KMS_GETSESSION(hSession).pCtx = p_ctx;
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_ECDSA && (KMS_ECDSA & KMS_FCT_SIGN | KMS_ECDSA & KMS_FCT_VERIFY)*/

#if defined(KMS_AES_CMAC) && ((KMS_AES_CMAC & KMS_FCT_SIGN) | (KMS_AES_CMAC & KMS_FCT_VERIFY))
    case CKM_AES_CMAC_GENERAL:
    case CKM_AES_CMAC:
    {
      kms_obj_keyhead_t *pkms_object;
      kms_attr_t *P_pKeyAttribute;
      kms_aes_cmac_sv_ctx_t *p_ctx;

      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* Search for the Key Value to use */
        e_ret_status = KMS_Objects_SearchAttributes(CKA_VALUE, pkms_object, &P_pKeyAttribute);

        if (e_ret_status == CKR_OK)
        {
          /* Set key size with value from attribute  */
          if ((P_pKeyAttribute->size == CA_CRL_AES128_KEY) ||     /* 128 bits */
              (P_pKeyAttribute->size == CA_CRL_AES192_KEY) ||     /* 192 bits */
              (P_pKeyAttribute->size == CA_CRL_AES256_KEY))       /* 256 bits */
          {
            /* Allocate context */
            p_ctx = KMS_Alloc(hSession, sizeof(kms_aes_cmac_sv_ctx_t));
            if (p_ctx == NULL_PTR)
            {
              e_ret_status = CKR_DEVICE_MEMORY;
              break;
            }
            /* Store information in session structure for later use */
            KMS_GETSESSION(hSession).hKey = hKey;
            KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
            KMS_GETSESSION(hSession).pCtx = p_ctx;

            /* Check parameters & retrieve tag length to compute */
            if ((pMechanism->mechanism == CKM_AES_CMAC_GENERAL)
                && (pMechanism->pParameter != NULL_PTR))
            {
              /* Tag length is specified by the parameter */
              p_ctx->tagLength = *(CK_ULONG_PTR)pMechanism->pParameter;
            }
            else if ((pMechanism->mechanism == CKM_AES_CMAC_GENERAL)
                     && (pMechanism->pParameter == NULL_PTR))
            {
              /* Specified parameter is wrong */
              KMS_Free(hSession, p_ctx);
              KMS_GETSESSION(hSession).pCtx = NULL_PTR;
              e_ret_status = CKR_ARGUMENTS_BAD;
              break;
            }
            else
            {
              /* Tag length not in parameter, use AES block size */
              p_ctx->tagLength = CA_CRL_AES_BLOCK;
            }

            /* Set flag field to default value */
            p_ctx->ca_ctx.mFlags = CA_E_SK_DEFAULT;

            /* Set key size  */
            p_ctx->ca_ctx.mKeySize = (int32_t)P_pKeyAttribute->size ;
            /* Read value from the structure. Need to be translated from
              (uint32_t*) to (uint8_t *) */
            KMS_Objects_BlobU32_2_u8ptr(&(P_pKeyAttribute->data[0]), P_pKeyAttribute->size, p_ctx->key);
            /* Fill in crypto library context parameters */
            p_ctx->ca_ctx.pmKey = p_ctx->key;
            p_ctx->ca_ctx.mTagSize = (int32_t)p_ctx->tagLength;
            p_ctx->ca_ctx.pmTag = p_ctx->tag;

            /* Initialize the operation, by passing the context */
#if (KMS_AES_CMAC & KMS_FCT_SIGN)
            if (sigver_flag == KMS_FLAG_SIGN)
            {
              if (CA_AES_CMAC_Encrypt_Init(&p_ctx->ca_ctx) == CA_AES_SUCCESS)
              {
                e_ret_status = CKR_OK;
              }
              else
              {
                /* Free allocated context before returning error */
                KMS_Free(hSession, p_ctx);
                KMS_GETSESSION(hSession).pCtx = NULL_PTR;
                e_ret_status = CKR_FUNCTION_FAILED;
              }
            }
#endif /* KMS_AES_CMAC & KMS_FCT_SIGN */
#if (KMS_AES_CMAC & KMS_FCT_VERIFY)
            if (sigver_flag == KMS_FLAG_VERIFY)
            {
              if (CA_AES_CMAC_Decrypt_Init(&p_ctx->ca_ctx) == CA_AES_SUCCESS)
              {
                e_ret_status = CKR_OK;
              }
              else
              {
                /* Free allocated context before returning error */
                KMS_Free(hSession, p_ctx);
                KMS_GETSESSION(hSession).pCtx = NULL_PTR;
                e_ret_status = CKR_FUNCTION_FAILED;
              }
            }
#endif /* KMS_AES_CMAC & KMS_FCT_VERIFY */
          }
          else
          {
            /* Unsupported value for P_pKeyAttribute->size */
            e_ret_status = CKR_ARGUMENTS_BAD;
          }
        }
      }
      else
      {
        /* Can not retrieve proper key handle */
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* KMS_AES_CMAC && (KMS_AES_CMAC & KMS_FCT_SIGN | KMS_AES_CMAC & KMS_FCT_VERIFY)*/

    default:
      break;
  }

  return e_ret_status;
}
#endif /* KMS_SIGN || KMS_VERIFY */
/**
  * @}
  */

/* Exported function ---------------------------------------------------------*/
/** @addtogroup KMS_SIGN_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is called upon @ref C_SignInit call
  * @note   Refer to @ref C_SignInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pMechanism signature mechanism
  * @param  hKey signature key
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         @ref sign_verify_init returned values
  */
CK_RV     KMS_SignInit(CK_SESSION_HANDLE hSession,
                       CK_MECHANISM_PTR  pMechanism,
                       CK_OBJECT_HANDLE  hKey)
{
#if defined(KMS_SIGN)
  CK_RV e_ret_status = sign_verify_init(hSession, pMechanism, hKey, KMS_FLAG_SIGN);

  if (e_ret_status == CKR_OK)
  {
    /* If successful, set processing state of the session */
    KMS_GETSESSION(hSession).state = KMS_SESSION_SIGN;
  }

  return e_ret_status;
#else /* KMS_SIGN */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SIGN */
}

/**
  * @brief  This function is called upon @ref C_Sign call
  * @note   Refer to @ref C_Sign function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pData data to sign
  * @param  ulDataLen data to sign length
  * @param  pSignature signature
  * @param  pulSignatureLen signature length
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_KEY_SIZE_RANGE
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV     KMS_Sign(CK_SESSION_HANDLE hSession,         /* the session's handle */
                   CK_BYTE_PTR       pData,           /* the data to sign */
                   CK_ULONG          ulDataLen,       /* count of bytes to sign */
                   CK_BYTE_PTR       pSignature,      /* gets the signature */
                   CK_ULONG_PTR      pulSignatureLen)  /* gets signature length */
{
#if defined(KMS_SIGN)
  CK_RV e_ret_status = CKR_OK;
  kms_obj_keyhead_t *pkms_object;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* No processing already on going  */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_SIGN)
  {
    return CKR_OPERATION_NOT_INITIALIZED;
  }

  /* If a digest has to be computed */
  switch (KMS_GETSESSION(hSession).Mechanism)
  {
#if (defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)) || (defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN))
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)
    case CKM_RSA_PKCS:
#endif /* KMS_RSA & KMS_FCT_SIGN */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN)
    case CKM_ECDSA:
#endif /* KMS_ECDSA & KMS_FCT_SIGN */
    {
      /* Retrieve the allocated context */
      kms_asym_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;
      /* Check parameter */
      if (ulDataLen == 0UL)
      {
        e_ret_status = CKR_ARGUMENTS_BAD;
        break;
      }
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
      else if (ulDataLen == CA_CRL_SHA1_SIZE)
      {
        /* Deduce hash method from provided hash length */
        p_ctx->hashMethod = CA_E_SHA1;
        /* Copy provided hash into context for later processing */
        (void)memcpy(p_ctx->hash, pData, CA_CRL_SHA1_SIZE);
        p_ctx->hashSize = (int32_t)CA_CRL_SHA1_SIZE;
        e_ret_status = CKR_OK;
      }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
      else if (ulDataLen == CA_CRL_SHA256_SIZE)
      {
        /* Deduce hash method from provided hash length */
        p_ctx->hashMethod = CA_E_SHA256;
        /* Copy provided hash into context for later processing */
        (void)memcpy(p_ctx->hash, pData, CA_CRL_SHA256_SIZE);
        p_ctx->hashSize = (int32_t)CA_CRL_SHA256_SIZE;
        e_ret_status = CKR_OK;
      }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
      else
      {
        /* Parameter does not correspond to a supported hash method */
        e_ret_status = CKR_ARGUMENTS_BAD;
        break;
      }
      break;
    }
#endif /* KMS_RSA || KMS_ECDSA */

#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST) \
    && ((defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)) \
     || (defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN)))
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)
    case CKM_SHA1_RSA_PKCS:
#endif /* KMS_RSA & KMS_FCT_SIGN */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN)
    case CKM_ECDSA_SHA1:
#endif /* KMS_ECDSA & KMS_FCT_SIGN */
    {
      CA_SHA1ctx_stt HASH_ctxt_st;
      /* Retrieve the allocated context */
      kms_asym_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Hash method is specified by mechanism */
      p_ctx->hashMethod = CA_E_SHA1;
      /* Fill in crypto library context parameters */
      HASH_ctxt_st.mFlags = CA_E_HASH_DEFAULT;
      HASH_ctxt_st.mTagSize = (int32_t)CA_CRL_SHA1_SIZE;

      /* Compute hash */
      if (CA_SHA1_Init(&HASH_ctxt_st) == CA_AES_SUCCESS)
      {
        if (CA_SHA1_Append(&HASH_ctxt_st, pData, (int32_t)ulDataLen) == CA_AES_SUCCESS)
        {
          if (CA_SHA1_Finish(&HASH_ctxt_st, p_ctx->hash, &(p_ctx->hashSize)) == CA_AES_SUCCESS)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            /* Hash finalization failed */
            e_ret_status = CKR_FUNCTION_FAILED;
          }
        }
        else
        {
          /* Hash processing failed */
          e_ret_status = CKR_FUNCTION_FAILED;
        }
      }
      else
      {
        /* Hash process init failed */
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST && (KMS_RSA & KMS_FCT_SIGN || KMS_ECDSA & KMS_FCT_SIGN) */

#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST) \
    && ((defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)) \
     || (defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN)))
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)
    case CKM_SHA256_RSA_PKCS:
#endif /* KMS_RSA & KMS_FCT_SIGN */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN)
    case CKM_ECDSA_SHA256:
#endif /* KMS_ECDSA & KMS_FCT_SIGN */
    {
      CA_SHA256ctx_stt HASH_ctxt_st;
      /* Retrieve the allocated context */
      kms_asym_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Hash method is specified by mechanism */
      p_ctx->hashMethod = CA_E_SHA256;
      /* Fill in crypto library context parameters */
      HASH_ctxt_st.mFlags = CA_E_HASH_DEFAULT;
      HASH_ctxt_st.mTagSize = (int32_t)CA_CRL_SHA256_SIZE;

      /* Compute hash */
      if (CA_SHA256_Init(&HASH_ctxt_st) == CA_AES_SUCCESS)
      {
        if (CA_SHA256_Append(&HASH_ctxt_st, pData, (int32_t)ulDataLen) == CA_AES_SUCCESS)
        {
          if (CA_SHA256_Finish(&HASH_ctxt_st, p_ctx->hash, &(p_ctx->hashSize)) == CA_AES_SUCCESS)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            /* Hash finalization failed */
            e_ret_status = CKR_FUNCTION_FAILED;
          }
        }
        else
        {
          /* Hash processing failed */
          e_ret_status = CKR_FUNCTION_FAILED;
        }
      }
      else
      {
        /* Hash process init failed */
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST && (KMS_RSA & KMS_FCT_SIGN || KMS_ECDSA & KMS_FCT_SIGN) */

#if defined(KMS_AES_CMAC) && (KMS_AES_CMAC & KMS_FCT_SIGN)
    case CKM_AES_CMAC_GENERAL:
    case CKM_AES_CMAC:
    {
      /* No digest computing, full data buffer is signed */
      e_ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_CMAC & KMS_FCT_SIGN */

    default:
    {
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
    }
  }

  if (e_ret_status == CKR_OK)
  {
    /* Read the key value from the Key Handle                 */
    /* Key Handle is the index to one of static or nvm        */
    pkms_object = KMS_Objects_GetPointer(KMS_GETSESSION(hSession).hKey);

    /* Check that hKey is valid:
     * - NULL_PTR value means not found key handle
     * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
     */
    if ((pkms_object != NULL) &&
        (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
        (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
    {
      switch (KMS_GETSESSION(hSession).Mechanism)
      {
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_SIGN)
        case CKM_RSA_PKCS:
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
        case CKM_SHA1_RSA_PKCS:
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
        case CKM_SHA256_RSA_PKCS:
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
        {
          /* Retrieve the allocated context */
          kms_rsa_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;
          kms_attr_t *pAttr;
          CA_membuf_stt mb_st;
          CA_RSAprivKey_stt PrivKey_st;

          /* Retrieve the RSA key private exponent */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_PRIVATE_EXPONENT, pkms_object, &pAttr);

          if (e_ret_status != CKR_OK)
          {
            /* Private exponent not found */
            e_ret_status = CKR_MECHANISM_PARAM_INVALID;
            break;
          }
          if (pAttr->size > sizeof(p_ctx->privExp))
          {
            /* Private exponent size is not supported */
            e_ret_status = CKR_KEY_SIZE_RANGE;
            break;
          }
          PrivKey_st.mExponentSize = (int32_t)pAttr->size;

          /* Read value from the structure. Need to be translated from
           (uint32_t*) to (uint8_t *) */
          KMS_Objects_BlobU32_2_u8ptr(&(pAttr->data[0]), pAttr->size, p_ctx->privExp);
          /* Set private exponent into crypto library context */
          PrivKey_st.pmExponent = (uint8_t *)p_ctx->privExp;

#if defined(CA_RSA_ADD_PUBEXP_IN_PRIVATEKEY)
          /* Retrieve the RSA key public exponent */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_PUBLIC_EXPONENT, pkms_object, &pAttr);
          if (e_ret_status != CKR_OK)
          {
            /* Public exponent not found */
            e_ret_status = CKR_MECHANISM_PARAM_INVALID;
            break;
          }
          if (pAttr->size > sizeof(p_ctx->pubExp))
          {
            /* Public exponent size is not supported */
            e_ret_status = CKR_KEY_SIZE_RANGE;
            break;
          }
          PrivKey_st.mPubExponentSize = (int32_t)pAttr->size;

          /* Read value from the structure. Need to be translated from
             (uint32_t*) to (uint8_t *) */
          KMS_Objects_BlobU32_2_u8ptr(&(pAttr->data[0]), pAttr->size, p_ctx->pubExp);
          /* Set public exponent into crypto library context */
          PrivKey_st.pmPubExponent = (uint8_t *)p_ctx->pubExp;
#endif /* (CA_RSA_ADD_PUBEXP_IN_PRIVATEKEY) */
          /* Retrieve the RSA key modulus */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_MODULUS, pkms_object, &pAttr);

          if (e_ret_status != CKR_OK)
          {
            /* Modulus not found */
            e_ret_status = CKR_MECHANISM_PARAM_INVALID;
            break;
          }
          if (pAttr->size > sizeof(p_ctx->modulus))
          {
            /* Modulus size is not supported */
            e_ret_status = CKR_KEY_SIZE_RANGE;
            break;
          }

          PrivKey_st.mModulusSize = (int32_t)pAttr->size;

          KMS_CHECK_BUFFER_SECTION5_2(pSignature, pulSignatureLen, (uint32_t)PrivKey_st.mModulusSize);

          /* Read value from the structure. Need to be translated from
            (uint32_t*) to (uint8_t *) */
          KMS_Objects_BlobU32_2_u8ptr(&(pAttr->data[0]), pAttr->size, p_ctx->modulus);
          /* Set modulus into crypto library context */
          PrivKey_st.pmModulus = (uint8_t *)p_ctx->modulus;

          /* Initialize the membuf_st that must be passed to the RSA functions */
          mb_st.mSize = (int32_t)sizeof(p_ctx->tmpbuffer);
          mb_st.mUsed = 0;
          mb_st.pmBuf = p_ctx->tmpbuffer;

          /* Sign with RSA */
          if (CA_RSA_PKCS1v15_Sign(&(PrivKey_st), p_ctx->hash, p_ctx->hashMethod, pSignature, &mb_st) == CA_RSA_SUCCESS)
          {
            /* Update generated signature length to upper layer */
            *pulSignatureLen = (uint32_t)PrivKey_st.mModulusSize;
            e_ret_status = CKR_OK;
          }
          else
          {
            e_ret_status = CKR_FUNCTION_FAILED;
          }
          break;
        }
#endif /* KMS_RSA & KMS_FCT_SIGN */

#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_SIGN)
        case CKM_ECDSA:
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
        case CKM_ECDSA_SHA1:
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
        case CKM_ECDSA_SHA256:
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
        {
          /* ECDSA signature not supported */
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
#endif /* KMS_ECDSA & KMS_FCT_SIGN */

#if defined(KMS_AES_CMAC) && (KMS_AES_CMAC & KMS_FCT_SIGN)
        case CKM_AES_CMAC_GENERAL:
        case CKM_AES_CMAC:
        {
          kms_aes_cmac_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

          /* Specify last packet used */
          p_ctx->ca_ctx.mFlags |= CA_E_SK_FINAL_APPEND;
          /* Encrypt Data */
          if (CA_AES_CMAC_Encrypt_Append(&(p_ctx->ca_ctx),
                                         pData,
                                         (int32_t)ulDataLen) == CA_AES_SUCCESS)
          {
            if (CA_AES_CMAC_Encrypt_Finish(&(p_ctx->ca_ctx),
                                           pSignature,
                                           (int32_t *)(uint32_t)&pulSignatureLen) == CA_AES_SUCCESS)
            {
              e_ret_status = CKR_OK;
            }
            else
            {
              /* Finalization failed */
              e_ret_status = CKR_FUNCTION_FAILED;
            }
          }
          else
          {
            /* Data processing failed */
            e_ret_status = CKR_FUNCTION_FAILED;
          }
          break;
        }
#endif /* KMS_AES_CMAC & KMS_FCT_SIGN */

        default:
        {
          e_ret_status = CKR_MECHANISM_INVALID;
          break;
        }
      }
    }
    else
    {
      /* No valid object pointer found for the requested key handle */
      e_ret_status = CKR_OBJECT_HANDLE_INVALID;
    }
  }

  /* Upon completion error or not:
   * - free the allocated context
   * - release the session
   */
  KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }

  KMS_SetStateIdle(hSession);

  return e_ret_status;
#else /* KMS_SIGN */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SIGN */
}

/**
  * @brief  This function is called upon @ref C_VerifyInit call
  * @note   Refer to @ref C_VerifyInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pMechanism verification mechanism
  * @param  hKey verification key
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         @ref sign_verify_init returned values
  */
CK_RV        KMS_VerifyInit(CK_SESSION_HANDLE hSession,
                            CK_MECHANISM_PTR  pMechanism,
                            CK_OBJECT_HANDLE  hKey)
{
#if defined(KMS_VERIFY)
  CK_RV e_ret_status = sign_verify_init(hSession, pMechanism, hKey, KMS_FLAG_VERIFY);

  if (e_ret_status == CKR_OK)
  {
    /* If successful, set processing state of the session */
    KMS_GETSESSION(hSession).state = KMS_SESSION_VERIFY;
  }

  return e_ret_status;
#else /* KMS_VERIFY */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_VERIFY */
}

/**
  * @brief  This function is called upon @ref C_Verify call
  * @note   Refer to @ref C_Verify function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pData data to verify
  * @param  ulDataLen data to verify length
  * @param  pSignature signature
  * @param  ulSignatureLen signature length
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DATA_INVALID
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_KEY_SIZE_RANGE
  *         CKR_OBJECT_HANDLE_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  *         CKR_SIGNATURE_LEN_RANGE
  *         CKR_SIGNATURE_INVALID
  */
CK_RV  KMS_Verify(CK_SESSION_HANDLE hSession,         /* the session's handle */
                  CK_BYTE_PTR       pData,           /* signed data */
                  CK_ULONG          ulDataLen,       /* length of signed data */
                  CK_BYTE_PTR       pSignature,      /* signature */
                  CK_ULONG          ulSignatureLen)  /* signature length */

{
#if defined(KMS_VERIFY)
  CK_RV    e_ret_status ;
  kms_obj_keyhead_t *pkms_object;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* No processing already on going  */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_VERIFY)
  {
    return CKR_OPERATION_NOT_INITIALIZED;
  }

  /* If a digest has to be computed */
  switch (KMS_GETSESSION(hSession).Mechanism)
  {
#if (defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)) || (defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY))
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)
    case CKM_RSA_PKCS:
#endif /* KMS_RSA & KMS_FCT_VERIFY */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY)
    case CKM_ECDSA:
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
    {
      /* Retrieve the allocated context */
      kms_asym_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Check parameter */
      if (ulDataLen == 0UL)
      {
        e_ret_status = CKR_ARGUMENTS_BAD;
        break;
      }
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
      else if (ulDataLen == CA_CRL_SHA1_SIZE)
      {
        /* Deduce hash method from provided hash length */
        p_ctx->hashMethod = CA_E_SHA1;
        /* Copy provided hash into context for later processing */
        (void)memcpy(p_ctx->hash, pData, CA_CRL_SHA1_SIZE);
        p_ctx->hashSize = (int32_t)CA_CRL_SHA1_SIZE;
        e_ret_status = CKR_OK;
      }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
      else if (ulDataLen == CA_CRL_SHA256_SIZE)
      {
        /* Deduce hash method from provided hash length */
        p_ctx->hashMethod = CA_E_SHA256;
        /* Copy provided hash into context for later processing */
        (void)memcpy(p_ctx->hash, pData, CA_CRL_SHA256_SIZE);
        p_ctx->hashSize = (int32_t)CA_CRL_SHA256_SIZE;
        e_ret_status = CKR_OK;
      }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
      else
      {
        /* Parameter does not correspond to a supported hash method */
        e_ret_status = CKR_ARGUMENTS_BAD;
        break;
      }
      break;
    }
#endif /* KMS_RSA & KMS_FCT_VERIFY || KMS_ECDSA & KMS_FCT_VERIFY */

#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST) \
    && ((defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)) \
     || (defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY)))
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)
    case CKM_SHA1_RSA_PKCS:
#endif /* KMS_RSA & KMS_FCT_VERIFY */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY)
    case CKM_ECDSA_SHA1:
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
    {
      CA_SHA1ctx_stt HASH_ctxt_st;
      /* Retrieve the allocated context */
      kms_asym_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Hash method is specified by mechanism */
      p_ctx->hashMethod = CA_E_SHA1;
      /* Fill in crypto library context parameters */
      HASH_ctxt_st.mFlags = CA_E_HASH_DEFAULT;
      HASH_ctxt_st.mTagSize = (int32_t)CA_CRL_SHA1_SIZE;

      /* Compute hash */
      if (CA_SHA1_Init(&HASH_ctxt_st) == CA_AES_SUCCESS)
      {
        if (CA_SHA1_Append(&HASH_ctxt_st, pData, (int32_t)ulDataLen) == CA_AES_SUCCESS)
        {
          if (CA_SHA1_Finish(&HASH_ctxt_st, p_ctx->hash, &(p_ctx->hashSize)) == CA_AES_SUCCESS)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            /* Hash finalization failed */
            e_ret_status = CKR_FUNCTION_FAILED;
          }
        }
        else
        {
          /* Hash processing failed */
          e_ret_status = CKR_FUNCTION_FAILED;
        }
      }
      else
      {
        /* Hash process init failed */
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_SHA1 & KMS_FCT_DIGEST && (KMS_RSA & KMS_FCT_VERIFY || KMS_ECDSA & KMS_FCT_VERIFY) */

#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST) \
    && ((defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)) \
     || (defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY)))
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)
    case CKM_SHA256_RSA_PKCS:
#endif /* KMS_RSA & KMS_FCT_VERIFY */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY)
    case CKM_ECDSA_SHA256:
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
    {
      CA_SHA256ctx_stt HASH_ctxt_st;
      /* Retrieve the allocated context */
      kms_asym_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Hash method is specified by mechanism */
      p_ctx->hashMethod = CA_E_SHA256;
      /* Fill in crypto library context parameters */
      HASH_ctxt_st.mFlags = CA_E_HASH_DEFAULT;
      HASH_ctxt_st.mTagSize = (int32_t)CA_CRL_SHA256_SIZE;

      /* Compute hash */
      if (CA_SHA256_Init(&HASH_ctxt_st) == CA_AES_SUCCESS)
      {
        if (CA_SHA256_Append(&HASH_ctxt_st, pData, (int32_t)ulDataLen) == CA_AES_SUCCESS)
        {
          if (CA_SHA256_Finish(&HASH_ctxt_st, p_ctx->hash, &(p_ctx->hashSize)) == CA_AES_SUCCESS)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            /* Hash finalization failed */
            e_ret_status = CKR_FUNCTION_FAILED;
          }
        }
        else
        {
          /* Hash processing failed */
          e_ret_status = CKR_FUNCTION_FAILED;
        }
      }
      else
      {
        /* Hash process init failed */
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_SHA256 & KMS_FCT_DIGEST && (KMS_RSA & KMS_FCT_VERIFY || KMS_ECDSA & KMS_FCT_VERIFY) */

#if defined(KMS_AES_CMAC) && (KMS_AES_CMAC & KMS_FCT_VERIFY)
    case CKM_AES_CMAC_GENERAL:
    case CKM_AES_CMAC:
    {
      /* No digest computing, full data buffer is signed */
      e_ret_status = CKR_OK;
      break;
    }
#endif /* KMS_AES_CMAC & KMS_FCT_VERIFY */

    default:
    {
      e_ret_status = CKR_FUNCTION_FAILED;
      break;
    }
  }

  if (e_ret_status == CKR_OK)
  {
    /* Read the key value from the Key Handle                 */
    /* Key Handle is the index to one of static or nvm        */
    pkms_object = KMS_Objects_GetPointer(KMS_GETSESSION(hSession).hKey);

    /* Check that hKey is valid:
     * - NULL_PTR value means not found key handle
     * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
     */
    if ((pkms_object != NULL) &&
        (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
        (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
    {

      switch (KMS_GETSESSION(hSession).Mechanism)
      {
#if defined(KMS_RSA) && (KMS_RSA & KMS_FCT_VERIFY)
        case CKM_RSA_PKCS:
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
        case CKM_SHA1_RSA_PKCS:
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
        case CKM_SHA256_RSA_PKCS:
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
        {
          /* Retrieve the allocated context */
          kms_rsa_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;
          kms_attr_t *pAttr;
          CA_membuf_stt mb_st;
          CA_RSApubKey_stt PubKey_st;

          /* Retrieve the RSA key public exponent */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_PUBLIC_EXPONENT, pkms_object, &pAttr);

          if (e_ret_status != CKR_OK)
          {
            /* Public exponent not found */
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }
          if (pAttr->size > sizeof(p_ctx->pubExp))
          {
            /* Public exponent size is not supported */
            e_ret_status = CKR_KEY_SIZE_RANGE;
            break;
          }
          PubKey_st.mExponentSize = (int32_t)pAttr->size;

          /* Read value from the structure. Need to be translated from
             (uint32_t*) to (uint8_t *) */
          KMS_Objects_BlobU32_2_u8ptr(&(pAttr->data[0]), pAttr->size, p_ctx->pubExp);
          /* Set public exponent into crypto library context */
          PubKey_st.pmExponent = (uint8_t *)p_ctx->pubExp;

          /* Retrieve the RSA key modulus */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_MODULUS, pkms_object, &pAttr);

          if (e_ret_status != CKR_OK)
          {
            /* Modulus not found */
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }
          if (pAttr->size > sizeof(p_ctx->modulus))
          {
            /* Modulus size is not supported */
            e_ret_status = CKR_KEY_SIZE_RANGE;
            break;
          }
          if (pAttr->size != ulSignatureLen)
          {
            /* Provided signature length does not correspond to key size */
            e_ret_status = CKR_SIGNATURE_LEN_RANGE ;
            break;
          }

          PubKey_st.mModulusSize = (int32_t)pAttr->size;

          /* Read value from the structure. Need to be translated from
            (uint32_t*) to (uint8_t *) */
          KMS_Objects_BlobU32_2_u8ptr(&(pAttr->data[0]), pAttr->size, p_ctx->modulus);
          /* Set modulus into crypto library context */
          PubKey_st.pmModulus = (uint8_t *)p_ctx->modulus;

          /* Initialize the membuf_st that must be passed to the RSA functions */
          mb_st.mSize = (int32_t)sizeof(p_ctx->tmpbuffer);
          mb_st.mUsed = 0;
          mb_st.pmBuf = p_ctx->tmpbuffer;

          /* Verify with RSA */
          if (CA_RSA_PKCS1v15_Verify(&(PubKey_st),
                                     p_ctx->hash,
                                     p_ctx->hashMethod,
                                     pSignature,
                                     &mb_st)
              == CA_SIGNATURE_VALID)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            e_ret_status = CKR_SIGNATURE_INVALID;
          }
          break;
        }
#endif /* KMS_RSA & KMS_FCT_VERIFY */

#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_VERIFY)
        case CKM_ECDSA:
#if defined(KMS_SHA1) && (KMS_SHA1 & KMS_FCT_DIGEST)
        case CKM_ECDSA_SHA1:
#endif /* KMS_SHA1 & KMS_FCT_DIGEST */
#if defined(KMS_SHA256) && (KMS_SHA256 & KMS_FCT_DIGEST)
        case CKM_ECDSA_SHA256:
#endif /* KMS_SHA256 & KMS_FCT_DIGEST */
        {
          /* Retrieve the allocated context */
          kms_ecdsa_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;
          kms_attr_t *pAttr;
          CA_membuf_stt mb_st;
          CA_EC_stt  EC_st;
          CA_ECpoint_stt *PubKey = NULL;
          CA_ECDSAsignature_stt *p_sign = NULL;
          CA_ECDSAverifyCtx_stt verctx;

          /* The CKA_EC_PARAMS attribute specify the Curve to use for the verification */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_EC_PARAMS, pkms_object, &pAttr);
          if (e_ret_status != CKR_OK)
          {
            /* CKA_EC_PARAMS attribute not found */
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }

          /* Load the Elliptic Curve defined in the Object by CKA_EC_PARAMS */
          e_ret_status = KMS_ECC_LoadCurve(pAttr, &EC_st);
          if (e_ret_status != CKR_OK)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }
          /* We prepare the memory buffer strucure */
          mb_st.pmBuf = p_ctx->tmpbuffer;
          mb_st.mUsed = 0;
          mb_st.mSize = (int16_t)sizeof(p_ctx->tmpbuffer);

          /* Init the Elliptic Curve, passing the required memory */
          /* Note: This is not a temporary buffer, the memory inside EC_stt_buf is linked to EC_st *
              As long as there's need to use EC_st the content of EC_stt_buf should not be altered */
          if (CA_ECCinitEC(&EC_st, &mb_st) != CA_ECC_SUCCESS)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }

          /* EC is initialized, now prepare to read the public key. First, allocate a point */
          if (CA_ECCinitPoint(&PubKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }

          /* We read the Public Key value from Object */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_EC_POINT, pkms_object, &pAttr);
          if (e_ret_status != CKR_OK)
          {
            /* CKA_EC_POINT attribute not found */
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }

          /* Here, the curve is loaded in EC_st */
          p_ctx->pub_size = (uint32_t)EC_st.mNsize;

          if (pAttr->size > sizeof(p_ctx->der_pub))
          {
            /* CKA_EC_POINT attribute size is not supported */
            e_ret_status = CKR_DATA_INVALID;
            break;
          }
          /* Read value from the structure. Need to be translated from
              (uint32_t*) to (uint8_t *) */
          KMS_Objects_BlobU32_2_u8ptr(&(pAttr->data[0]), pAttr->size, p_ctx->der_pub);

          /* We expect an EC Point in DER format uncompressed:
           * extract X & Y coordinates from it */
          if (KMS_DerX962_ExtractPublicKeyCoord(p_ctx->der_pub,
                                                &(p_ctx->pub_x[0]),
                                                &(p_ctx->pub_y[0]),
                                                (uint32_t)p_ctx->pub_size)
              != CKR_OK)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }

          /* Point is initialized, now import the public key */
          (void)CA_ECCsetPointCoordinate(PubKey, CA_E_ECC_POINT_COORDINATE_X, p_ctx->pub_x, (int32_t)(p_ctx->pub_size));
          (void)CA_ECCsetPointCoordinate(PubKey, CA_E_ECC_POINT_COORDINATE_Y, p_ctx->pub_y, (int32_t)(p_ctx->pub_size));

          /* Try to validate the Public Key  */
          if (CA_ECCvalidatePubKey(PubKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }
          /* Public Key is validated, Initialize the signature object */
          if (CA_ECDSAinitSign(&p_sign, &EC_st, &mb_st) != CA_ECC_SUCCESS)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }

          /* Prepare the signature values to crypto library format */
          (void)CA_ECDSAsetSignature(p_sign,
                                     CA_E_ECDSA_SIGNATURE_R_VALUE,
                                     pSignature,
                                     (int32_t)(p_ctx->pub_size));
          (void)CA_ECDSAsetSignature(p_sign,
                                     CA_E_ECDSA_SIGNATURE_S_VALUE,
                                     &pSignature[p_ctx->pub_size],
                                     (int32_t)(p_ctx->pub_size));

          /* Prepare the structure for the ECDSA signature verification */
          verctx.pmEC = &EC_st;
          verctx.pmPubKey = PubKey;

          /* Verify it */
          if (CA_ECDSAverify(p_ctx->hash, p_ctx->hashSize, p_sign, &verctx, &mb_st) == CA_SIGNATURE_VALID)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            e_ret_status = CKR_SIGNATURE_INVALID;
          }

          /* release resource ...*/
          (void)CA_ECDSAfreeSign(&p_sign, &mb_st);
          (void)CA_ECCfreePoint(&PubKey, &mb_st);
          (void)CA_ECCfreeEC(&EC_st, &mb_st);
          break;
        }
#endif /* KMS_ECDSA & KMS_FCT_VERIFY */
#if defined(KMS_AES_CMAC) && (KMS_AES_CMAC & KMS_FCT_VERIFY)
        case CKM_AES_CMAC_GENERAL:
        case CKM_AES_CMAC:
        {
          /* Retrieve the allocated context */
          kms_aes_cmac_sv_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

          /* Specify last packet used */
          p_ctx->ca_ctx.mFlags |= CA_E_SK_FINAL_APPEND;
          /* Encrypt Data */
          if (CA_AES_CMAC_Decrypt_Append(&(p_ctx->ca_ctx),
                                         pData,
                                         (int32_t)ulDataLen) != CA_HASH_SUCCESS)
          {
            e_ret_status = CKR_FUNCTION_FAILED;
            break;
          }
          /* Set TAG in context for verification */
          p_ctx->ca_ctx.mTagSize = (int32_t)ulSignatureLen;
          p_ctx->ca_ctx.pmTag = pSignature;
          if (CA_AES_CMAC_Decrypt_Finish(&(p_ctx->ca_ctx),
                                         pSignature,
                                         (int32_t *)(uint32_t)&ulSignatureLen) == CA_AUTHENTICATION_SUCCESSFUL)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            e_ret_status = CKR_SIGNATURE_INVALID;
          }
          break;
        }
#endif /* KMS_AES_CMAC & KMS_FCT_VERIFY */
        default:
        {
          e_ret_status = CKR_FUNCTION_FAILED;
          break;
        }
      }
    }
    else
    {
      e_ret_status = CKR_OBJECT_HANDLE_INVALID;
    }
  }

  /* Upon completion error or not:
   * - free the allocated context
   * - release the session
   */
  KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }

  KMS_SetStateIdle(hSession);

  return e_ret_status;

#else /* KMS_VERIFY */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_VERIFY */
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
