/**
  ******************************************************************************
  * @file    kms_platf_objects_config.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module platform objects management configuration
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
#ifndef KMS_PLATF_OBJECTS_CONFIG_H
#define KMS_PLATF_OBJECTS_CONFIG_H

/* Includes ------------------------------------------------------------------*/
#include "kms_platf_objects_interface.h"

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

/* We consider that the ORDER (static = lower ids) will be kept  */
#define KMS_INDEX_MIN_EMBEDDED_OBJECTS                1UL   /*!< Embedded objects min ID. Must be > 0 as '0' is never a
                                                                 valid key index */
#define KMS_INDEX_MAX_EMBEDDED_OBJECTS               29UL   /*!< Embedded objects max ID */
#define KMS_INDEX_MIN_NVM_STATIC_OBJECTS             30UL   /*!< NVM static objects min ID */
#define KMS_INDEX_MAX_NVM_STATIC_OBJECTS             49UL   /*!< NVM static objects max ID */
#define KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS            50UL   /*!< NVM dynamic objects min ID */
#define KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS            69UL   /*!< NVM dynamic objects max ID */

#define KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS             50UL   /*!< VM dynamic objects min ID */
#define KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS             69UL   /*!< VM dynamic objects max ID */

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
/**
  * @brief  KMS embedded objects definition
  * @note   Must contains KMS blob verification and decryption keys
  */
const kms_obj_keyhead_t *const KMS_PlatfObjects_EmbeddedList[KMS_INDEX_MAX_EMBEDDED_OBJECTS -
                                                             KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1] =
{
  (kms_obj_keyhead_t *) NULL,       /* Index = 1 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 2 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 3 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 4 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 5 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 6 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 7 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 8 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 9 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 10 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 11 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 12 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 13 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 14 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 15 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 16 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 17 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 18 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 19 */
  (kms_obj_keyhead_t *) NULL,       /* Index = 20 */
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

