/**
  ******************************************************************************
  * @file    kms_blob_metadata.h
  * @author  MCD Application Team
  * @brief   This file contains metadata definitions for BLOB functionalities.
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
#ifndef KMS_BLOB_DEF_METADATA_H
#define KMS_BLOB_DEF_METADATA_H

#ifdef __cplusplus
extern "C" {
#endif


/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_OBJECTS Blob Objects Management
  * @{
  */

/** @addtogroup KMS_OBJECTS_MD Metadata
  * @note   Encrypted Blobs to import uses the following crypto scheme:
  *         <ul>
  *         <li> SHA256 tag signed with ECCDSA to authenticate the KMS blob Metadata: @ref KMS_BLOB_MAC_LEN
  *         <li> Asymmetric keys
  *         <li> AES-CBC encryption and SHA256 for firmware tag
  *         </ul>
  * @{
  */

/** @addtogroup KMS_OBJECTS_MD_Exported_Constants Exported Constants
  * @{
  */

#define KMS_BLOB_HEADER_TOT_LEN       ((uint32_t) sizeof(KMS_BlobRawHeaderTypeDef)) /*!< Blob INFO header Total Length*/
#define KMS_BLOB_IMG_OFFSET           ((uint32_t) 320) /*!< Blob image offset */

#define KMS_BLOB_MAC_LEN              ((uint32_t) 64)  /*!< KMS blob Header MAC LEN*/
#define KMS_BLOB_ASYM_PUBKEY_LEN      ((uint32_t) 64)  /*!< KMS blob Asymmetric Public Key length (bytes)*/
#define KMS_BLOB_IV_LEN               ((uint32_t) 16)  /*!< KMS blob IV Length (Bytes): same size as an AES block*/
#define KMS_BLOB_TAG_LEN              ((uint32_t) 32)  /*!< KMS blob Tag Length (Bytes): SHA-256 for the FW tag */

/**
  * @}
  */

/** @addtogroup KMS_OBJECTS_MD_Exported_Types Exported Types
  * @{
  */

/**
  * @brief  KMS Blob Header structure definition
  */
typedef struct
{
  uint32_t KMSMagic;                     /*!< KMS Magic 'KMSB'*/
  uint16_t ProtocolVersion;              /*!< KMS Protocol version*/
  uint16_t BlobVersion;                  /*!< Blob version*/
  uint32_t BlobSize;                     /*!< Blob size (bytes)*/
  uint32_t Reserved1;                    /*!< empty */
  uint32_t Reserved2;                    /*!< empty */
  uint8_t  BlobTag[KMS_BLOB_TAG_LEN];    /*!< Blob Tag*/
  uint8_t  Reserved3[KMS_BLOB_TAG_LEN];  /*!< empty */
  uint8_t  InitVector[KMS_BLOB_IV_LEN];  /*!< IV used to encrypt firmware */
  uint8_t  Reserved4[28U];               /*!< empty */
  uint8_t  HeaderMAC[KMS_BLOB_MAC_LEN];  /*!< MAC of the full header message */
  uint8_t  Reserved5[128U];              /*!< empty */
} KMS_BlobRawHeaderTypeDef;

/**
  * @}
  */

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

#endif /* KMS_BLOB_DEF_METADATA_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

