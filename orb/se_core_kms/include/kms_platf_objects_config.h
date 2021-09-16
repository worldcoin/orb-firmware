/**
  ******************************************************************************
  * @file    kms_platf_objects_config.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module platform objects management configuration
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef KMS_PLATF_OBJECTS_CONFIG_H
#define KMS_PLATF_OBJECTS_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "kms_platf_objects_interface.h"
#include "app_sfu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_PLATF Platform Objects
  * @{
  */

/* Exported constants --------------------------------------------------------*/

/** @addtogroup KMS_PLATF_Exported_Constants Exported Constants
  * @note KMS support different type of objects, their respective ranges are
  *       defined here
  * @{
  */

/* We consider that the ORDER (static = lower ids) will be kept. */
#define KMS_INDEX_MIN_EMBEDDED_OBJECTS       1UL   /*!< Embedded objects min ID. Must be > 0 as '0' is never a
                                                        valid key index */
#define KMS_INDEX_MAX_EMBEDDED_OBJECTS       (KMS_INDEX_MIN_EMBEDDED_OBJECTS+19UL)    /*!< Embedded objects max ID */
#define KMS_INDEX_MIN_NVM_STATIC_OBJECTS     (KMS_INDEX_MAX_EMBEDDED_OBJECTS+1UL)     /*!< NVM static objects min ID */
#define KMS_INDEX_MAX_NVM_STATIC_OBJECTS     (KMS_INDEX_MIN_NVM_STATIC_OBJECTS+19UL)  /*!< NVM static objects max ID */

#define KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS    (KMS_INDEX_MAX_NVM_STATIC_OBJECTS+1UL)   /*!< NVM dynamic objects min ID */
#define KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS    (KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS+19UL) /*!< NVM dynamic objects max ID */

/* When EXTERNAL TOKEN is not supported the below values can be commented */
#define KMS_INDEX_MIN_EXT_TOKEN_STATIC_OBJECTS        70UL  /*!< External token static objects min ID */
#define KMS_INDEX_MAX_EXT_TOKEN_STATIC_OBJECTS        89UL  /*!< External token static objects max ID */
#define KMS_INDEX_MIN_EXT_TOKEN_DYNAMIC_OBJECTS       90UL  /*!< External token dynamic objects min ID */
#define KMS_INDEX_MAX_EXT_TOKEN_DYNAMIC_OBJECTS      110UL  /*!< External token dynamic objects max ID */

/* Blob import key index */
#define KMS_INDEX_BLOBIMPORT_VERIFY       (1U)      /*!< Index in @ref KMS_PlatfObjects_EmbeddedList
                                                         where the blob verification key is stored */
#define KMS_INDEX_BLOBIMPORT_DECRYPT      (2U)      /*!< Index in @ref KMS_PlatfObjects_EmbeddedList
                                                         where the blob decryption key is stored */

/**
  * @}
  */

/*
 * Embedded objects definition
 *
 */
#ifdef KMS_PLATF_OBJECTS_C
/** @addtogroup KMS_PLATF_Private_Variables Private Variables
  * @{
  */

KMS_DECLARE_BLOB_STRUCT(, 24);
KMS_DECLARE_BLOB_STRUCT(, 30);
KMS_DECLARE_BLOB_STRUCT(, 256);

/* This object is used for KMS blob header signature                 */
static const kms_obj_keyhead_30_t   KMS_Blob_ECDSA_Verify =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  120,                             /*  uint32_t blobs_size */
  4,                               /*  uint32_t blobs_count */
  1,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_PUBLIC_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_EC,
    CKA_EC_PARAMS,    10, 0x06082a86U, 0x48ce3d03U, 0x0107U,
    CKA_EC_POINT,     67, 0x044104baU, 0xf297f83eU, 0xe307dc16U, 0xc371781dU, 0xf1b03ef0U, 0x95b04454U, 0x128148fbU, 0x2c66b954U, 0x3da54ae8U, 0x260476b7U, 0x378b3c46U, 0xd8fd6a63U, 0x617c46c3U, 0x7de94644U, 0x316ed7e1U, 0x6dba70edU, 0x44ba02U,
  }
};

/* This object is used for KMS blob encryption                    */
static const kms_obj_keyhead_24_t   KMS_Blob_AES_CBC128_Decrypt =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  96,                              /*  uint32_t blobs_size */
  7,                               /*  uint32_t blobs_count */
  2,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_SECRET_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_AES,
    CKA_VALUE,        16, 0x4f454d5fU, 0x4b45595fU, 0x434f4d50U, 0x414e5931U,
    CKA_ENCRYPT,      sizeof(CK_BBOOL), CK_FALSE,
    CKA_COPYABLE,     sizeof(CK_BBOOL), CK_FALSE,
    CKA_EXTRACTABLE,  sizeof(CK_BBOOL), CK_FALSE,
    CKA_DERIVE,       sizeof(CK_BBOOL), CK_FALSE,
  }
};


#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
static const kms_obj_keyhead_24_t   SBSFU_AES_1_128 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  96,                              /*  uint32_t blobs_size */
  7,                               /*  uint32_t blobs_count */
  3,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_SECRET_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_AES,
    CKA_VALUE,        @AES_SBSFU_KEY_1@
    CKA_ENCRYPT,      sizeof(CK_BBOOL), CK_FALSE,
    CKA_COPYABLE,     sizeof(CK_BBOOL), CK_FALSE,
    CKA_EXTRACTABLE,  sizeof(CK_BBOOL), CK_FALSE,
    CKA_DERIVE,       sizeof(CK_BBOOL), CK_FALSE,
  }
};
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
static const kms_obj_keyhead_24_t   SBSFU_AES_2_128 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  96,                              /*  uint32_t blobs_size */
  7,                               /*  uint32_t blobs_count */
  4,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_SECRET_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_AES,
    CKA_VALUE,        @AES_SBSFU_KEY_2@
    CKA_ENCRYPT,      sizeof(CK_BBOOL), CK_FALSE,
    CKA_COPYABLE,     sizeof(CK_BBOOL), CK_FALSE,
    CKA_EXTRACTABLE,  sizeof(CK_BBOOL), CK_FALSE,
    CKA_DERIVE,       sizeof(CK_BBOOL), CK_FALSE,
  }
};
#endif /*(SFU_NB_MAX_ACTIVE_IMAGE > 1U)*/

#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
static const kms_obj_keyhead_24_t   SBSFU_AES_3_128 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  96,                              /*  uint32_t blobs_size */
  7,                               /*  uint32_t blobs_count */
  5,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_SECRET_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_AES,
    CKA_VALUE,        @AES_SBSFU_KEY_3@
    CKA_ENCRYPT,      sizeof(CK_BBOOL), CK_FALSE,
    CKA_COPYABLE,     sizeof(CK_BBOOL), CK_FALSE,
    CKA_EXTRACTABLE,  sizeof(CK_BBOOL), CK_FALSE,
    CKA_DERIVE,       sizeof(CK_BBOOL), CK_FALSE,
  }
};
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 2U) */
#endif /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) */

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256)
static const kms_obj_keyhead_30_t   SBSFU_ECDSA_1_Verify =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  120,                             /*  uint32_t blobs_size */
  4,                               /*  uint32_t blobs_count */
  6,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_PUBLIC_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_EC,
    CKA_EC_PARAMS,    10, 0x06082a86U, 0x48ce3d03U, 0x0107U,   /* EC OIDs */
    CKA_EC_POINT,     @ECDSA_SBSFU_KEY_1@
  }
};

#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
static const kms_obj_keyhead_30_t   SBSFU_ECDSA_2_Verify =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  120,                             /*  uint32_t blobs_size */
  4,                               /*  uint32_t blobs_count */
  7,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_PUBLIC_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_EC,
    CKA_EC_PARAMS,    10, 0x06082a86U, 0x48ce3d03U, 0x0107U,   /* EC OIDs */
    CKA_EC_POINT,     @ECDSA_SBSFU_KEY_2@
  }
};
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 1U) */

#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
static const kms_obj_keyhead_30_t   SBSFU_ECDSA_3_Verify =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  120,                             /*  uint32_t blobs_size */
  4,                               /*  uint32_t blobs_count */
  8,                               /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_PUBLIC_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_EC,
    CKA_EC_PARAMS,    10, 0x06082a86U, 0x48ce3d03U, 0x0107U,
    CKA_EC_POINT,     @ECDSA_SBSFU_KEY_3@
  }
};
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 2U) */
#endif /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) */

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256)
static const kms_obj_keyhead_256_t   SBSFU_Root_CA =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  0,                             /*  uint32_t blobs_size */
  5,                               /*  uint32_t blobs_count */
  9,                               /*  uint32_t object_id */
  {
    CKA_CLASS,                sizeof(CK_OBJECT_CLASS),          CKO_CERTIFICATE,
    CKA_CERTIFICATE_TYPE,     sizeof(CKA_CERTIFICATE_TYPE),     CKC_X_509,
    CKA_CERTIFICATE_CATEGORY, sizeof(CKA_CERTIFICATE_CATEGORY), CK_CERTIFICATE_CATEGORY_AUTHORITY,
    /* 'SBSF' 'URoo' 'tCAC' 'erti' 'fica' 'te' */
    CKA_LABEL,                22, 0x46534253U, 0x6F6F5255U, 0x43414374U, 0x69747265U, 0x61636966U, 0x6574U,
    CKA_VALUE,                505, 0x308201f5U, 0x3082019aU, 0xa0030201U, 0x02020900U, 0x982dc2b4U, 0xde65397cU, 0x300a0608U, 0x2a8648ceU, 0x3d040302U, 0x304d310bU, 0x30090603U, 0x55040613U, 0x02465231U, 0x1f301d06U, 0x0355040aU, 0x0c165354U, 0x4d696372U, 0x6f656c65U, 0x6374726fU, 0x6e696373U, 0x20496e63U, 0x311d301bU, 0x06035504U, 0x030c1453U, 0x42534655U, 0x2053616dU, 0x706c6520U, 0x526f6f74U, 0x20434130U, 0x1e170d32U, 0x30303431U, 0x30313534U, 0x3832305aU, 0x170d3233U, 0x30313035U, 0x31353438U, 0x32305a30U, 0x4d310b30U, 0x09060355U, 0x04061302U, 0x4652311fU, 0x301d0603U, 0x55040a0cU, 0x1653544dU, 0x6963726fU, 0x656c6563U, 0x74726f6eU, 0x69637320U, 0x496e6331U, 0x1d301b06U, 0x03550403U, 0x0c145342U, 0x53465520U, 0x53616d70U, 0x6c652052U, 0x6f6f7420U, 0x43413059U, 0x30130607U, 0x2a8648ceU, 0x3d020106U, 0x082a8648U, 0xce3d0301U, 0x07034200U, 0x047929dfU, 0xf9ecf344U, 0xb3d202e0U, 0x9e2f9991U, 0x7ef89464U, 0x73eebd7fU, 0x9c62cb97U, 0x5810c986U, 0x887e2c7eU, 0xc507ec11U, 0x47e4de5eU, 0x1bc71958U, 0x4e311a59U, 0x057b6e71U, 0x7bcccdf1U, 0x3cbc6fc5U, 0xcea36330U, 0x61301d06U, 0x03551d0eU, 0x04160414U, 0x533d9df9U, 0x23f9ed59U, 0x06059748U, 0x1914f114U, 0xbe373d6bU, 0x301f0603U, 0x551d2304U, 0x18301680U, 0x14533d9dU, 0xf923f9edU, 0x59060597U, 0x481914f1U, 0x14be373dU, 0x6b300f06U, 0x03551d13U, 0x0101ff04U, 0x05300301U, 0x01ff300eU, 0x0603551dU, 0x0f0101ffU, 0x04040302U, 0x0186300aU, 0x06082a86U, 0x48ce3d04U, 0x03020349U, 0x00304602U, 0x2100d8caU, 0xc816ecabU, 0xfa4d9400U, 0x744f59dfU, 0xd91e22b0U, 0x191fc67aU, 0x15a0aee7U, 0xa729bb04U, 0xf42c0221U, 0x0088ca65U, 0x7291eab6U, 0x3af0b9f2U, 0xb817ff4bU, 0x6eeaf631U, 0xd0309f28U, 0x4bb090adU, 0x861a3fdeU, 0x38U,
  }
};

static const kms_obj_keyhead_256_t   SBSFU_OEM_CA_1 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  0,                             /*  uint32_t blobs_size */
  5,                               /*  uint32_t blobs_count */
  10,                              /*  uint32_t object_id */
  {
    CKA_CLASS,                sizeof(CK_OBJECT_CLASS),          CKO_CERTIFICATE,
    CKA_CERTIFICATE_TYPE,     sizeof(CKA_CERTIFICATE_TYPE),     CKC_X_509,
    CKA_CERTIFICATE_CATEGORY, sizeof(CKA_CERTIFICATE_CATEGORY), CK_CERTIFICATE_CATEGORY_AUTHORITY,
    /* 'OEMI' 'nter' 'mCAC' 'erti' 'fica' 'te_1' */
    CKA_LABEL,                24, 0x494D454FU, 0x7265746EU, 0x4341436DU, 0x69747265U, 0x61636966U, 0x315F6574U,
    CKA_VALUE,                491, 0x308201e7U, 0x3082018eU, 0xa0030201U, 0x02020101U, 0x300a0608U, 0x2a8648ceU, 0x3d040302U, 0x304d310bU, 0x30090603U, 0x55040613U, 0x02465231U, 0x1f301d06U, 0x0355040aU, 0x0c165354U, 0x4d696372U, 0x6f656c65U, 0x6374726fU, 0x6e696373U, 0x20496e63U, 0x311d301bU, 0x06035504U, 0x030c1453U, 0x42534655U, 0x2053616dU, 0x706c6520U, 0x526f6f74U, 0x20434130U, 0x1e170d32U, 0x30303431U, 0x30313534U, 0x3832335aU, 0x170d3231U, 0x30343230U, 0x31353438U, 0x32335a30U, 0x46310b30U, 0x09060355U, 0x04061302U, 0x46523111U, 0x300f0603U, 0x55040a0cU, 0x0853544dU, 0x204f454dU, 0x31312430U, 0x22060355U, 0x04030c1bU, 0x53425346U, 0x55205361U, 0x6d706c65U, 0x20494e54U, 0x4552313aU, 0x4f454d31U, 0x20434130U, 0x59301306U, 0x072a8648U, 0xce3d0201U, 0x06082a86U, 0x48ce3d03U, 0x01070342U, 0x00047b4bU, 0x5ceb6362U, 0x78fbfd05U, 0x0401885fU, 0xc840008dU, 0xd8476465U, 0x8d867db3U, 0xbcdffa46U, 0x5fce2beeU, 0x7ff59179U, 0x73808acdU, 0x2951af93U, 0xf87a88abU, 0x2a798bf4U, 0xb3099b1eU, 0x25457823U, 0x5c96a366U, 0x3064301dU, 0x0603551dU, 0x0e041604U, 0x14adc781U, 0x10a625ecU, 0xc7d6a366U, 0x65916732U, 0x2c584958U, 0x47301f06U, 0x03551d23U, 0x04183016U, 0x8014533dU, 0x9df923f9U, 0xed590605U, 0x97481914U, 0xf114be37U, 0x3d6b3012U, 0x0603551dU, 0x130101ffU, 0x04083006U, 0x0101ff02U, 0x0101300eU, 0x0603551dU, 0x0f0101ffU, 0x04040302U, 0x0186300aU, 0x06082a86U, 0x48ce3d04U, 0x03020347U, 0x00304402U, 0x20493eb6U, 0x24f10dd4U, 0x23a0afd0U, 0xf0183365U, 0xb62a2b37U, 0x49c699a5U, 0x5ae18992U, 0x8e188ec4U, 0xcd02207aU, 0xfd5b0c81U, 0x484b7e7cU, 0x276acc97U, 0xeca6dd74U, 0x8b7a6d97U, 0x6c4f94ecU, 0xca696f88U, 0x228273U,
  }
};

#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
static const kms_obj_keyhead_256_t   SBSFU_OEM_CA_2 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  0,                             /*  uint32_t blobs_size */
  5,                               /*  uint32_t blobs_count */
  11,                              /*  uint32_t object_id */
  {
    CKA_CLASS,                sizeof(CK_OBJECT_CLASS),          CKO_CERTIFICATE,
    CKA_CERTIFICATE_TYPE,     sizeof(CKA_CERTIFICATE_TYPE),     CKC_X_509,
    CKA_CERTIFICATE_CATEGORY, sizeof(CKA_CERTIFICATE_CATEGORY), CK_CERTIFICATE_CATEGORY_AUTHORITY,
    /* 'OEMI' 'nter' 'mCAC' 'erti' 'fica' 'te_2' */
    CKA_LABEL,                24, 0x494D454FU, 0x7265746EU, 0x4341436DU, 0x69747265U, 0x61636966U, 0x325F6574U,
    CKA_VALUE,                @SBSFU_OEM_CA_2@
  }
};
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 1U) */

#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
static const kms_obj_keyhead_256_t   SBSFU_OEM_CA_3 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  0,                             /*  uint32_t blobs_size */
  5,                               /*  uint32_t blobs_count */
  12,                              /*  uint32_t object_id */
  {
    CKA_CLASS,                sizeof(CK_OBJECT_CLASS),          CKO_CERTIFICATE,
    CKA_CERTIFICATE_TYPE,     sizeof(CKA_CERTIFICATE_TYPE),     CKC_X_509,
    CKA_CERTIFICATE_CATEGORY, sizeof(CKA_CERTIFICATE_CATEGORY), CK_CERTIFICATE_CATEGORY_AUTHORITY,
    /* 'OEMI' 'nter' 'mCAC' 'erti' 'fica' 'te_3' */
    CKA_LABEL,                24, 0x494D454FU, 0x7265746EU, 0x4341436DU, 0x69747265U, 0x61636966U, 0x335F6574U,
    CKA_VALUE,                @SBSFU_OEM_CA_3@
  }
};
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 2U) */
#endif /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256) */



/* These objects are used by user tKMS application                 */
static const kms_obj_keyhead_24_t   Test_Key_AES128 =
{
  KMS_ABI_VERSION_CK_2_40,         /*  uint32_t version */
  KMS_ABI_CONFIG_KEYHEAD,          /*  uint32_t configuration */
  68,                              /*  uint32_t blobs_size */
  4,                               /*  uint32_t blobs_count */
  13,                              /*  uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_SECRET_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_AES,
    CKA_VALUE,        16, 0xfeffe992U, 0x8665731cU, 0x6d6a8f94U, 0x67308308U,
    CKA_LABEL,        12, 0x52455355U, 0x50595243U, 0x00383231U,      /* 'USER', 'CRYP', '128' */
    0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL /* Fill end of structure */
  }
};

static const kms_obj_keyhead_24_t   Test_Key_AES256 =
{
  KMS_ABI_VERSION_CK_2_40,         /* uint32_t version  */
  KMS_ABI_CONFIG_KEYHEAD,          /* uint32_t configuration */
  84,                              /* uint32_t blobs_size */
  9,                               /* uint32_t blobs_count */
  14,                              /* uint32_t object_id */
  {
    CKA_CLASS,        sizeof(CK_OBJECT_CLASS), CKO_SECRET_KEY,
    CKA_KEY_TYPE,     sizeof(CK_KEY_TYPE),     CKK_AES,
    CKA_VALUE,        32, 0x03030303U, 0x03030303U, 0x03030303U, 0x03030303U,
    0x03030303U, 0x03030303U, 0x03030303U, 0x03030303U,
    CKA_LABEL,        12, 0x52455355U, 0x50595243U, 0x00363532U,      /* 'USER', 'CRYP', '256' */
    0UL, 0UL, 0UL                 /* Fill end of structure */
  }
};

/**
  * @brief  KMS embedded objects definition
  * @note   Must contains KMS blob verification and decryption keys
  */
static const kms_obj_keyhead_t *const KMS_PlatfObjects_EmbeddedList[KMS_INDEX_MAX_EMBEDDED_OBJECTS -
                                                                    KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1] =
{
  /* KMS Blob import keys */
  (kms_obj_keyhead_t *)(uint32_t) &KMS_Blob_ECDSA_Verify,       /* Index = 1 */
  (kms_obj_keyhead_t *)(uint32_t) &KMS_Blob_AES_CBC128_Decrypt, /* Index = 2 */
  /* SBSFU keys */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_AES_1_128,             /* Index = 3 */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_AES_2_128,             /* Index = 4 */
#else
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 4 */
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 1U) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_AES_3_128,             /* Index = 5 */
#else
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 5 */
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 2U) */
#else /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 3 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 4 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 5 */
#endif /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) */

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_ECDSA_1_Verify,        /* Index = 6 */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_ECDSA_2_Verify,        /* Index = 7 */
#else
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 7 */
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 1U) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_ECDSA_3_Verify,        /* Index = 8 */
#else
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 8 */
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 2U) */
#else /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 6 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 7 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 8 */
#endif /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_Root_CA,               /* Index = 9 */
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_OEM_CA_1,              /* Index = 10 */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_OEM_CA_2,              /* Index = 11 */
#else
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 11 */
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 1U) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  (kms_obj_keyhead_t *)(uint32_t) &SBSFU_OEM_CA_3,              /* Index = 12 */
#else
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 12 */
#endif /* (SFU_NB_MAX_ACTIVE_IMAGE > 2U) */
#else /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256) */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 9 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 10 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 11 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 12 */
#endif /* (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256) */
  /* UserApp example keys */
  (kms_obj_keyhead_t *)(uint32_t) &Test_Key_AES128,             /* Index = 13 */
  (kms_obj_keyhead_t *)(uint32_t) &Test_Key_AES256,             /* Index = 14 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 15 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 16 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 17 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 18 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 19 */
  (kms_obj_keyhead_t *) NULL,                                   /* Index = 20 */
};

/**
  * @}
  */
#endif /* KMS_PLATF_OBJECTS_C */


/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* KMS_PLATF_OBJECTS_CONFIG_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

