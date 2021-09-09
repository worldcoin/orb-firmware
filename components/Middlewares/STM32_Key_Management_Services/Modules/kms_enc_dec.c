/**
  ******************************************************************************
  * @file    kms_enc_dec.c
  * @author  MCD Application Team
  * @brief   This file contains implementation for Key Management Services (KMS)
  *          module encryption & decryption functionalities.
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
#include "kms_enc_dec.h"        /* KMS encryption & decryption services */
#include "kms_objects.h"        /* KMS object management services */
#include "CryptoApi/ca.h"       /* Crypto API services */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_ENC_DEC Encryption & Decryption services
  * @{
  */

/* Private types -------------------------------------------------------------*/
/** @addtogroup KMS_ENC_DEC_Private_Types Private Types
  * @{
  */
#if defined(KMS_AES_CBC)
/**
  * @brief  AES CBC context structure
  */
typedef struct
{
  uint8_t key[CA_CRL_AES256_KEY];       /*!< Used to store key to use during processing */
  CA_AESCBCctx_stt ca_ctx;              /*!< Used to store crypto library context */
} kms_aes_cbc_ec_ctx_t;
#endif /* KMS_AES_CBC */
#if defined(KMS_AES_CCM)
/**
  * @brief  AES CCM context structure
  */
typedef struct
{
  uint8_t key[CA_CRL_AES256_KEY];       /*!< Used to store key to use during processing */
  uint8_t tag[CA_CRL_AES_BLOCK];        /*!< Used to store tag at the end of processing */
  uint32_t tagLength;                   /*!< Host stored tag length */
  uint32_t dataRemain;                  /*!< Host data to decrypt for the all processing */
  CA_AESCCMctx_stt ca_ctx;              /*!< Used to store crypto library context */
} kms_aes_ccm_ec_ctx_t;
#endif /* KMS_AES_CCM */
#if defined(KMS_AES_ECB)
/**
  * @brief  AES ECB context structure
  */
typedef struct
{
  uint8_t key[CA_CRL_AES256_KEY];       /*!< Used to store key to use during processing */
  CA_AESECBctx_stt ca_ctx;              /*!< Used to store crypto library context */
} kms_aes_ecb_ec_ctx_t;
#endif /* KMS_AES_ECB */
#if defined(KMS_AES_GCM)
/**
  * @brief  AES GCM context structure
  */
typedef struct
{
  uint8_t key[CA_CRL_AES256_KEY];       /*!< Used to store key to use during processing */
  CA_AESGCMctx_stt ca_ctx;              /*!< Used to store crypto library context */
} kms_aes_gcm_ec_ctx_t;
#endif /* KMS_AES_GCM */
/**
  * @}
  */
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup KMS_ENC_DEC_Private_Functions Private Functions
  * @{
  */
#define KMS_FLAG_ENCRYPT 0    /*!< Encryption requested */
#define KMS_FLAG_DECRYPT 1    /*!< Decryption requested */

#if defined(KMS_ENCRYPT) || defined(KMS_DECRYPT)
/* Common function used to process encryption & decryption initialization */
static CK_RV          encrypt_decrypt_init(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                                           CK_OBJECT_HANDLE hKey, int32_t encdec_flag);
#endif /* KMS_ENCRYPT || KMS_DECRYPT */

/**
  * @}
  */

/* Private function ----------------------------------------------------------*/

/** @addtogroup KMS_ENC_DEC_Private_Functions Private Functions
  * @{
  */

#if defined(KMS_ENCRYPT) || defined(KMS_DECRYPT) || defined(KMS_DERIVE_KEY)
/**
  * @brief  This function is called upon @ref C_EncryptInit or @ref C_DecryptInit call
  * @note   Refer to @ref C_EncryptInit or @ref C_DecryptInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @note   The CKA_ENCRYPT attribute of the encryption key, which indicates
  *         whether the key supports encryption, MUST be CK_TRUE
  * @param  hSession session handle
  * @param  pMechanism encryption or decryption mechanism
  * @param  hKey handle of the encryption or decryption key
  * @param  encdec_flag action to perform: encrypt or decrypt
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_FAILED
  *         CKR_MECHANISM_INVALID
  *         CKR_OBJECT_HANDLE_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  *         KMS_Objects_SearchAttributes
  */
static CK_RV encrypt_decrypt_init(CK_SESSION_HANDLE hSession,
                                  CK_MECHANISM_PTR pMechanism,
                                  CK_OBJECT_HANDLE hKey,
                                  int32_t encdec_flag)
{
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;

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
  /* Check parameter */
  if (pMechanism == NULL_PTR)
  {
    return CKR_ARGUMENTS_BAD;
  }
  switch (pMechanism->mechanism)
  {
#if defined(KMS_AES_CBC) && ((KMS_AES_CBC & KMS_FCT_ENCRYPT) | (KMS_AES_CBC & KMS_FCT_DECRYPT))
    case CKM_AES_CBC:
    {
      kms_obj_keyhead_t *pkms_object;
      kms_attr_t *P_pKeyAttribute;
      kms_aes_cbc_ec_ctx_t *p_ctx;

      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL_PTR) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* If IV is defined */
        if ((pMechanism->pParameter != NULL_PTR) &&
            (pMechanism->ulParameterLen != 0U))
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
              p_ctx = KMS_Alloc(hSession, sizeof(kms_aes_cbc_ec_ctx_t));
              if (p_ctx == NULL_PTR)
              {
                e_ret_status = CKR_DEVICE_MEMORY;
                break;
              }
              /* Store information in session structure for later use */
              KMS_GETSESSION(hSession).hKey = hKey;
              KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
              KMS_GETSESSION(hSession).pCtx = p_ctx;

              /* Set flag field to default value */
              p_ctx->ca_ctx.mFlags = CA_E_SK_DEFAULT;
              p_ctx->ca_ctx.mKeySize = (int32_t)P_pKeyAttribute->size ;
              p_ctx->ca_ctx.pmKey = p_ctx->key;

              /* Read value from the structure. Need to be translated from
              (uint32_t*) to (uint8_t *) */
              KMS_Objects_BlobU32_2_u8ptr(&(P_pKeyAttribute->data[0]), P_pKeyAttribute->size, p_ctx->key);

              /* Set iv size field with value passed through the MECHANISM */
              p_ctx->ca_ctx.mIvSize = (int32_t)pMechanism->ulParameterLen;

              /* Call the right init function */
              if (encdec_flag == KMS_FLAG_ENCRYPT)
              {
#if (KMS_AES_CBC & KMS_FCT_ENCRYPT)
                /* load the key and ivec, eventually performs key schedule, etc  */
                if (CA_AES_CBC_Encrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            pMechanism->pParameter) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_CBC & KMS_FCT_ENCRYPT */
              }
              else
              {
#if (KMS_AES_CBC & KMS_FCT_DECRYPT)
                /* load the key and ivec, eventually performs key schedule, etc  */
                if (CA_AES_CBC_Decrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            pMechanism->pParameter) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_CBC & KMS_FCT_DECRYPT */
              }
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
          /* No IV defined */
          e_ret_status = CKR_ARGUMENTS_BAD;
        }
      }
      else
      {
        /* Can not retrieve proper key handle */
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* (KMS_AES_CBC & KMS_FCT_ENCRYPT) | (KMS_AES_CBC & KMS_FCT_DECRYPT) */

#if defined(KMS_AES_CCM) && ((KMS_AES_CCM & KMS_FCT_ENCRYPT) | (KMS_AES_CCM & KMS_FCT_DECRYPT))
    case CKM_AES_CCM:
    {
      kms_obj_keyhead_t *pkms_object;
      kms_attr_t *P_pKeyAttribute;
      kms_aes_ccm_ec_ctx_t *p_ctx;
      CK_CCM_PARAMS *pCCMParams;

      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL_PTR) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* If IV is defined */
        if ((pMechanism->pParameter != NULL_PTR) &&
            (pMechanism->ulParameterLen != 0UL))
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
              p_ctx = KMS_Alloc(hSession, sizeof(kms_aes_ccm_ec_ctx_t));
              if (p_ctx == NULL_PTR)
              {
                e_ret_status = CKR_DEVICE_MEMORY;
                break;
              }
              /* Store information in session structure for later use */
              KMS_GETSESSION(hSession).hKey = hKey;
              KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
              KMS_GETSESSION(hSession).pCtx = p_ctx;

              /* Get pointer to parameters */
              pCCMParams = (CK_CCM_PARAMS *)pMechanism->pParameter;

              /* Set flag field to default value */
              p_ctx->ca_ctx.mFlags = CA_E_SK_DEFAULT;

              p_ctx->ca_ctx.mKeySize = (int32_t)P_pKeyAttribute->size;

              /* Read value from the structure. Need to be translated from
              (uint32_t*) to (uint8_t *) */
              KMS_Objects_BlobU32_2_u8ptr(&(P_pKeyAttribute->data[0]), P_pKeyAttribute->size, p_ctx->key);

              /* Set nonce size field to IvLength, note that valid values are 7,8,9,10,11,12,13*/
              p_ctx->ca_ctx.mNonceSize = (int32_t)pCCMParams->ulNonceLen;

              /* Point to the pNonce used as IV */
              p_ctx->ca_ctx.pmNonce = pCCMParams->pNonce;

              /* Size of returned authentication TAG */
              p_ctx->ca_ctx.mTagSize = (int32_t)pCCMParams->ulMACLen;

              /* Set the size of the header */
              p_ctx->ca_ctx.mAssDataSize = (int32_t)pCCMParams->ulAADLen;

              /* Set the size of thepayload */
              p_ctx->ca_ctx.mPayloadSize = (int32_t)pCCMParams->ulDataLen;

              /* Call the right init function */
              if (encdec_flag == KMS_FLAG_ENCRYPT)
              {
#if (KMS_AES_CCM & KMS_FCT_ENCRYPT)
                /* Initialize the operation, by passing the key and IV */
                if (CA_AES_CCM_Encrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            pCCMParams->pNonce) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_CCM & KMS_FCT_ENCRYPT */
              }
              else
              {
#if (KMS_AES_CCM & KMS_FCT_DECRYPT)
                /* Set remaining data to process in order to detect TAG within cyphertext */
                p_ctx->dataRemain = (uint32_t)p_ctx->ca_ctx.mPayloadSize;
                /* Initialize the operation, by passing the key and IV */
                if (CA_AES_CCM_Decrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            pCCMParams->pNonce) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_CCM & KMS_FCT_DECRYPT */
              }

              if ((e_ret_status == CKR_OK) && (pCCMParams->ulAADLen != 0UL))
              {
                /* If a header (or Additional Data is proposed ==> AAD from MECHANISM) */
                if (CA_AES_CCM_Header_Append(&(p_ctx->ca_ctx),
                                             pCCMParams->pAAD,
                                             (int32_t)pCCMParams->ulAADLen) == CA_AES_SUCCESS)
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
          /* No IV defined */
          e_ret_status = CKR_ARGUMENTS_BAD;
        }
      }
      else
      {
        /* Can not retrieve proper key handle */
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* (KMS_AES_CCM & KMS_FCT_ENCRYPT) | (KMS_AES_CCM & KMS_FCT_DECRYPT) */

#if defined(KMS_AES_ECB) && ((KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DECRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY))
    case CKM_AES_ECB:
    {
      kms_obj_keyhead_t *pkms_object;
      kms_attr_t *P_pKeyAttribute;
      kms_aes_ecb_ec_ctx_t *p_ctx;

      /* Read the key value from the Key Handle                 */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL_PTR) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* No parameter for ECB */
        if ((pMechanism->pParameter == NULL_PTR) &&
            (pMechanism->ulParameterLen == 0U))
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
              p_ctx = KMS_Alloc(hSession, sizeof(kms_aes_ecb_ec_ctx_t));
              if (p_ctx == NULL_PTR)
              {
                e_ret_status = CKR_DEVICE_MEMORY;
                break;
              }
              /* Store information in session structure for later use */
              KMS_GETSESSION(hSession).hKey = hKey;
              KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
              KMS_GETSESSION(hSession).pCtx = p_ctx;

              /* Set flag field to default value */
              p_ctx->ca_ctx.mFlags = CA_E_SK_DEFAULT;
              /* Set key size */
              p_ctx->ca_ctx.mKeySize = (int32_t)P_pKeyAttribute->size ;

              /* Read value from the structure. Need to be translated from
              (uint32_t*) to (uint8_t *) */
              KMS_Objects_BlobU32_2_u8ptr(&(P_pKeyAttribute->data[0]), P_pKeyAttribute->size, p_ctx->key);

              /* Call the right init function */
              if (encdec_flag == KMS_FLAG_ENCRYPT)
              {
#if ((KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY))
                /* load the key and ivec, eventually performs key schedule, etc  */
                if (CA_AES_ECB_Encrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            NULL_PTR) == CA_AES_SUCCESS)
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
#endif /* (KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY) */
              }
              else
              {
#if (KMS_AES_ECB & KMS_FCT_DECRYPT)
                /* load the key and ivec, eventually performs key schedule, etc  */
                if (CA_AES_ECB_Decrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            NULL_PTR) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_ECB & KMS_FCT_DECRYPT */
              }
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
          /* Parameter supplied but should not */
          e_ret_status = CKR_ARGUMENTS_BAD;
        }
      }
      else
      {
        /* Can not retrieve proper key handle */
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* (KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DECRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY) */

#if defined(KMS_AES_GCM) && ((KMS_AES_GCM & KMS_FCT_ENCRYPT) | (KMS_AES_GCM & KMS_FCT_DECRYPT))
    case CKM_AES_GCM:
    {
      kms_obj_keyhead_t *pkms_object;
      kms_attr_t *P_pKeyAttribute;
      kms_aes_gcm_ec_ctx_t *p_ctx;
      CK_GCM_PARAMS *pGCMParams;

      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hKey);

      /* Check that hKey is valid:
       * - NULL_PTR value means not found key handle
       * - KMS_ABI_VERSION_CK_2_40 & KMS_ABI_CONFIG_KEYHEAD are magic in header of the key
       */
      if ((pkms_object != NULL_PTR) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {
        /* If IV is defined */
        if ((pMechanism->pParameter != NULL_PTR) &&
            (pMechanism->ulParameterLen != 0UL))
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
              p_ctx = KMS_Alloc(hSession, sizeof(kms_aes_gcm_ec_ctx_t));
              if (p_ctx == NULL_PTR)
              {
                e_ret_status = CKR_DEVICE_MEMORY;
                break;
              }
              /* Store information in session structure for later use */
              KMS_GETSESSION(hSession).hKey = hKey;
              KMS_GETSESSION(hSession).Mechanism = pMechanism->mechanism;
              KMS_GETSESSION(hSession).pCtx = p_ctx;

              /* Get pointer to parameters */
              pGCMParams = (CK_GCM_PARAMS *)pMechanism->pParameter;

              /* Set flag field to default value */
              p_ctx->ca_ctx.mFlags = CA_E_SK_DEFAULT;
              /* Set key size */
              p_ctx->ca_ctx.mKeySize = (int32_t)P_pKeyAttribute->size ;

              /* Read value from the structure. Need to be translated from
              (uint32_t*) to (uint8_t *) */
              KMS_Objects_BlobU32_2_u8ptr(&(P_pKeyAttribute->data[0]), P_pKeyAttribute->size, p_ctx->key);
              p_ctx->ca_ctx.pmKey = p_ctx->key;

              /* Set nonce size field to iv_length, note that valid values are 7,8,9,10,11,12,13*/
              p_ctx->ca_ctx.mIvSize = (int32_t)pGCMParams->ulIvLen;

              /* Size of returned authentication TAG */
              p_ctx->ca_ctx.mTagSize = ((int32_t)(pGCMParams->ulTagBits) / 8L);

              /* Call the right init function */
              if (encdec_flag == KMS_FLAG_ENCRYPT)
              {
#if (KMS_AES_GCM & KMS_FCT_ENCRYPT)
                /* Crypto function call*/
                if (CA_AES_GCM_Encrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            pGCMParams->pIv) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_GCM & KMS_FCT_ENCRYPT */
              }
              else
              {
#if (KMS_AES_GCM & KMS_FCT_DECRYPT)
                /* Crypto function call*/
                if (CA_AES_GCM_Decrypt_Init(&(p_ctx->ca_ctx),
                                            p_ctx->key,
                                            pGCMParams->pIv) == CA_AES_SUCCESS)
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
#endif /* KMS_AES_GCM & KMS_FCT_ENCRYPT */
              }

              if ((e_ret_status == CKR_OK) && (pGCMParams->ulAADLen != 0UL))
              {
                /* If a header (or Additional Data is proposed ==> AAD from MECHANISM) */
                if (CA_AES_GCM_Header_Append(&(p_ctx->ca_ctx),
                                             pGCMParams->pAAD,
                                             (int32_t)pGCMParams->ulAADLen) == CA_AES_SUCCESS)
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
          /* No IV defined */
          e_ret_status = CKR_ARGUMENTS_BAD;
        }
      }
      else
      {
        /* Can not retrieve proper key handle */
        e_ret_status = CKR_OBJECT_HANDLE_INVALID;
      }
      break;
    }
#endif /* (KMS_AES_GCM & KMS_FCT_ENCRYPT) | (KMS_AES_GCM & KMS_FCT_DECRYPT) */
    default:
    {
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
    }
  }
  if (e_ret_status == CKR_OK)
  {
    /* If successful, set processing state of the session */
    if (encdec_flag == KMS_FLAG_ENCRYPT)
    {
      KMS_GETSESSION(hSession).state = KMS_SESSION_ENCRYPT;
    }
    else
    {
      KMS_GETSESSION(hSession).state = KMS_SESSION_DECRYPT;
    }
  }

  return e_ret_status;
}
#endif /* KMS_ENCRYPT || KMS_DECRYPT */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @addtogroup KMS_ENC_DEC_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is called upon @ref C_EncryptInit call
  * @note   Refer to @ref C_EncryptInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @note   The CKA_ENCRYPT attribute of the encryption key, which indicates
  *         whether the key supports encryption, MUST be CK_TRUE
  * @param  hSession session handle
  * @param  pMechanism encryption mechanism
  * @param  hKey handle of the encryption key
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         @ref encrypt_decrypt_init returned values
  */
CK_RV          KMS_EncryptInit(CK_SESSION_HANDLE hSession,
                               CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
#if defined(KMS_ENCRYPT)
  CK_RV e_ret_status;

  /* We reuse similar code between Encrypt & Decrypt Init */
  e_ret_status = encrypt_decrypt_init(hSession, pMechanism, hKey, KMS_FLAG_ENCRYPT);

  return (e_ret_status) ;
#else /* KMS_ENCRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_ENCRYPT */
}

/**
  * @brief  This function is called upon @ref C_Encrypt call
  * @note   Refer to @ref C_Encrypt function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pData data to encrypt
  * @param  ulDataLen length of data to encrypt
  * @param  pEncryptedData encrypted data
  * @param  pulEncryptedDataLen length of encrypted data
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         @ref KMS_EncryptUpdate returned values
  */
CK_RV          KMS_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
                           CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData,
                           CK_ULONG_PTR pulEncryptedDataLen)
{
#if defined(KMS_ENCRYPT)
  CK_RV e_ret_status ;

  e_ret_status = KMS_EncryptUpdate(hSession, pData, ulDataLen, pEncryptedData,
                                   pulEncryptedDataLen);

  /* Encryption completed on one packet processing:
   * - free the allocated context if any
   * - release the session
   */
  KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }

  KMS_SetStateIdle(hSession);

  return (e_ret_status) ;
#else /* KMS_ENCRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_ENCRYPT */
}

/**
  * @brief  This function is called upon @ref C_EncryptUpdate call
  * @note   Refer to @ref C_EncryptUpdate function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pPart data part to encrypt
  * @param  ulPartLen length of data part to encrypt
  * @param  pEncryptedPart encrypted data part
  * @param  pulEncryptedPartLen length of encrypted data part
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV          KMS_EncryptUpdate(CK_SESSION_HANDLE hSession,
                                 CK_BYTE_PTR pPart, CK_ULONG ulPartLen,
                                 CK_BYTE_PTR pEncryptedPart,
                                 CK_ULONG_PTR pulEncryptedPartLen)
{
#if defined(KMS_ENCRYPT)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  int32_t lEncryptPartLen = 0;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_ENCRYPT)
  {
    return CKR_OPERATION_NOT_INITIALIZED;
  }

  KMS_CHECK_BUFFER_SECTION5_2(pEncryptedPart, pulEncryptedPartLen, ulPartLen);

  switch (KMS_GETSESSION(hSession).Mechanism)
  {
#if defined(KMS_AES_CBC) && (KMS_AES_CBC & KMS_FCT_ENCRYPT)
    case CKM_AES_CBC:
    {
      /* Retrieve the allocated context */
      kms_aes_cbc_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Encrypt Data */
      if (CA_AES_CBC_Encrypt_Append(&(p_ctx->ca_ctx),
                                    pPart,
                                    (int32_t)ulPartLen,
                                    pEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CBC & KMS_FCT_ENCRYPT */

#if defined(KMS_AES_CCM) && (KMS_AES_CCM & KMS_FCT_ENCRYPT)
    case CKM_AES_CCM:
    {
      /* Retrieve the allocated context */
      kms_aes_ccm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Encrypt Data */
      if (CA_AES_CCM_Encrypt_Append(&(p_ctx->ca_ctx),
                                    pPart,
                                    (int32_t)ulPartLen,
                                    pEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CCM & KMS_FCT_ENCRYPT */

#if defined(KMS_AES_ECB) && ((KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY))
    case CKM_AES_ECB:
    {
      /* Retrieve the allocated context */
      kms_aes_ecb_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Encrypt Data */
      if (CA_AES_ECB_Encrypt_Append(&(p_ctx->ca_ctx),
                                    pPart,
                                    (int32_t)ulPartLen,
                                    (uint8_t *)pEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* (KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY) */

#if defined(KMS_AES_GCM) && (KMS_AES_GCM & KMS_FCT_ENCRYPT)
    case CKM_AES_GCM:
    {
      /* Retrieve the allocated context */
      kms_aes_gcm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Encrypt Data */
      if (CA_AES_GCM_Encrypt_Append(&(p_ctx->ca_ctx),
                                    pPart,
                                    (int32_t)ulPartLen,
                                    pEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_GCM & KMS_FCT_ENCRYPT */

    default:
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
  }

  /* Return status*/
  if (e_ret_status == CKR_OK)
  {
    /* Update the encrypted length to upper layer */
    *pulEncryptedPartLen = (uint32_t)lEncryptPartLen;
  }
  else
  {
    /* Return a 0-length value to upper layer */
    *pulEncryptedPartLen = 0UL;
    /* In case of error:
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
  }

  return e_ret_status;
#else /* KMS_ENCRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_ENCRYPT */
}

/**
  * @brief  This function is called upon @ref C_EncryptFinal call
  * @note   Refer to @ref C_EncryptFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pLastEncryptedPart last encrypted data part
  * @param  pulLastEncryptedPartLen length of the last encrypted data part
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV          KMS_EncryptFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pLastEncryptedPart,
                                CK_ULONG_PTR pulLastEncryptedPartLen)
{
#if defined(KMS_ENCRYPT)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  int32_t lEncryptPartLen = 0;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_ENCRYPT)
  {
    return CKR_OPERATION_NOT_INITIALIZED;
  }

  switch (KMS_GETSESSION(hSession).Mechanism)
  {
#if defined(KMS_AES_CBC) && (KMS_AES_CBC & KMS_FCT_ENCRYPT)
    case CKM_AES_CBC:
    {
      /* Retrieve the allocated context */
      kms_aes_cbc_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Complete encryption process */
      if (CA_AES_CBC_Encrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CBC & KMS_FCT_ENCRYPT */

#if defined(KMS_AES_CCM) && (KMS_AES_CCM & KMS_FCT_ENCRYPT)
    case CKM_AES_CCM:
    {
      /* Retrieve the allocated context */
      kms_aes_ccm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pLastEncryptedPart, pulLastEncryptedPartLen, (uint32_t)p_ctx->ca_ctx.mTagSize);

      /* Complete encryption process */
      if (CA_AES_CCM_Encrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        /* Update last encrypted part length */
        *pulLastEncryptedPartLen = (uint32_t)lEncryptPartLen;
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CCM & KMS_FCT_ENCRYPT */

#if defined(KMS_AES_ECB) && ((KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY))
    case CKM_AES_ECB:
    {
      /* Retrieve the allocated context */
      kms_aes_ecb_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Complete encryption process */
      if (CA_AES_ECB_Encrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* (KMS_AES_ECB & KMS_FCT_ENCRYPT) | (KMS_AES_ECB & KMS_FCT_DERIVE_KEY) */

#if defined(KMS_AES_GCM) && (KMS_AES_GCM & KMS_FCT_ENCRYPT)
    case CKM_AES_GCM:
    {
      /* Retrieve the allocated context */
      kms_aes_gcm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pLastEncryptedPart, pulLastEncryptedPartLen, (uint32_t)(p_ctx->ca_ctx.mTagSize));

      /* Complete encryption process */
      if (CA_AES_GCM_Encrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastEncryptedPart,
                                    &lEncryptPartLen) == CA_AES_SUCCESS)
      {
        /* Update last encrypted part length */
        *pulLastEncryptedPartLen = (uint32_t)lEncryptPartLen;
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_GCM & KMS_FCT_ENCRYPT */

    default:
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
  }

  /* Upon completion:
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
#else /* KMS_ENCRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_ENCRYPT */
}

/**
  * @brief  This function is called upon @ref C_DecryptInit call
  * @note   Refer to @ref C_DecryptInit function description
  *         for more details on the APIs, parameters and possible returned values
  * @note   The CKA_ENCRYPT attribute of the decryption key, which indicates
  *         whether the key supports encryption, MUST be CK_TRUE
  * @param  hSession session handle
  * @param  pMechanism decryption mechanism
  * @param  hKey handle of decryption key
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         @ref encrypt_decrypt_init returned values
  */
CK_RV          KMS_DecryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                               CK_OBJECT_HANDLE hKey)
{
#if defined(KMS_DECRYPT)
  /* We reuse similar code between Encrypt & Decrypt Init */
  return    encrypt_decrypt_init(hSession, pMechanism, hKey, KMS_FLAG_DECRYPT);
#else /* KMS_DECRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DECRYPT */
}

/**
  * @brief  This function is called upon @ref C_Decrypt call
  * @note   Refer to @ref C_Decrypt function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pEncryptedData encrypted data
  * @param  ulEncryptedDataLen length of encrypted data
  * @param  pData decrypted data
  * @param  pulDataLen length of decrypted data
  * @retval CKR_OK
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         @ref KMS_DecryptUpdate returned values
  */
CK_RV          KMS_Decrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pEncryptedData,
                           CK_ULONG ulEncryptedDataLen,
                           CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{
#if defined(KMS_DECRYPT)
  CK_RV e_ret_status;

  e_ret_status = KMS_DecryptUpdate(hSession, pEncryptedData, ulEncryptedDataLen,
                                   pData,   pulDataLen);

  /* Decryption completed on one packet processing:
   * - free the allocated context if any
   * - release the session
   */
  KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }

  KMS_SetStateIdle(hSession);

  return (e_ret_status) ;
#else /* KMS_DECRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DECRYPT */
}

/**
  * @brief  This function is called upon @ref C_DecryptUpdate call
  * @note   Refer to @ref C_DecryptUpdate function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pEncryptedPart encrypted data part
  * @param  ulEncryptedPartLen length of encrypted data part
  * @param  pPart decrypted data part
  * @param  pulPartLen length of decrypted data part
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV          KMS_DecryptUpdate(CK_SESSION_HANDLE hSession,
                                 CK_BYTE_PTR pEncryptedPart,
                                 CK_ULONG ulEncryptedPartLen,
                                 CK_BYTE_PTR pPart, CK_ULONG_PTR pulPartLen)
{
#if defined(KMS_DECRYPT)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  int32_t lPartLen = 0;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_DECRYPT)
  {
    return CKR_OPERATION_NOT_INITIALIZED;
  }

  switch (KMS_GETSESSION(hSession).Mechanism)
  {
#if defined(KMS_AES_CBC) && (KMS_AES_CBC & KMS_FCT_DECRYPT)
    case CKM_AES_CBC:
    {
      /* Retrieve the allocated context */
      kms_aes_cbc_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pPart, pulPartLen, ulEncryptedPartLen);
      /* Encrypt Data */
      if (CA_AES_CBC_Decrypt_Append(&(p_ctx->ca_ctx),
                                    pEncryptedPart,
                                    (int32_t)ulEncryptedPartLen,
                                    pPart,
                                    (int32_t *)&lPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CBC & KMS_FCT_DECRYPT */

#if defined(KMS_AES_CCM) && (KMS_AES_CCM & KMS_FCT_DECRYPT)
    case CKM_AES_CCM:
    {
      /* Retrieve the allocated context */
      kms_aes_ccm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Check if this chunk is last one including tag */
      if (p_ctx->dataRemain < ulEncryptedPartLen)
      {
        KMS_CHECK_BUFFER_SECTION5_2(pPart, pulPartLen, p_ctx->dataRemain);
        /* Set TAG position */
        p_ctx->ca_ctx.pmTag = &pEncryptedPart[p_ctx->dataRemain];
        /* Encrypt Data */
        if (CA_AES_CCM_Decrypt_Append(&(p_ctx->ca_ctx),
                                      pEncryptedPart,
                                      (int32_t)p_ctx->dataRemain,
                                      pPart,
                                      (int32_t *)&lPartLen) == CA_AES_SUCCESS)
        {
          /* No more data to decrypt */
          p_ctx->dataRemain = 0;
          /* Complete decryption to verify TAG */
          if (CA_AES_CCM_Decrypt_Finish(&(p_ctx->ca_ctx),
                                        p_ctx->tag,
                                        (int32_t *)(uint32_t) &(p_ctx->tagLength)) == CA_AUTHENTICATION_SUCCESSFUL)
          {
            e_ret_status = CKR_OK;
          }
          else
          {
            e_ret_status = CKR_FUNCTION_FAILED;
          }
        }
        else
        {
          e_ret_status = CKR_FUNCTION_FAILED;
        }
      }
      else
      {
        KMS_CHECK_BUFFER_SECTION5_2(pPart, pulPartLen, ulEncryptedPartLen);
        /* Encrypt Data */
        if (CA_AES_CCM_Decrypt_Append(&(p_ctx->ca_ctx),
                                      pEncryptedPart,
                                      (int32_t)ulEncryptedPartLen,
                                      pPart,
                                      (int32_t *)&lPartLen) == CA_AES_SUCCESS)
        {
          /* Decrease remaining data to decrypt */
          p_ctx->dataRemain -= ulEncryptedPartLen;
          e_ret_status = CKR_OK;
        }
        else
        {
          e_ret_status = CKR_FUNCTION_FAILED;
        }
      }

      break;
    }
#endif /* KMS_AES_CCM & KMS_FCT_DECRYPT */

#if defined(KMS_AES_ECB) && (KMS_AES_ECB & KMS_FCT_DECRYPT)
    case CKM_AES_ECB:
    {
      /* Retrieve the allocated context */
      kms_aes_ecb_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pPart, pulPartLen, ulEncryptedPartLen);
      /* Encrypt Data */
      if (CA_AES_ECB_Decrypt_Append(&(p_ctx->ca_ctx),
                                    pEncryptedPart,
                                    (int32_t)ulEncryptedPartLen,
                                    pPart,
                                    (int32_t *)&lPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_ECB */

#if defined(KMS_AES_GCM) && (KMS_AES_GCM & KMS_FCT_DECRYPT)
    case CKM_AES_GCM:
    {
      /* Retrieve the allocated context */
      kms_aes_gcm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pPart, pulPartLen, ulEncryptedPartLen);
      /* Encrypt Data */
      if (CA_AES_GCM_Decrypt_Append(&(p_ctx->ca_ctx),
                                    pEncryptedPart,
                                    (int32_t)ulEncryptedPartLen,
                                    pPart,
                                    (int32_t *)&lPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_GCM & KMS_FCT_DECRYPT */

    default:
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
  }

  /* Return status*/
  if (e_ret_status == CKR_OK)
  {
    /* Update the encrypted length to upper layer */
    *pulPartLen = (uint32_t)lPartLen;
  }
  else
  {
    /* Return a 0-length value to upper layer */
    *pulPartLen = 0UL;
    /* In case of error:
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
  }

  return e_ret_status;
#else /* KMS_DECRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DECRYPT */
}

/**
  * @brief  This function is called upon @ref C_DecryptFinal call
  * @note   Refer to @ref C_DecryptFinal function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pLastPart last decrypted data part
  * @param  pulLastPartLen length of the last decrypted data part
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_OPERATION_NOT_INITIALIZED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV          KMS_DecryptFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pLastPart, CK_ULONG_PTR pulLastPartLen)
{
#if defined(KMS_DECRYPT)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if DigestInit has been called previously */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_DECRYPT)
  {
    return CKR_OPERATION_NOT_INITIALIZED;
  }

  switch (KMS_GETSESSION(hSession).Mechanism)
  {
#if defined(KMS_AES_CBC) && (KMS_AES_CBC & KMS_FCT_DECRYPT)
    case CKM_AES_CBC:
    {
      /* Retrieve the allocated context */
      kms_aes_cbc_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Complete decryption process */
      if (CA_AES_CBC_Decrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastPart,
                                    (int32_t *)(uint32_t)pulLastPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CBC & KMS_FCT_DECRYPT */

#if defined(KMS_AES_CCM) && (KMS_AES_CCM & KMS_FCT_DECRYPT)
    case CKM_AES_CCM:
    {
      /* Retrieve the allocated context */
      kms_aes_ccm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pLastPart, pulLastPartLen, p_ctx->tagLength);
      /* Process already completed */
      if ((p_ctx->dataRemain == 0UL) && (p_ctx->tagLength > 0UL))
      {
        /* Update data buffer with previously retrieved TAG data */
        (void)memcpy(pLastPart, p_ctx->tag, p_ctx->tagLength);
        *pulLastPartLen = p_ctx->tagLength;
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_CCM & KMS_FCT_DECRYPT */

#if defined(KMS_AES_ECB) && (KMS_AES_ECB & KMS_FCT_DECRYPT)
    case CKM_AES_ECB:
    {
      /* Retrieve the allocated context */
      kms_aes_ecb_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      /* Complete decryption process */
      if (CA_AES_ECB_Decrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastPart,
                                    (int32_t *)(uint32_t)pulLastPartLen) == CA_AES_SUCCESS)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_ECB & KMS_FCT_DECRYPT */

#if defined(KMS_AES_GCM) && (KMS_AES_GCM & KMS_FCT_DECRYPT)
    case CKM_AES_GCM:
    {
      /* Retrieve the allocated context */
      kms_aes_gcm_ec_ctx_t *p_ctx = KMS_GETSESSION(hSession).pCtx;

      KMS_CHECK_BUFFER_SECTION5_2(pLastPart, pulLastPartLen, (uint32_t)p_ctx->ca_ctx.mTagSize);
      /* Set crypto library context tag pointer to buffer */
      p_ctx->ca_ctx.pmTag = pLastPart;
      /* Complete decryption process */
      if (CA_AES_GCM_Decrypt_Finish(&(p_ctx->ca_ctx),
                                    pLastPart,
                                    (int32_t *)(uint32_t)pulLastPartLen) == CA_AUTHENTICATION_SUCCESSFUL)
      {
        e_ret_status = CKR_OK;
      }
      else
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      break;
    }
#endif /* KMS_AES_GCM & KMS_FCT_DECRYPT */

    default:
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
  }

  /* Upon completion:
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
#else /* KMS_DECRYPT */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DECRYPT */
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
