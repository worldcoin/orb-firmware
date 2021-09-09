/**
  ******************************************************************************
  * @file    kms_blob_headers.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services module
  *          Blob objects
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef KMS_BLOB_HEADERS_H
#define KMS_BLOB_HEADERS_H


#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_OBJECTS Blob Objects Management
  * @{
  */

/* Exported constants --------------------------------------------------------*/

/** @addtogroup KMS_OBJECTS_Exported_Constants Exported Constants
  * @{
  */
/* define adapted to identify Objects in Blobs    */
/*  upper value = 0xB10B on both Version & CONFIG */
/**
  * @brief KMS ABI version
  * @note  Starts with 0xB10B for "Blob"
  * @note  Match with
  *        <a href=docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *        PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a>
  */
#define KMS_ABI_VERSION_CK_2_40           0xB10B0240U
/**
  * @brief KMS Blob objects structure version
  * @note  Starts with 0xB10B for "Blob"
  */
#define KMS_ABI_CONFIG_KEYHEAD            0xB10B0003U

/**
  * @}
  */

/* Exported types ------------------------------------------------------------*/

/** @addtogroup KMS_OBJECTS_Exported_Types Exported Types
  * @{
  */
/* These structures are used even for static & dynamic object definition */

/**
  * @brief KMS Blob header structure definition
  * @note  Not used to store objects cause contains no data but to ease access
  *        to variable length blob objects
  */
typedef struct
{
  uint32_t version;               /*!< ABI version in use: must be @ref KMS_ABI_VERSION_CK_2_40 */
  uint32_t configuration;         /*!< Blob version in use: must be @ref KMS_ABI_CONFIG_KEYHEAD */
  uint32_t blobs_size;            /*!< Blob size */
  uint32_t blobs_count;           /*!< Blob count */
  uint32_t object_id;             /*!< Blob object ID */
} kms_obj_keyhead_no_blob_t;

/**
  * @brief KMS Blob object structure definition
  * @note  Is used as a cast structure to address variable length blobs arrays
  */
typedef struct
{
  uint32_t version;               /*!< ABI version in use: must be @ref KMS_ABI_VERSION_CK_2_40 */
  uint32_t configuration;         /*!< Blob version in use: must be @ref KMS_ABI_CONFIG_KEYHEAD */
  uint32_t blobs_size;            /*!< Blob size */
  uint32_t blobs_count;           /*!< Blob count */
  uint32_t object_id;             /*!< Blob object ID */
  uint32_t blobs[1];              /*!< Blob object first data elements */
} kms_obj_keyhead_t;

/**
  * @brief Macro to declare kms_obj_keyhead_t like structures of various size
  * @param PREFIX   A prefix to the declared type name
  * @param BLOB_NB  The blobs array length the created structure must contain
  * @note  Declared struct will be name [PREFIX]kms_obj_keyhead_<BLOB_NB>_t
  */
#define KMS_DECLARE_BLOB_STRUCT(PREFIX, BLOB_NB) \
  typedef struct                    \
  {                                 \
    uint32_t version;               \
    uint32_t configuration;         \
    uint32_t blobs_size;            \
    uint32_t blobs_count;           \
    uint32_t object_id;             \
    uint32_t blobs[BLOB_NB];        \
  } PREFIX##kms_obj_keyhead_##BLOB_NB##_t;

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
#ifdef __cplusplus
}
#endif

#endif /* KMS_BLOB_HEADERS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
