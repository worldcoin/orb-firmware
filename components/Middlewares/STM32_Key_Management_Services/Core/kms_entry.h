/**
  ******************************************************************************
  * @file    kms_entry.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for the entry point of Key Management
  *          Services module functionalities.
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
#ifndef KMS_ENTRY_H
#define KMS_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_ENTRY Entry Point
  * @{
  */

/* Exported types ------------------------------------------------------------*/

/** @addtogroup KMS_ENTRY_Exported_Types Exported Types
  * @{
  */

#define KMS_INITIALIZE_FCT_ID             (0x01UL)                                /*!< C_Initialize function ID */
#define KMS_FINALIZE_FCT_ID               (KMS_INITIALIZE_FCT_ID+0x01UL)          /*!< C_Finalize function ID */
#define KMS_GET_INFO_FCT_ID               (KMS_FINALIZE_FCT_ID+0x01UL)            /*!< C_GetInfo function ID */
#define KMS_GET_SLOT_LIST_FCT_ID          (KMS_GET_INFO_FCT_ID+0x01UL)            /*!< C_GetSlotList function ID */
#define KMS_GET_SLOT_INFO_FCT_ID          (KMS_GET_SLOT_LIST_FCT_ID+0x01UL)       /*!< C_GetSlotInfo function ID */
#define KMS_GET_TOKEN_INFO_FCT_ID         (KMS_GET_SLOT_INFO_FCT_ID+0x01UL)       /*!< C_GetTokenInfo function ID */
#define KMS_GET_MECHANISM_LIST_FCT_ID     (KMS_GET_TOKEN_INFO_FCT_ID+0x01UL)      /*!< C_GetMechanismList function ID */
#define KMS_GET_MECHANISM_INFO_FCT_ID     (KMS_GET_MECHANISM_LIST_FCT_ID+0x01UL)  /*!< C_GetMechanismInfo function ID */
#define KMS_INIT_TOKEN_FCT_ID             (KMS_GET_MECHANISM_INFO_FCT_ID+0x01UL)  /*!< C_InitToken function ID */
#define KMS_INIT_PIN_FCT_ID               (KMS_INIT_TOKEN_FCT_ID+0x01UL)          /*!< C_InitPIN function ID */
#define KMS_SET_PIN_FCT_ID                (KMS_INIT_PIN_FCT_ID+0x01UL)            /*!< C_SetPIN function ID */
#define KMS_OPEN_SESSION_FCT_ID           (KMS_SET_PIN_FCT_ID+0x01UL)             /*!< C_OpenSession function ID */
#define KMS_CLOSE_SESSION_FCT_ID          (KMS_OPEN_SESSION_FCT_ID+0x01UL)        /*!< C_CloseSession function ID */
#define KMS_CLOSE_ALL_SESSIONS_FCT_ID     (KMS_CLOSE_SESSION_FCT_ID+0x01UL)       /*!< C_CloseAllSessions function ID */
#define KMS_GET_SESSION_INFO_FCT_ID       (KMS_CLOSE_ALL_SESSIONS_FCT_ID+0x01UL)  /*!< C_GetSessionInfo function ID */
#define KMS_GET_OPERATION_STATE_FCT_ID    (KMS_GET_SESSION_INFO_FCT_ID+0x01UL)    /*!< C_GetOperationState function ID */
#define KMS_SET_OPERATION_STATE_FCT_ID    (KMS_GET_OPERATION_STATE_FCT_ID+0x01UL) /*!< C_SetOperationState function ID */
#define KMS_LOGIN_FCT_ID                  (KMS_SET_OPERATION_STATE_FCT_ID+0x01UL) /*!< C_Login function ID */
#define KMS_LOGOUT_FCT_ID                 (KMS_LOGIN_FCT_ID+0x01UL)               /*!< C_Logout function ID */
#define KMS_CREATE_OBJECT_FCT_ID          (KMS_LOGOUT_FCT_ID+0x01UL)              /*!< C_CreateObject function ID */
#define KMS_COPY_OBJECT_FCT_ID            (KMS_CREATE_OBJECT_FCT_ID+0x01UL)       /*!< C_CopyObject function ID */
#define KMS_DESTROY_OBJECT_FCT_ID         (KMS_COPY_OBJECT_FCT_ID+0x01UL)         /*!< C_DestroyObject function ID */
#define KMS_GET_OBJECT_SIZE_FCT_ID        (KMS_DESTROY_OBJECT_FCT_ID+0x01UL)      /*!< C_GetObjectSize function ID */
#define KMS_GET_ATTRIBUTE_VALUE_FCT_ID    (KMS_GET_OBJECT_SIZE_FCT_ID+0x01UL)            /*!< C_GetAttributeValue function ID*/
#define KMS_SET_ATTRIBUTE_VALUE_FCT_ID    (KMS_GET_ATTRIBUTE_VALUE_FCT_ID+0x01UL) /*!< C_SetAttributeValue function ID*/
#define KMS_FIND_OBJECTS_INIT_FCT_ID      (KMS_SET_ATTRIBUTE_VALUE_FCT_ID+0x01UL)      /*!< C_FindObjectsInit function ID */
#define KMS_FIND_OBJECTS_FCT_ID           (KMS_FIND_OBJECTS_INIT_FCT_ID+0x01UL)   /*!< C_FindObjects function ID */
#define KMS_FIND_OBJECTS_FINAL_FCT_ID     (KMS_FIND_OBJECTS_FCT_ID+0x01UL)        /*!< C_FindObjectsFinal function ID */
#define KMS_ENCRYPT_INIT_FCT_ID           (KMS_FIND_OBJECTS_FINAL_FCT_ID+0x01UL)  /*!< C_EncryptInit function ID */
#define KMS_ENCRYPT_FCT_ID                (KMS_ENCRYPT_INIT_FCT_ID+0x01UL)        /*!< C_Encrypt function ID */
#define KMS_ENCRYPT_UPDATE_FCT_ID         (KMS_ENCRYPT_FCT_ID+0x01UL)             /*!< C_EncryptUpdate function ID */
#define KMS_ENCRYPT_FINAL_FCT_ID          (KMS_ENCRYPT_UPDATE_FCT_ID+0x01UL)      /*!< C_EncryptFinal function ID */
#define KMS_DECRYPT_INIT_FCT_ID           (KMS_ENCRYPT_FINAL_FCT_ID+0x01UL)       /*!< C_DecryptInit function ID */
#define KMS_DECRYPT_FCT_ID                (KMS_DECRYPT_INIT_FCT_ID+0x01UL)        /*!< C_Decrypt function ID */
#define KMS_DECRYPT_UPDATE_FCT_ID         (KMS_DECRYPT_FCT_ID+0x01UL)             /*!< C_DecryptUpdate function ID */
#define KMS_DECRYPT_FINAL_FCT_ID          (KMS_DECRYPT_UPDATE_FCT_ID+0x01UL)      /*!< C_DecryptFinal function ID */
#define KMS_DIGEST_INIT_FCT_ID            (KMS_DECRYPT_FINAL_FCT_ID+0x01UL)       /*!< C_DigestInit function ID */
#define KMS_DIGEST_FCT_ID                 (KMS_DIGEST_INIT_FCT_ID+0x01UL)         /*!< C_Digest function ID */
#define KMS_DIGEST_UPDATE_FCT_ID          (KMS_DIGEST_FCT_ID+0x01UL)              /*!< C_DigestUpdate function ID */
#define KMS_DIGEST_KEY_FCT_ID             (KMS_DIGEST_UPDATE_FCT_ID+0x01UL)       /*!< C_DigestKey function ID */
#define KMS_DIGEST_FINAL_FCT_ID           (KMS_DIGEST_KEY_FCT_ID+0x01UL)          /*!< C_DigestFinal function ID */
#define KMS_SIGN_INIT_FCT_ID              (KMS_DIGEST_FINAL_FCT_ID+0x01UL)        /*!< C_SignInit function ID */
#define KMS_SIGN_FCT_ID                   (KMS_SIGN_INIT_FCT_ID+0x01UL)           /*!< C_Sign function ID */
#define KMS_SIGN_UPDATE_FCT_ID            (KMS_SIGN_FCT_ID+0x01UL)                /*!< C_SignUpdate function ID */
#define KMS_SIGN_FINAL_FCT_ID             (KMS_SIGN_UPDATE_FCT_ID+0x01UL)         /*!< C_SignFinal function ID */
#define KMS_SIGN_RECOVER_INIT_FCT_ID      (KMS_SIGN_FINAL_FCT_ID+0x01UL)          /*!< C_SignRecoverInit function ID */
#define KMS_SIGN_RECOVER_FCT_ID           (KMS_SIGN_RECOVER_INIT_FCT_ID+0x01UL)   /*!< C_SignRecover function ID */
#define KMS_VERIFY_INIT_FCT_ID            (KMS_SIGN_RECOVER_FCT_ID+0x01UL)        /*!< C_VerifyInit function ID */
#define KMS_VERIFY_FCT_ID                 (KMS_VERIFY_INIT_FCT_ID+0x01UL)         /*!< C_Verify function ID */
#define KMS_VERIFY_UPDATE_FCT_ID          (KMS_VERIFY_FCT_ID+0x01UL)              /*!< C_VerifyUpdate function ID */
#define KMS_VERIFY_FINAL_FCT_ID           (KMS_VERIFY_UPDATE_FCT_ID+0x01UL)       /*!< C_VerifyFinal function ID */
#define KMS_VERIFY_RECOVER_INIT_FCT_ID    (KMS_VERIFY_FINAL_FCT_ID+0x01UL)        /*!< C_VerifyRecoverInit function ID */
#define KMS_VERIFY_RECOVER_FCT_ID         (KMS_VERIFY_RECOVER_INIT_FCT_ID+0x01UL) /*!< C_VerifyRecover function ID */
#define KMS_DIGEST_ENCRYPT_UPDATE_FCT_ID  (KMS_VERIFY_RECOVER_FCT_ID+0x01UL)      /*!< C_DigestEncryptUpdate function ID */
#define KMS_DECRYPT_DIGEST_UPDATE_FCT_ID  (KMS_DIGEST_ENCRYPT_UPDATE_FCT_ID+0x01UL) /*!< C_DecryptDigestUpdate function ID */
#define KMS_SIGN_ENCRYPT_UPDATE_FCT_ID    (KMS_DECRYPT_DIGEST_UPDATE_FCT_ID+0x01UL) /*!< C_SignEncryptUpdate function ID */
#define KMS_DECRYPT_VERIFY_UPDATE_FCT_ID  (KMS_SIGN_ENCRYPT_UPDATE_FCT_ID+0x01UL) /*!< C_DecryptVerifyUpdate function ID */
#define KMS_GENERATE_KEY_FCT_ID           (KMS_DECRYPT_VERIFY_UPDATE_FCT_ID+0x01UL) /*!< C_GenerateKey function ID */
#define KMS_GENERATE_KEYPAIR_FCT_ID       (KMS_GENERATE_KEY_FCT_ID+0x01UL)        /*!< C_GenerateKeyPair function ID */
#define KMS_WRAP_KEY_FCT_ID               (KMS_GENERATE_KEYPAIR_FCT_ID+0x01UL)    /*!< C_WrapKey function ID */
#define KMS_UNWRAP_KEY_FCT_ID             (KMS_WRAP_KEY_FCT_ID+0x01UL)            /*!< C_UnwrapKey function ID */
#define KMS_DERIVE_KEY_FCT_ID             (KMS_UNWRAP_KEY_FCT_ID+0x01UL)          /*!< C_DeriveKey function ID */
#define KMS_SEED_RANDOM_FCT_ID            (KMS_DERIVE_KEY_FCT_ID+0x01UL)          /*!< C_SeedRandom function ID */
#define KMS_GENERATE_RANDOM_FCT_ID        (KMS_SEED_RANDOM_FCT_ID+0x01UL)         /*!< C_GenerateRandom function ID */
#define KMS_GET_FUNCTION_STATUS_FCT_ID    (KMS_GENERATE_RANDOM_FCT_ID+0x01UL)     /*!< C_GetFunctionStatus function ID */
#define KMS_CANCEL_FUNCTION_FCT_ID        (KMS_GET_FUNCTION_STATUS_FCT_ID+0x01UL) /*!< C_CancelFunction function ID */
#define KMS_WAIT_FOR_SLOT_EVENT_FCT_ID    (KMS_CANCEL_FUNCTION_FCT_ID+0x01UL)     /*!< C_WaitForSlotEvent function ID */
#define KMS_IMPORT_BLOB_FCT_ID            (KMS_WAIT_FOR_SLOT_EVENT_FCT_ID+0x01UL) /*!< C_STM_ImportBlob function ID */
#define KMS_LOCK_KEYS_FCT_ID              (KMS_IMPORT_BLOB_FCT_ID+0x01UL)         /*!< C_STM_LockKeys function ID */
#define KMS_LOCK_SERVICES_FCT_ID          (KMS_LOCK_KEYS_FCT_ID+0x01UL)           /*!< C_STM_LockServices function ID */
#define KMS_LAST_ID_CHECK                 (KMS_LOCK_SERVICES_FCT_ID)              /*!< Last function ID */

/**
  * @brief KMS entry function ID type definition
  */
typedef uint32_t KMS_FunctionID_t;

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/

/** @addtogroup KMS_ENTRY_Exported_Constants Exported Constants
  * @{
  */
#define KMS_FIRST_ID  KMS_INITIALIZE_FCT_ID   /*!< KMS entry first function ID */
#define KMS_LAST_ID   KMS_LAST_ID_CHECK       /*!< KMS entry last function ID */

/* KMS clusters definition */
#define KMS_CLUST_MASK        (0x00FF0000U)         /*!< KMS cluster mask to specify origin of a request */
#define KMS_CLUST_UNSEC       (0x005A0000U)         /*!< Unsecure world cluster, used for messages going through secure enclave entry point */
#define KMS_CLUST_SECX        (0x00A10000U)         /*!< Secure world generic cluster, used for messages coming from the secure enclave or used if there is no secure enclave */

/**
  * @}
  */

/* Exported functions prototypes ---------------------------------------------*/

/** @addtogroup KMS_ENTRY_Exported_Functions Exported Functions
  * @{
  */
CK_RV  KMS_Entry(KMS_FunctionID_t eID, va_list arguments);

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

#endif /* KMS_ENTRY_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

