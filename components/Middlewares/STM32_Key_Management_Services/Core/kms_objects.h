/**
  ******************************************************************************
  * @file    kms_objects.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module object manipulation services.
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
#ifndef KMS_OBJECTS_H
#define KMS_OBJECTS_H

#ifdef __cplusplus
extern "C" {
#endif


/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_OBJECTS Blob Objects Management
  * @{
  */

/* Exported types ------------------------------------------------------------*/

/** @addtogroup KMS_OBJECTS_Exported_Types Exported Types
  * @{
  */

/**
  * @brief Allows to identify the different range of object IDs
  */
typedef enum
{
  KMS_OBJECT_RANGE_EMBEDDED = 0,              /*!< Objects embedded in code at compilation time */
  KMS_OBJECT_RANGE_NVM_STATIC_ID,             /*!< Objects stored in NVM with Static IDs */
  KMS_OBJECT_RANGE_NVM_DYNAMIC_ID,            /*!< Objects stored in NVM with Dynamic IDs */
  KMS_OBJECT_RANGE_VM_DYNAMIC_ID,             /*!< Objects stored in VM with Dynamic IDs */
  KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID,       /*!< Objects stored in external token with Static IDs */
  KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID,      /*!< Objects stored in external token with Dynamic IDs */
  KMS_OBJECT_RANGE_UNKNOWN                    /*!< Unknown objects */
} kms_obj_range_t;

/**
  * @brief Attribute element pointer in a serial blob
  */
typedef struct
{
  uint32_t id;        /*!< Item ID */
  uint32_t size;      /*!< Item Size */
  uint32_t data[1];   /*!< Item data */
} kms_attr_t;

/**
  * @brief Key pair structure definition
  */
typedef struct
{
  uint8_t           *pPub;          /*!< Public key */
  uint32_t           pubSize;       /*!< Public key size */
  uint8_t           *pPriv;         /*!< Private key */
  uint32_t          privSize;       /*!< Private key size */
} kms_obj_key_pair_t;

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

/** @addtogroup KMS_OBJECTS_Exported_Functions Exported Functions
  * @{
  */

kms_obj_keyhead_t *KMS_Objects_GetPointer(CK_OBJECT_HANDLE hKey);
kms_obj_range_t    KMS_Objects_GetRange(CK_OBJECT_HANDLE hKey);

CK_RV KMS_LockKeyHandle(CK_OBJECT_HANDLE hKey);
CK_RV KMS_CheckKeyIsNotLocked(CK_OBJECT_HANDLE hKey);
CK_RV KMS_LockServiceFctId(CK_ULONG fctId);
CK_RV KMS_CheckServiceFctIdIsNotLocked(CK_ULONG fctId);

CK_RV KMS_FindAttributeInTemplate(CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_ATTRIBUTE_TYPE type,
                                  CK_ATTRIBUTE_PTR *ppAttr);
CK_RV KMS_FindObjectsFromTemplate(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxCount,
                                  CK_ULONG_PTR pulObjectCount, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
CK_RV KMS_Objects_SearchAttributes(uint32_t searched_id, kms_obj_keyhead_t *pkms_key_head,
                                   kms_attr_t **pAttribute);

void KMS_Objects_u8ptr_2_BlobU32(uint8_t *pU8, uint32_t u8Size, uint32_t *pU32);
void  KMS_Objects_BlobU32_2_u8ptr(uint32_t *pU32, uint32_t u32Size, uint8_t *pU8);

CK_RV KMS_Objects_CreateNStoreBlobFromTemplates(CK_SESSION_HANDLE hSession,
                                                CK_ATTRIBUTE_PTR pTemplate1,
                                                CK_ULONG ulCount1,
                                                CK_ATTRIBUTE_PTR pTemplate2,
                                                CK_ULONG ulCount2,
                                                CK_OBJECT_HANDLE_PTR phObject);
CK_RV KMS_Objects_CreateNStoreBlobForAES(CK_SESSION_HANDLE hSession,
                                         uint8_t *pKey,
                                         uint32_t keySize,
                                         CK_ATTRIBUTE_PTR pTemplate,
                                         CK_ULONG ulCount,
                                         CK_OBJECT_HANDLE_PTR phObject);
CK_RV KMS_Objects_CreateNStoreBlobForECCPair(CK_SESSION_HANDLE hSession,
                                             kms_obj_key_pair_t *pKeyPair,
                                             CK_ATTRIBUTE_PTR pPubTemplate,
                                             CK_ULONG ulPubCount,
                                             CK_ATTRIBUTE_PTR pPrivTemplate,
                                             CK_ULONG ulPrivCount,
                                             CK_OBJECT_HANDLE_PTR phPubObject,
                                             CK_OBJECT_HANDLE_PTR phPrivObject);

CK_RV KMS_Objects_ImportBlob(CK_BYTE_PTR pHdr, CK_BYTE_PTR pFlash);
CK_RV KMS_Objects_LockKeys(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount);
CK_RV KMS_Objects_LockServices(CK_ULONG_PTR pServices, CK_ULONG ulCount);

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

#endif /* KMS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
