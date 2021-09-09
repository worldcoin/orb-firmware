/**
  ******************************************************************************
  * @file    kms_key_mgt.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
  *          module key handling functionalities.
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
#include "kms_key_mgt.h"        /* KMS key creation services */
#include "kms_enc_dec.h"        /* Encryption & key storage service for key derivation */
#include "kms_objects.h"        /* KMS object management services */
#include "kms_nvm_storage.h"    /* KMS NVM storage services */
#include "kms_vm_storage.h"     /* KMS VM storage services */
#include "kms_platf_objects.h"  /* KMS platform objects services */
#include "CryptoApi/ca.h"       /* Crypto API services */
#include "kms_ecc.h"            /* KMS elliptic curves utils */
#include "kms_der_x962.h"       /* KMS DER & X962 utilities */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_KEY Key handling services
  * @{
  */

/* Private types -------------------------------------------------------------*/
/** @addtogroup KMS_KEY_Private_Types Private Types
  * @{
  */
#if defined(KMS_ECDSA)
/**
  * @brief  ECDSA Key generation context
  */
typedef struct
{
  uint8_t tmpbuffer[CA_ECDSA_REQUIRED_WORKING_BUFFER];    /*!< Used as a working buffer for the crypto library */
#if defined(KMS_EC_SECP384)
  uint8_t der_pub[2 * CA_CRL_ECC_P384_SIZE + 4 /* To include DER & X9.62 header */]; /*!< Used to store processing
                                                                                  public key in DER +X9.62 format */
  uint8_t pub_x[CA_CRL_ECC_P384_SIZE];                    /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P384_SIZE];                    /*!< Used to store processing public key y coordinate */
  uint8_t priv[CA_CRL_ECC_P384_SIZE];                     /*!< Used to store processing private key */
#elif defined(KMS_EC_SECP256)
  uint8_t der_pub[2 * CA_CRL_ECC_P256_SIZE + 4 /* To include DER & X9.62 header */]; /*!< Used to store processing
                                                                                  public key in DER +X9.62 format */
  uint8_t pub_x[CA_CRL_ECC_P256_SIZE];                    /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P256_SIZE];                    /*!< Used to store processing public key y coordinate */
  uint8_t priv[CA_CRL_ECC_P256_SIZE];                     /*!< Used to store processing private key */
#else /* KMS_EC_SECP384 || KMS_EC_SECP256 */
  uint8_t der_pub[2 * CA_CRL_ECC_P192_SIZE + 4 /* To include DER & X9.62 header */]; /*!< Used to store processing
                                                                                  public key in DER +X9.62 format */
  uint8_t pub_x[CA_CRL_ECC_P192_SIZE];                    /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P192_SIZE];                    /*!< Used to store processing public key y coordinate */
  uint8_t priv[CA_CRL_ECC_P192_SIZE];                     /*!< Used to store processing private key */
#endif /* KMS_EC_SECP384 || KMS_EC_SECP256 */
  uint32_t pub_size;                                      /*!< Used to store processing public key size */
  uint32_t priv_size;                                     /*!< Used to store processing private key size */
} kms_ecdsa_gk_ctx_t;

/**
  * @brief  ECDSA Key derivation context
  */
typedef struct
{
  uint8_t tmpbuffer[CA_ECDSA_REQUIRED_WORKING_BUFFER];    /*!< Used as a working buffer for the crypto library */
#if defined(KMS_EC_SECP384)
  uint8_t pub_x[CA_CRL_ECC_P384_SIZE];                    /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P384_SIZE];                    /*!< Used to store processing public key y coordinate */
  uint8_t priv[CA_CRL_ECC_P384_SIZE];                     /*!< Used to store processing private key */
#elif defined(KMS_EC_SECP256)
  uint8_t pub_x[CA_CRL_ECC_P256_SIZE];                    /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P256_SIZE];                    /*!< Used to store processing public key y coordinate */
  uint8_t priv[CA_CRL_ECC_P256_SIZE];                     /*!< Used to store processing private key */
#else /* KMS_EC_SECP384 || KMS_EC_SECP256 */
  uint8_t pub_x[CA_CRL_ECC_P192_SIZE];                    /*!< Used to store processing public key x coordinate */
  uint8_t pub_y[CA_CRL_ECC_P192_SIZE];                    /*!< Used to store processing public key y coordinate */
  uint8_t priv[CA_CRL_ECC_P192_SIZE];                     /*!< Used to store processing private key */
#endif /* KMS_EC_SECP384 || KMS_EC_SECP256 */
  uint32_t pub_size;                                      /*!< Used to store processing public key size */
} kms_ecdsa_dk_ctx_t;
#endif /* KMS_ECDSA */
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_KEY_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is called upon @ref C_DeriveKey call
  * @note   Refer to @ref C_DeriveKey function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pMechanism key derivation mechanism
  * @param  hBaseKey base key for derivation
  * @param  pTemplate new key template
  * @param  ulAttributeCount template length
  * @param  phKey new key handle
  * @retval CKR_OK
  *         CKR_ACTION_PROHIBITED
  *         CKR_ARGUMENTS_BAD
  *         CKR_ATTRIBUTE_VALUE_INVALID
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_KEY_HANDLE_INVALID
  *         CKR_MECHANISM_INVALID
  *         CKR_MECHANISM_PARAM_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  *         @ref KMS_Objects_SearchAttributes returned values
  *         @ref KMS_EncryptInit returned values
  *         @ref KMS_Encrypt returned values
  *         @ref KMS_Objects_CreateNStoreBlobForAES returned values
  *         @ref KMS_ECC_LoadCurve returned values
  */
CK_RV          KMS_DeriveKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                             CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR  pTemplate,
                             CK_ULONG  ulAttributeCount, CK_OBJECT_HANDLE_PTR  phKey)
{
#ifdef KMS_DERIVE_KEY
  CK_RV e_ret_status;
  kms_obj_keyhead_t *pkms_object;
  kms_attr_t *P_pKeyAttribute = NULL;

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
  if (pMechanism == NULL_PTR)
  {
    return CKR_ARGUMENTS_BAD;
  }
  switch (pMechanism->mechanism)
  {
#if defined(KMS_AES_ECB) && (KMS_AES_ECB & KMS_FCT_DERIVE_KEY)
    case CKM_AES_ECB_ENCRYPT_DATA:
    {
      CK_MECHANISM aes_ecb_mechanism = { CKM_AES_ECB, NULL, 0 };
      CK_ULONG EncryptedLen;
      uint8_t *pKeyBuffer;

      /* Derivation is done based on the value passed in the MEchanism */
      if ((pMechanism->pParameter == NULL) ||
          (pMechanism->ulParameterLen == 0UL))
      {
        e_ret_status = CKR_MECHANISM_PARAM_INVALID;
        break;
      }
      if ((pMechanism->ulParameterLen != CA_CRL_AES128_KEY)
          && (pMechanism->ulParameterLen != CA_CRL_AES192_KEY)
          && (pMechanism->ulParameterLen != CA_CRL_AES256_KEY))
      {
        e_ret_status = CKR_MECHANISM_PARAM_INVALID;
        break;
      }

      /* The Key */
      /* Read the key value from the Key Handle                 */
      /* Key Handle is the index to one of static or nvm        */
      pkms_object = KMS_Objects_GetPointer(hBaseKey);

      /* Check that hKey is valid */
      if ((pkms_object != NULL) &&
          (pkms_object->version == KMS_ABI_VERSION_CK_2_40) &&
          (pkms_object->configuration == KMS_ABI_CONFIG_KEYHEAD))
      {

        /* Search for the Key Value to use */
        e_ret_status = KMS_Objects_SearchAttributes(CKA_VALUE, pkms_object, &P_pKeyAttribute);

        if (e_ret_status == CKR_OK)
        {
          kms_attr_t  *pDeriveAttribute;

          /* As stated in PKCS11 spec:                                                   */
          /* The CKA_DERIVE attribute has the value CK_TRUE if and only if it is         */
          /*   possible to derive other keys from the key                                */
          /* Check that the object allows to DERIVE a KEY, checking ATTRIBUTE CKA_DERIVE */
          e_ret_status = KMS_Objects_SearchAttributes(CKA_DERIVE, pkms_object, &pDeriveAttribute);

          if (e_ret_status == CKR_OK)
          {
            if (*pDeriveAttribute->data != CK_TRUE)
            {
              /* Key derivation not permitted for the selected object  */
              e_ret_status = CKR_ACTION_PROHIBITED;
              break;
            }
          }

          /* Set key size with value from attribute  */
          if ((P_pKeyAttribute->size == CA_CRL_AES128_KEY) ||    /* 128 bits */
              (P_pKeyAttribute->size == CA_CRL_AES192_KEY) ||     /* 192 bits */
              (P_pKeyAttribute->size == CA_CRL_AES256_KEY))       /* 256 bits */
          {
            /* Allocate a Key buffer */
            pKeyBuffer = (uint8_t *)KMS_Alloc(hSession, pMechanism->ulParameterLen);
            if (pKeyBuffer == NULL)
            {
              e_ret_status = CKR_DEVICE_MEMORY;
              break;
            }

          }
          else
          {
            /* Unsupported key size */
            e_ret_status = CKR_ATTRIBUTE_VALUE_INVALID;
            break;
          }

          /* Reuse the AES-EncryptInit function */
          /* The Encryption mechanism do not expect any param, use one the local definition */
          e_ret_status = KMS_EncryptInit(hSession, &aes_ecb_mechanism, hBaseKey);
          if (e_ret_status != CKR_OK)
          {
            KMS_Free(hSession, pKeyBuffer);
            break;
          }
          /* Calculate the derived KEY from the Encrypting the pMechanism->pParameter */
          /* with the hBaseKey */
          EncryptedLen = pMechanism->ulParameterLen;
          e_ret_status = KMS_Encrypt(hSession, pMechanism->pParameter,
                                     pMechanism->ulParameterLen, pKeyBuffer, &EncryptedLen);

          if (e_ret_status == CKR_OK)
          {
            /* At this stage, creation of an object to store the derived key   */
            /* The object should embed the created Key, and the Template part  */
            e_ret_status = KMS_Objects_CreateNStoreBlobForAES(hSession,
                                                              pKeyBuffer,
                                                              EncryptedLen,
                                                              pTemplate,
                                                              ulAttributeCount,
                                                              phKey);
          }

          /* Free the buffer */
          if (pKeyBuffer != NULL)
          {
            KMS_Free(hSession, pKeyBuffer);
          }
        }
      }
      else
      {
        e_ret_status = CKR_KEY_HANDLE_INVALID;
      }

      /* No more crypto to manage with this key */
      KMS_GETSESSION(hSession).hKey = KMS_HANDLE_KEY_NOT_KNOWN;
      break;
    }
#endif /* KMS_AES_ECB & KMS_FCT_DERIVE_KEY */
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_DERIVE_KEY)
    case CKM_ECDH1_DERIVE:
    {
      CK_ECDH1_DERIVE_PARAMS_PTR p_params;
      kms_attr_t *pAttr;
      CA_membuf_stt mb_st;
      CA_EC_stt  EC_st;
      CA_ECpoint_stt *pubKey = NULL;
      CA_ECpoint_stt *resKey = NULL;
      CA_ECCprivKey_stt *privKey = NULL;
      kms_ecdsa_dk_ctx_t *p_ctx;

      /* Assign mechanism params */
      p_params = (CK_ECDH1_DERIVE_PARAMS_PTR)(pMechanism->pParameter);
      if (p_params == NULL_PTR)
      {
        e_ret_status = CKR_ARGUMENTS_BAD;
        break;
      }
      /*
       * This implementation only support for
       * - Key Deriviation function: CKD_NULL
       * - Shared data: none
       */
      if ((p_params->kdf != CKD_NULL) || (p_params->pSharedData != NULL_PTR) || (p_params->ulSharedDataLen != 0UL))
      {
        e_ret_status = CKR_MECHANISM_PARAM_INVALID;
        break;
      }

      /* Check if a public key was specified in input */
      if ((p_params->pPublicData == NULL_PTR) || (p_params->ulPublicDataLen == 0UL))
      {
        e_ret_status = CKR_DOMAIN_PARAMS_INVALID;
        break;
      }

      pkms_object = KMS_Objects_GetPointer(hBaseKey);
      /* Check that hKey is valid */
      if ((pkms_object == NULL) ||
          (pkms_object->version != KMS_ABI_VERSION_CK_2_40) ||
          (pkms_object->configuration != KMS_ABI_CONFIG_KEYHEAD))
      {
        e_ret_status = CKR_KEY_HANDLE_INVALID;
        break;
      }

      /* The CKA_EC_PARAMS attribute, specify the Curve to use for the verification */
      e_ret_status = KMS_Objects_SearchAttributes(CKA_EC_PARAMS, pkms_object, &pAttr);

      if (e_ret_status != CKR_OK)
      {
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

      /* Allocate context */
      p_ctx = KMS_Alloc(hSession, sizeof(kms_ecdsa_dk_ctx_t));
      if (p_ctx == NULL_PTR)
      {
        e_ret_status = CKR_DEVICE_MEMORY;
        break;
      }

      /* Prepare the memory buffer strucure */
      mb_st.pmBuf = p_ctx->tmpbuffer;
      mb_st.mUsed = 0;
      mb_st.mSize = (int16_t)sizeof(p_ctx->tmpbuffer);
      /* Init the Elliptic Curve, passing the required memory */
      if (CA_ECCinitEC(&EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }
      /* EC is initialized, now prepare to read the base key. First, allocate a point */
      if (CA_ECCinitPoint(&pubKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }

      /*
       * Import public key from parameters
       */

      /* Here, the curve is loaded in EC_st */
      p_ctx->pub_size = (uint32_t)(EC_st.mNsize);

      /* Expect an EC Point in DER format uncompressed */
      if (KMS_DerX962_ExtractPublicKeyCoord(p_params->pPublicData, &(p_ctx->pub_x[0]), &(p_ctx->pub_y[0]),
                                            (uint32_t)p_ctx->pub_size)
          != CKR_OK)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Point is initialized, now import the public key */
      (void)CA_ECCsetPointCoordinate(pubKey, CA_E_ECC_POINT_COORDINATE_X, p_ctx->pub_x, (int32_t)(p_ctx->pub_size));
      (void)CA_ECCsetPointCoordinate(pubKey, CA_E_ECC_POINT_COORDINATE_Y, p_ctx->pub_y, (int32_t)(p_ctx->pub_size));

      /* The CKA_VALUE attribute, specify the private key for scalar multiplication */
      e_ret_status = KMS_Objects_SearchAttributes(CKA_VALUE, pkms_object, &pAttr);
      if (e_ret_status != CKR_OK)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Initialize the key */
      if (CA_ECCinitPrivKey(&privKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }
      KMS_Objects_BlobU32_2_u8ptr(pAttr->data, pAttr->size, &(p_ctx->priv[0]));
      if (CA_ECCsetPrivKeyValue(privKey, p_ctx->priv, (int32_t)(pAttr->size)) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Compute */
      if (CA_ECCinitPoint(&resKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }
      if (CA_ECCscalarMul(pubKey, privKey, resKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Extract the secret store on X coordinate */
      (void)memset(p_ctx->pub_x, 0, sizeof(p_ctx->pub_x));
      if (CA_ECCgetPointCoordinate(resKey, CA_E_ECC_POINT_COORDINATE_X, &(p_ctx->pub_x[0]),
                                   (int32_t *)(uint32_t) &(p_ctx->pub_size)) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        /* Free allocated context upon error */
        KMS_Free(hSession, p_ctx);
        break;
      }
      /* Store generated key into KMS NVM database */
      e_ret_status = KMS_Objects_CreateNStoreBlobForAES(hSession,
                                                        &(p_ctx->pub_x[0]),
                                                        p_ctx->pub_size,
                                                        pTemplate,
                                                        ulAttributeCount,
                                                        phKey);
      KMS_Free(hSession, p_ctx);
      break;
    }
#endif /* KMS_ECDSA & KMS_FCT_DERIVE_KEY */

    default:
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
  }

  return e_ret_status;
#else /* KMS_DERIVE_KEY */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_DERIVE_KEY */
}

/**
  * @brief  This function is called upon @ref C_GenerateKeyPair call
  * @note   Refer to @ref C_GenerateKeyPair function description
  *         for more details on the APIs, parameters and possible returned values
  *         This implementation supports only CKM_EC_KEY_PAIR_GEN generation mechanism.
  * @note   This function don't respect the PKCS11 standard
  * @param  hSession                     session handle
  * @param  pMechanism                   key-gen mech.
  * @param  pPublicKeyTemplate           template for pub. key
  * @param  ulPublicKeyAttributeCount    # pub. attrs.
  * @param  pPrivateKeyTemplate          template for priv. key
  * @param  ulPrivateKeyAttributeCount   # priv.  attrs.
  * @param  phPublicKey                  pointer to the public key to genrerate
  * @param  phPrivateKey                 pointer to the private key  to generate
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_MECHANISM_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  *         @ref KMS_Objects_CreateNStoreBlobForECCPair returned values
  */
CK_RV KMS_GenerateKeyPair(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
                          CK_ATTRIBUTE_PTR pPublicKeyTemplate, CK_ULONG ulPublicKeyAttributeCount,
                          CK_ATTRIBUTE_PTR pPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount,
                          CK_OBJECT_HANDLE_PTR phPublicKey, CK_OBJECT_HANDLE_PTR phPrivateKey)
{
#if defined(KMS_GENERATE_KEYS)
  CK_RV e_ret_status = CKR_OK;
  kms_obj_key_pair_t key_pair;

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
  /*Check input parameters*/
  if ((pMechanism == NULL_PTR) || (pPublicKeyTemplate == NULL_PTR) || (pPrivateKeyTemplate == NULL_PTR)
      || (phPublicKey == NULL_PTR) || (phPrivateKey == NULL_PTR) || (ulPublicKeyAttributeCount == 0UL)
      || (ulPrivateKeyAttributeCount == 0UL))
  {
    return CKR_ARGUMENTS_BAD;
  }
  /* Implementation note
   * switch/case in use instead of simple if for more readability
   * allows usage of break to stop processing without compplicated if imbrication
   */
  switch (pMechanism->mechanism)
  {
#if defined(KMS_ECDSA) && (KMS_ECDSA & KMS_FCT_GENERATE_KEYS)
    case CKM_EC_KEY_PAIR_GEN:
    {
      CA_EC_stt  EC_st;
      CA_membuf_stt mb_st;
      kms_attr_t *pAttr = NULL_PTR;
      kms_ecdsa_gk_ctx_t *p_ctx;
      CA_ECpoint_stt *PubKey = NULL;
      CA_ECCprivKey_stt *PrivKey = NULL;
      CA_RNGinitInput_stt RNG_st;
      uint8_t entropy_data[32] =
      {
        0x9d, 0x20, 0x1a, 0x18, 0x9b, 0x6d, 0x1a, 0xa7, 0x0e, 0x79, 0x57, 0x6f, 0x36,
        0xb6, 0xaa, 0x88, 0x55, 0xfd, 0x4a, 0x7f, 0x97, 0xe9, 0x71, 0x69, 0xb6, 0x60,
        0x88, 0x78, 0xe1, 0x9c, 0x8b, 0xa5
      };
      CA_RNGstate_stt RNG_state;
      uint32_t dsize = 0;

      /* Search for curve params */
      for (uint32_t i = 0; (i < ulPublicKeyAttributeCount) && (pAttr == NULL_PTR); i++)
      {
        if (pPublicKeyTemplate[i].type == CKA_EC_PARAMS)
        {
          pAttr = KMS_Alloc(hSession, pPublicKeyTemplate[i].ulValueLen + sizeof(kms_attr_t));
          if (pAttr == NULL_PTR)
          {
            return CKR_DEVICE_MEMORY;
          }
          (void)memcpy(&(pAttr->data[0]), pPublicKeyTemplate[i].pValue, pPublicKeyTemplate[i].ulValueLen);
          pAttr->id = CKA_EC_PARAMS;
          pAttr->size = pPublicKeyTemplate[i].ulValueLen;
        }
      }
      if (pAttr == NULL_PTR)
      {
        return CKR_ARGUMENTS_BAD;
      }
      /* Load the Elliptic Curve defined in the Object by CKA_EC_PARAMS */
      e_ret_status = KMS_ECC_LoadCurve(pAttr, &EC_st);
      KMS_Free(hSession, pAttr);
      if (e_ret_status != CKR_OK)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        break;
      }
      p_ctx = KMS_Alloc(hSession, sizeof(kms_ecdsa_gk_ctx_t));
      if (p_ctx == NULL_PTR)
      {
        return CKR_DEVICE_MEMORY;
      }

      /* Prepare the memory buffer strucure */
      mb_st.pmBuf = p_ctx->tmpbuffer;
      mb_st.mUsed = 0;
      mb_st.mSize = (int16_t)sizeof(p_ctx->tmpbuffer);

      /* Init the Elliptic Curve, passing the required memory */
      if (CA_ECCinitEC(&EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }
      /* EC is initialized, now init the public key point*/
      if (CA_ECCinitPoint(&PubKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }
      /* EC is initialized, now init the private key point*/
      if (CA_ECCinitPrivKey(&PrivKey, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Initialize random state */
      RNG_st.pmEntropyData = entropy_data;
      RNG_st.mEntropyDataSize = (int32_t)sizeof(entropy_data);
      RNG_st.pmNonce = NULL;
      RNG_st.mNonceSize = 0;
      RNG_st.mPersDataSize = 0;
      RNG_st.pmPersData = NULL;

      if (CA_RNGinit(&RNG_st,  &RNG_state) != CA_RNG_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Generate key pair */
      if (CA_ECCkeyGen(PrivKey, PubKey, &RNG_state, &EC_st, &mb_st) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Retrieve coordinet of the public key */
      if (CA_ECCgetPointCoordinate(PubKey, CA_E_ECC_POINT_COORDINATE_X, p_ctx->pub_x,
                                   (int32_t *)(uint32_t) &(p_ctx->pub_size)) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }
      if (CA_ECCgetPointCoordinate(PubKey, CA_E_ECC_POINT_COORDINATE_Y, p_ctx->pub_y,
                                   (int32_t *)(uint32_t) &(p_ctx->pub_size)) != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }
      /* Store an EC Point in DER format uncompressed */
      if (KMS_DerX962_ConstructDERPublicKeyCoord(&(p_ctx->pub_x[0]), &(p_ctx->pub_y[0]), (uint32_t)p_ctx->pub_size,
                                                 p_ctx->der_pub, &dsize)
          != CKR_OK)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }

      if (CA_ECCgetPrivKeyValue(PrivKey, &(p_ctx->priv[0]), (int32_t *)(uint32_t) &(p_ctx->priv_size))
          != CA_ECC_SUCCESS)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
        KMS_Free(hSession, p_ctx);
        break;
      }

      /* Fill in key pair structure with generated key pair */
      key_pair.pPub = p_ctx->der_pub;
      key_pair.pubSize = dsize;
      key_pair.pPriv = p_ctx->priv;
      key_pair.privSize = p_ctx->priv_size;
      /* Store key pair with associated templates */
      e_ret_status = KMS_Objects_CreateNStoreBlobForECCPair(hSession,
                                                            &key_pair,
                                                            pPublicKeyTemplate, ulPublicKeyAttributeCount,
                                                            pPrivateKeyTemplate, ulPrivateKeyAttributeCount,
                                                            phPublicKey, phPrivateKey);

      (void)CA_RNGfree(&RNG_state);
      (void)CA_ECCfreePoint(&PubKey, &mb_st);
      (void)CA_ECCfreePrivKey(&PrivKey, &mb_st);
      (void)CA_ECCfreeEC(&EC_st, &mb_st);

      KMS_Free(hSession, p_ctx);
      break;
    }
#endif /* KMS_ECDSA & KMS_FCT_GENERATE_KEYS */

    default:
    {
      e_ret_status = CKR_MECHANISM_INVALID;
      break;
    }
  }

  return e_ret_status;
#else /* KMS_GENERATE_KEYS */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_GENERATE_KEYS */
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
