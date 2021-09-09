/**
  ******************************************************************************
  * @file    kms_objects.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"
#if defined(KMS_ENABLED)
#include "kms_init.h"                   /* KMS session services */
#include "kms_objects.h"                /* KMS object management services */

#include "kms_nvm_storage.h"            /* KMS NVM storage services */
#include "kms_vm_storage.h"             /* KMS VM storage services */
#include "kms_platf_objects.h"          /* KMS platform objects services */
#include "kms_low_level.h"              /* Flash access to read blob */
#include "kms_blob_metadata.h"          /* Blob header definitions */

#include "kms_mem.h"                    /* KMS memory utilities */

#include "kms_digest.h"                 /* KMS digest services for Blob verification */
#include "kms_sign_verify.h"            /* KMS Signature & verifcation services for Blob authentication */
#include "kms_enc_dec.h"                /* KMS encryption & decryption services */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_OBJECTS Blob Objects Management
  * @{
  */

/* Private define ------------------------------------------------------------*/
/** @addtogroup KMS_OBJECTS_Private_Defines Private Defines
  * @{
  */
#if defined(KMS_IMPORT_BLOB_CHUNK_SIZE)
#define KMS_BLOB_CHUNK_SIZE  KMS_IMPORT_BLOB_CHUNK_SIZE   /*!< Blob import working chunk size */
#else /* KMS_IMPORT_BLOB_CHUNK_SIZE */
#define KMS_BLOB_CHUNK_SIZE  512UL                        /*!< Blob import working chunk size */
#endif /* KMS_IMPORT_BLOB_CHUNK_SIZE */

/**
  * @}
  */
/* Private types -------------------------------------------------------------*/
#if defined(KMS_IMPORT_BLOB)
/** @addtogroup KMS_OBJECTS_Private_Types Private Types
  * @{
  */
/**
  * @brief  Blob importation context structure
  */
typedef struct
{
  /*  chunk size is the maximum , the 1st block can be smaller */
  uint8_t fw_decrypted_chunk[KMS_BLOB_CHUNK_SIZE] __attribute__((aligned(4))); /*!< Decrypted chunk buffer */
  uint8_t fw_encrypted_chunk[KMS_BLOB_CHUNK_SIZE]; /*!< Encrypted chunk buffer */
} kms_importblob_ctx_t;
/**
  * @}
  */
#endif /* KMS_IMPORT_BLOB */
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @addtogroup KMS_OBJECTS_Private_Variables Private Variables
  * @{
  */
#if defined(KMS_SE_LOCK_KEYS)
static CK_OBJECT_HANDLE kms_locked_key_handles[KMS_SE_LOCK_KEYS_MAX];     /*!< Locked key handles list */
static uint32_t         kms_locked_key_index = 0;                         /*!< Locked key index */
#endif /* KMS_SE_LOCK_KEYS */
#if defined(KMS_SE_LOCK_SERVICES)
static CK_OBJECT_HANDLE kms_locked_function_id[KMS_SE_LOCK_SERVICES_MAX]; /*!< Locked functions id list */
static uint32_t         kms_locked_function_id_index = 0;                 /*!< Locked functions index */
#endif /* KMS_SE_LOCK_SERVICES */
/**
  * @}
  */
/* Private function prototypes -----------------------------------------------*/
#if defined(KMS_IMPORT_BLOB)
static CK_RV authenticate_blob_header(kms_importblob_ctx_t *pCtx,
                                      KMS_BlobRawHeaderTypeDef *pBlobHeader,
                                      uint8_t *pBlobInFlash);
static CK_RV authenticate_blob(kms_importblob_ctx_t *pCtx,
                               KMS_BlobRawHeaderTypeDef *pBlobHeader,
                               uint8_t *pBlobInFlash);
static CK_RV install_blob(kms_importblob_ctx_t *pCtx,
                          KMS_BlobRawHeaderTypeDef *pBlobHeader,
                          uint8_t *pBlobInFlash);
CK_RV  read_next_chunk(kms_importblob_ctx_t *pCtx,
                       uint32_t session,
                       uint8_t *p_source_address,
                       uint32_t size,
                       uint8_t *p_decrypted_chunk,
                       uint32_t *p_decrypted_size);
#endif /* KMS_IMPORT_BLOB */

/* Private function ----------------------------------------------------------*/

/** @addtogroup KMS_OBJECTS_Private_Functions Private Functions
  * @{
  */
#if defined(KMS_NVM_DYNAMIC_ENABLED) || defined(KMS_VM_DYNAMIC_ENABLED)
/**
  * @brief  Fill CK_ATTRIBUTE TLV elements
  * @param  pTemp       CK_ATTRIBUTE pointer
  * @param  Type        T:Type
  * @param  pValue      V:Value
  * @param  ulValueLen  L:Length
  * @retval None
  */
static inline void fill_TLV(CK_ATTRIBUTE_PTR  pTemp,
                            CK_ATTRIBUTE_TYPE Type,
                            CK_VOID_PTR pValue,
                            CK_ULONG ulValueLen)
{
  pTemp->type = Type;
  pTemp->pValue = pValue;
  pTemp->ulValueLen = ulValueLen;
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED || KMS_VM_DYNAMIC_ENABLED */

#if defined(KMS_IMPORT_BLOB)
/**
  * @brief  Authenticate KMS Blob header
  * @param  pCtx Blob importation context
  * @param  pBlobHeader the blob header
  * @param  pBlobInFlash the blob to import in flash
  * @retval CKR_OK
  *         CKR_FUNCTION_FAILED
  *         CKR_SIGNATURE_INVALID
  *         @ref KMS_OpenSession returned values
  */
static CK_RV authenticate_blob_header(kms_importblob_ctx_t *pCtx,
                                      KMS_BlobRawHeaderTypeDef *pBlobHeader,
                                      uint8_t *pBlobInFlash)
{
  CK_RV e_ret_status;
  CK_RV e_authenticate_status = CKR_SIGNATURE_INVALID;
  CK_SESSION_HANDLE session;
  CK_MECHANISM smech;
  CK_ULONG obj_id_index;
  CK_ULONG blob_hdr_mac_add;
  CK_ULONG blob_hdr_add;

  (void)pCtx;

  /* Open session */
  e_ret_status = KMS_OpenSession(0, CKF_SERIAL_SESSION, NULL, 0, &session);
  if (e_ret_status == CKR_OK)
  {
    /* Verify signature */
    obj_id_index = KMS_PlatfObjects_GetBlobVerifyKey();
    smech.pParameter = NULL;
    smech.ulParameterLen = 0;
    smech.mechanism = CKM_ECDSA_SHA256;

    if (KMS_VerifyInit(session, &smech, obj_id_index) != CKR_OK)
    {
      e_ret_status = CKR_FUNCTION_FAILED;
    }
    else
    {
      blob_hdr_mac_add = (CK_ULONG)(&(pBlobHeader->HeaderMAC[0]));
      blob_hdr_add = (CK_ULONG)pBlobHeader;
      if (KMS_Verify(session,
                     (CK_BYTE_PTR)pBlobHeader, blob_hdr_mac_add - blob_hdr_add,
                     (CK_BYTE_PTR)(uint32_t) &(pBlobHeader->HeaderMAC[0]),
                     (CK_ULONG)KMS_BLOB_MAC_LEN) != CKR_OK)
      {
        e_ret_status = CKR_SIGNATURE_INVALID;
      }
    }
    (void)KMS_CloseSession(session);
  }

  /* Double Check to avoid basic fault injection */
  if (e_ret_status == CKR_OK)
  {
    /* Open session */
    e_ret_status = KMS_OpenSession(0, CKF_SERIAL_SESSION, NULL, 0, &session);
    if (e_ret_status == CKR_OK)
    {
      /* Verify signature */
      obj_id_index = KMS_PlatfObjects_GetBlobVerifyKey();
      smech.pParameter = NULL;
      smech.ulParameterLen = 0;
      smech.mechanism = CKM_ECDSA_SHA256;

      if (KMS_VerifyInit(session, &smech, obj_id_index) != CKR_OK)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      else
      {
        blob_hdr_mac_add = (CK_ULONG)(&(pBlobHeader->HeaderMAC[0]));
        blob_hdr_add = (CK_ULONG)pBlobHeader;
        if (KMS_Verify(session,
                       (CK_BYTE_PTR)pBlobHeader, blob_hdr_mac_add - blob_hdr_add,
                       (CK_BYTE_PTR)(uint32_t) &(pBlobHeader->HeaderMAC[0]),
                       (CK_ULONG)KMS_BLOB_MAC_LEN) == CKR_OK)
        {
          e_authenticate_status = CKR_OK;
        }
      }
      (void)KMS_CloseSession(session);
    }
  }

  return e_authenticate_status;
}
#endif /* KMS_IMPORT_BLOB */

#if defined(KMS_IMPORT_BLOB)
/**
  * @brief  Verify KMS Blob
  * @note   Decrypt is done to verify the Integrity of the Blob.
  * @param  pCtx Blob importation context
  * @param  pBlobHeader the blob header
  * @param  pBlobInFlash the blob to import in flash
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_FUNCTION_FAILED
  *         @ref KMS_DecryptInit returned values
  *         @ref KMS_DecryptUpdate returned values
  *         @ref KMS_DecryptFinal returned values
  *         @ref KMS_DigestInit returned values
  *         @ref KMS_DigestUpdate returned values
  *         @ref KMS_DigestFinal returned values
  *         @ref KMS_LL_FLASH_Read returned values
  */
static CK_RV authenticate_blob(kms_importblob_ctx_t *pCtx,
                               KMS_BlobRawHeaderTypeDef *pBlobHeader,
                               uint8_t *pBlobInFlash)
{
  CK_RV        e_ret_status;
  CK_RV e_authenticate_status = CKR_SIGNATURE_INVALID;
  uint8_t *pfw_source_address = (uint8_t *)0xFFFFFFFFU;
  uint8_t fw_tag_output[KMS_BLOB_TAG_LEN];
  uint32_t fw_decrypted_total_size = 0;
  uint32_t size;
  uint32_t fw_decrypted_chunk_size;
  uint32_t fw_tag_len;
  uint32_t pass_index;
  CK_SESSION_HANDLE aessession;
  CK_SESSION_HANDLE digsession;
  CK_ULONG obj_id_index;
  CK_MECHANISM aesmech;
  CK_MECHANISM digmech;

  if ((pBlobHeader == NULL))
  {
    return CKR_ARGUMENTS_BAD;
  }

  /* Open session */
  if (KMS_OpenSession(0, CKF_SERIAL_SESSION, NULL, 0, &aessession) != CKR_OK)
  {
    return CKR_FUNCTION_FAILED;
  }
  if (KMS_OpenSession(0, CKF_SERIAL_SESSION, NULL, 0, &digsession) != CKR_OK)
  {
    (void)KMS_CloseSession(aessession);
    return CKR_FUNCTION_FAILED;
  }

  pass_index = 0;

  /* Decryption process */
  obj_id_index = KMS_PlatfObjects_GetBlobDecryptKey();
  aesmech.mechanism = CKM_AES_CBC;
  aesmech.pParameter = &(pBlobHeader->InitVector[0]);
  aesmech.ulParameterLen = KMS_BLOB_IV_LEN;

  e_ret_status = KMS_DecryptInit(aessession, &aesmech, obj_id_index);
  if (e_ret_status == CKR_OK)
  {
    /* Digest process */
    digmech.mechanism = CKM_SHA256;
    digmech.pParameter = NULL;
    digmech.ulParameterLen = 0;
    e_ret_status = KMS_DigestInit(digsession, &digmech);
  }

  if (e_ret_status == CKR_OK)
  {
    /* Decryption loop */
    while ((fw_decrypted_total_size < (pBlobHeader->BlobSize)) && (e_ret_status == CKR_OK))
    {
      if (pass_index == 0UL)
      {
        pfw_source_address = pBlobInFlash;
      }
      fw_decrypted_chunk_size = sizeof(pCtx->fw_decrypted_chunk);
      if ((pBlobHeader->BlobSize - fw_decrypted_total_size) < fw_decrypted_chunk_size)
      {
        fw_decrypted_chunk_size = pBlobHeader->BlobSize - fw_decrypted_total_size;
      }
      size = fw_decrypted_chunk_size;

      /* Decrypt Append */
      e_ret_status = KMS_LL_FLASH_Read(pCtx->fw_encrypted_chunk, pfw_source_address, size);

      if (e_ret_status == CKR_OK)
      {
        if (size == 0UL)
        {
          pass_index += 1UL;
        }
        else
        {
          e_ret_status = KMS_DecryptUpdate(aessession,
                                           (CK_BYTE *)pCtx->fw_encrypted_chunk,
                                           size,
                                           (CK_BYTE *)pCtx->fw_decrypted_chunk,
                                           (CK_ULONG_PTR)(uint32_t)&fw_decrypted_chunk_size);
          /* Ensure also that decrypted length is equal to requested one to be sure decryption went well */
          if ((e_ret_status == CKR_OK) && (fw_decrypted_chunk_size == size))
          {
            e_ret_status = KMS_DigestUpdate(digsession, pCtx->fw_decrypted_chunk, fw_decrypted_chunk_size);
            if (e_ret_status == CKR_OK)
            {
              /* Update source pointer */
              pfw_source_address = &pfw_source_address[fw_decrypted_chunk_size];
              fw_decrypted_total_size += fw_decrypted_chunk_size;
              (void)memset(pCtx->fw_decrypted_chunk, 0xff, fw_decrypted_chunk_size);
              pass_index += 1UL;
            }
          }
        }
      }
    }
  }

  if ((e_ret_status == CKR_OK))
  {
    /* Do the finalization, check the authentication TAG */
    fw_tag_len = KMS_BLOB_TAG_LEN; /* PKCS#11 - Section 5.2: Buffer handling compliance */
    e_ret_status =  KMS_DecryptFinal(aessession, fw_tag_output, (CK_ULONG_PTR)(uint32_t)&fw_tag_len);

    if (e_ret_status == CKR_OK)
    {
      fw_tag_len = KMS_BLOB_TAG_LEN; /* PKCS#11 - Section 5.2: Buffer handling compliance */
      e_ret_status = KMS_DigestFinal(digsession, fw_tag_output, (CK_ULONG_PTR)(uint32_t)&fw_tag_len);
      if (e_ret_status == CKR_OK)
      {
        if ((fw_tag_len == KMS_BLOB_TAG_LEN) && (memcmp(fw_tag_output, pBlobHeader->BlobTag, KMS_BLOB_TAG_LEN) == 0))
        {
          e_ret_status = CKR_OK;
        }
      }
    }
  }

  (void)KMS_CloseSession(aessession);
  (void)KMS_CloseSession(digsession);

  if (e_ret_status == CKR_OK)
  {
    if ((fw_tag_len == KMS_BLOB_TAG_LEN) && (memcmp(fw_tag_output, pBlobHeader->BlobTag, KMS_BLOB_TAG_LEN) == 0))
    {
      e_authenticate_status = CKR_OK;
    }
  }

  return e_authenticate_status;
}
#endif /* KMS_IMPORT_BLOB */

#if defined(KMS_IMPORT_BLOB)
/**
  * @brief  Read and decrypt a blob chunk
  * @note   Chunk size is @ref KMS_BLOB_CHUNK_SIZE
  * @param  pCtx Blob importation context
  * @param  session the current KMS session in use for blob decryption
  * @param  p_source_address the current blob decryption address
  * @param  size the size to decrypt
  * @param  p_decrypted_chunk the decrypted chunk
  * @param  p_decrypted_size the decrypted chunk size
  * @retval @ref KMS_DecryptUpdate returned values
  *         @ref KMS_LL_FLASH_Read returned values
  */
CK_RV  read_next_chunk(kms_importblob_ctx_t *pCtx,
                       uint32_t session,
                       uint8_t *p_source_address,
                       uint32_t size,
                       uint8_t *p_decrypted_chunk,
                       uint32_t *p_decrypted_size)
{
  CK_RV        e_ret_status;

  /* Read */
  e_ret_status = KMS_LL_FLASH_Read(pCtx->fw_encrypted_chunk, p_source_address, size);

  if (e_ret_status == CKR_OK)
  {
    e_ret_status = KMS_DecryptUpdate(session,
                                     (CK_BYTE *)pCtx->fw_encrypted_chunk,
                                     size,
                                     (CK_BYTE *)p_decrypted_chunk,
                                     (CK_ULONG_PTR)(uint32_t)&p_decrypted_size);
  }

  return e_ret_status;
}
#endif /* KMS_IMPORT_BLOB */

#if defined(KMS_IMPORT_BLOB)
/**
  * @brief  Install KMS blob
  * @param  pCtx Blob importation context
  * @param  pBlobHeader the blob header
  * @param  pBlobInFlash the blob to import in flash
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_DATA_INVALID
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_FAILED
  *         @ref KMS_DecryptInit returned values
  *         @ref read_next_chunk returned values
  *         @ref KMS_PlatfObjects_NvmStoreObject returned values
  */
static CK_RV install_blob(kms_importblob_ctx_t *pCtx,
                          KMS_BlobRawHeaderTypeDef *pBlobHeader,
                          uint8_t *pBlobInFlash)
{
  CK_RV        e_ret_status;
  CK_SESSION_HANDLE session;
  CK_ULONG obj_id_index;
  CK_MECHANISM aesmech;
  uint8_t *pfw_source_address;
  uint32_t fw_decrypted_total_size = 0;
  uint32_t size;
  uint32_t fw_decrypted_chunk_size;
  uint32_t fw_tag_len;
  uint8_t fw_tag_output[KMS_BLOB_TAG_LEN];
  uint32_t *p_next_magic;
  uint32_t bytes_copied_in_kms;
  uint32_t index_in_decrypted_chunk;
  uint32_t tmp;
  kms_obj_keyhead_no_blob_t blobObject;
  kms_obj_keyhead_t *pBlob = NULL_PTR;


  if (pBlobHeader == NULL)
  {
    return CKR_ARGUMENTS_BAD;
  }

  /* Ensure a decrypted chunk can contain at least one blob header */
  if (sizeof(pCtx->fw_decrypted_chunk) < sizeof(kms_obj_keyhead_no_blob_t))
  {
    return CKR_FUNCTION_FAILED;
  }

  /* Open session */
  if (KMS_OpenSession(0, CKF_SERIAL_SESSION, NULL, 0, &session) != CKR_OK)
  {
    return CKR_FUNCTION_FAILED;
  }

  /* Decryption process */
  obj_id_index = KMS_PlatfObjects_GetBlobDecryptKey();
  aesmech.mechanism = CKM_AES_CBC;
  aesmech.pParameter = &(pBlobHeader->InitVector[0]);
  aesmech.ulParameterLen = KMS_BLOB_IV_LEN;

  e_ret_status = KMS_DecryptInit(session, &aesmech, obj_id_index);
  if (e_ret_status != CKR_OK)
  {
    return e_ret_status;
  }

  /* Start by decrypting first chunk of blob data */
  pfw_source_address = pBlobInFlash;
  fw_decrypted_chunk_size = sizeof(pCtx->fw_decrypted_chunk);
  if (pBlobHeader->BlobSize < fw_decrypted_chunk_size)
  {
    fw_decrypted_chunk_size = pBlobHeader->BlobSize;
  }
  size = fw_decrypted_chunk_size;

  /* Read next chunk */
  e_ret_status = read_next_chunk(pCtx,
                                 session,
                                 pfw_source_address,
                                 size,
                                 pCtx->fw_decrypted_chunk,
                                 &fw_decrypted_chunk_size);
  pfw_source_address = &pfw_source_address[fw_decrypted_chunk_size];
  fw_decrypted_total_size += fw_decrypted_chunk_size;
  index_in_decrypted_chunk = 0;

  p_next_magic = (uint32_t *)(uint32_t)pCtx->fw_decrypted_chunk;

  /* Process the Blob to the end of the last decrypted chunk of the blob  */
  /* fw_decrypted_total_size < (pBlobHeader->BlobSize):
   *  the whole blob has not been yet decrypted */
  /* p_next_magic < (uint32_t *)(&(pCtx->fw_decrypted_chunk[fw_decrypted_chunk_size])):
   *  the whole has been decrypted but there is still some data to parse */
  while (((fw_decrypted_total_size < (pBlobHeader->BlobSize))
          || (p_next_magic < (uint32_t *)(uint32_t)(&(pCtx->fw_decrypted_chunk[fw_decrypted_chunk_size]))))
         && (e_ret_status == CKR_OK))
  {
    /* 1 ---------------------------------------------------------------------------------------------------------- */
    /* Search for first or next magic */
    while ((*p_next_magic != KMS_ABI_VERSION_CK_2_40) && (e_ret_status == CKR_OK))
    {
      /* If we reach the end of the chunk */
      if (p_next_magic == (uint32_t *)(uint32_t) &(pCtx->fw_decrypted_chunk[fw_decrypted_chunk_size]))
      {
        if (fw_decrypted_total_size == (pBlobHeader->BlobSize))
        {
          /* If last chunk decrypted but no next magic, then means no more magic. ==> End of blob reached */
          break;
        }

        /* Read next chunk */
        /* If it is the last access with less than ckunk size to be read */
        if ((pBlobHeader->BlobSize - fw_decrypted_total_size) < fw_decrypted_chunk_size)
        {
          fw_decrypted_chunk_size = pBlobHeader->BlobSize - fw_decrypted_total_size;
        }
        size = fw_decrypted_chunk_size;

        e_ret_status = read_next_chunk(pCtx, session,
                                       pfw_source_address,
                                       size,
                                       pCtx->fw_decrypted_chunk,
                                       &fw_decrypted_chunk_size);
        if (e_ret_status != CKR_OK)
        {
          /* Error while reading next chunk */
          break;
        }
        pfw_source_address = &pfw_source_address[fw_decrypted_chunk_size];
        fw_decrypted_total_size += fw_decrypted_chunk_size;
        index_in_decrypted_chunk = 0;

        p_next_magic = (uint32_t *)(uint32_t)(uint32_t)pCtx->fw_decrypted_chunk;
      }
      else
      {
        p_next_magic++;
        index_in_decrypted_chunk += 4UL;
      }
    } /* ((*p_next_magic != KMS_ABI_VERSION_CK_2_40) && (e_ret_status == CKR_OK)) */

    if ((*p_next_magic == KMS_ABI_VERSION_CK_2_40) && (e_ret_status == CKR_OK))
    {
      /* 2 -------------------------------------------------------------------------------------------------------- */
      /* Copy header into temp header struct */
      if ((fw_decrypted_chunk_size - index_in_decrypted_chunk) > sizeof(kms_obj_keyhead_no_blob_t))
      {
        /* Complete header available into decrypted chunk */

        /* Copy the header of the object */
        (void)memcpy(&blobObject, pCtx->fw_decrypted_chunk + index_in_decrypted_chunk,
                     sizeof(kms_obj_keyhead_no_blob_t));

        index_in_decrypted_chunk = index_in_decrypted_chunk + sizeof(kms_obj_keyhead_no_blob_t);
      }
      else if (fw_decrypted_total_size < (pBlobHeader->BlobSize))
      {
        /* When the end of the Chunk do not contain a full object header */
        /* Copy the beginning with decrypted last bytes */
        /* Then read a chunk */
        /* To complete the header reading */

        /* Just copy the end of the decrypted buffer */
        (void)memcpy(&blobObject, pCtx->fw_decrypted_chunk + index_in_decrypted_chunk,
                     fw_decrypted_chunk_size - index_in_decrypted_chunk);

        bytes_copied_in_kms = fw_decrypted_chunk_size - index_in_decrypted_chunk;

        /* Read the next chunk */
        /* Read next chunk */
        /* If it is the last access with less than ckunk size to be read */
        if ((pBlobHeader->BlobSize - fw_decrypted_total_size) < fw_decrypted_chunk_size)
        {
          fw_decrypted_chunk_size = pBlobHeader->BlobSize - fw_decrypted_total_size;
        }
        size = fw_decrypted_chunk_size;

        if (size < (sizeof(kms_obj_keyhead_no_blob_t) - bytes_copied_in_kms))
        {
          /* Incomplete blob found */
          e_ret_status = CKR_DATA_INVALID;
          break;
        }

        e_ret_status = read_next_chunk(pCtx,
                                       session,
                                       pfw_source_address,
                                       size,
                                       pCtx->fw_decrypted_chunk,
                                       &fw_decrypted_chunk_size);
        if (e_ret_status != CKR_OK)
        {
          /* Error while reading next chunk */
          break;
        }
        pfw_source_address = &pfw_source_address[fw_decrypted_chunk_size];
        fw_decrypted_total_size += fw_decrypted_chunk_size;

        tmp = (uint32_t)(&blobObject);
        tmp += bytes_copied_in_kms;
        (void)memcpy((void *)((uint32_t *)tmp), pCtx->fw_decrypted_chunk,
                     sizeof(kms_obj_keyhead_no_blob_t) - bytes_copied_in_kms);
        index_in_decrypted_chunk = sizeof(kms_obj_keyhead_no_blob_t) - bytes_copied_in_kms ;
      }
      else /* --> fw_decrypted_total_size >= (pBlobHeader->BlobSize) */
      {
        /* Incomplete blob found */
        e_ret_status = CKR_DATA_INVALID;
        break;
      }

      /* 3 -------------------------------------------------------------------------------------------------------- */
      /* Allocate object & retrieve full object */

      pBlob = KMS_Alloc(session, sizeof(kms_obj_keyhead_no_blob_t) + blobObject.blobs_size);
      if (pBlob == NULL_PTR)
      {
        (void)KMS_CloseSession(session);
        return CKR_DEVICE_MEMORY;
      }
      /* Copy header */
      (void)memcpy(pBlob, &blobObject, sizeof(kms_obj_keyhead_no_blob_t));
      /* Reset bytes_copied_in_kms to count blob data bytes */
      bytes_copied_in_kms = 0;

      while ((e_ret_status == CKR_OK) && (pBlob->blobs_size > bytes_copied_in_kms))
      {
        if ((fw_decrypted_chunk_size - index_in_decrypted_chunk) > (pBlob->blobs_size - bytes_copied_in_kms))
        {
          /* Complete blob available into decrypted chunk */

          /* Copy the blob */
          tmp = (uint32_t)(&(pBlob->blobs[0]));
          tmp += bytes_copied_in_kms;
          (void)memcpy((void *)((uint32_t *)(tmp)),
                       pCtx->fw_decrypted_chunk + index_in_decrypted_chunk,
                       pBlob->blobs_size - bytes_copied_in_kms);

          index_in_decrypted_chunk = index_in_decrypted_chunk + pBlob->blobs_size - bytes_copied_in_kms;

          /* Complete blob copied */
          /* Store the object just finalized */
          e_ret_status = KMS_PlatfObjects_NvmStoreObject(pBlob->object_id,
                                                         (uint8_t *)pBlob,
                                                         pBlob->blobs_size + sizeof(kms_obj_keyhead_no_blob_t));
          /* Align Next magic ptr with current reading index */
          p_next_magic = (uint32_t *)(uint32_t) &(pCtx->fw_decrypted_chunk[index_in_decrypted_chunk]);
          KMS_Free(session, pBlob);
          pBlob = NULL_PTR;
          break;
        }
        else if (fw_decrypted_total_size < (pBlobHeader->BlobSize))
        {
          /* When the end of the Chunk do not contain a full blob */
          /* Copy the beginning with decrypted last bytes */
          /* Then read a chunk */
          /* To complete the blob reading */

          /* Just copy the end of the decrypted buffer */
          tmp = (uint32_t)(&(pBlob->blobs[0]));
          tmp += bytes_copied_in_kms;
          (void)memcpy((void *)((uint32_t *)(tmp)),
                       pCtx->fw_decrypted_chunk + index_in_decrypted_chunk,
                       fw_decrypted_chunk_size - index_in_decrypted_chunk);

          bytes_copied_in_kms += fw_decrypted_chunk_size - index_in_decrypted_chunk;

          /* Read next chunk */
          /* If it is the last access with less than ckunk size to be read */
          if ((pBlobHeader->BlobSize - fw_decrypted_total_size) < fw_decrypted_chunk_size)
          {
            fw_decrypted_chunk_size = pBlobHeader->BlobSize - fw_decrypted_total_size;
          }
          size = fw_decrypted_chunk_size;

          e_ret_status = read_next_chunk(pCtx,
                                         session,
                                         pfw_source_address,
                                         size,
                                         pCtx->fw_decrypted_chunk,
                                         &fw_decrypted_chunk_size);
          if (e_ret_status != CKR_OK)
          {
            /* Error while reading next chunk */
            break;
          }
          pfw_source_address = &pfw_source_address[fw_decrypted_chunk_size];
          fw_decrypted_total_size += fw_decrypted_chunk_size;
          index_in_decrypted_chunk = 0;
          /* Align Next magic ptr with current reading index */
          p_next_magic = (uint32_t *)(uint32_t) &(pCtx->fw_decrypted_chunk[index_in_decrypted_chunk]);
        }
        else /* --> fw_decrypted_total_size >= (pBlobHeader->BlobSize) */
        {
          /* Incomplete blob found */
          e_ret_status = CKR_DATA_INVALID;
          break;
        }
      } /* End of: while (pBlob->blobs_size > bytes_copied_in_kms) ... */
    } /* End of: if (*p_next_magic == KMS_ABI_VERSION_CK_2_40) ...  */
  } /* End of: while (fw_decrypted_total_size < (pBlobHeader->BlobSize)) ... */

  if (pBlob != NULL_PTR)
  {
    KMS_Free(session, pBlob);
  }

  if (e_ret_status == CKR_OK)
  {
    /* Do the finalization */
    fw_tag_len = KMS_BLOB_TAG_LEN; /* PKCS#11 - Section 5.2: Buffer handling compliance */
    e_ret_status =  KMS_DecryptFinal(session, fw_tag_output, (CK_ULONG_PTR)(uint32_t)&fw_tag_len);
  }

  (void)KMS_CloseSession(session);
  return e_ret_status;
}
#endif /* KMS_IMPORT_BLOB */

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_OBJECTS_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function returns object pointer from key handle
  * @param  hKey key handle
  * @retval NULL if not found, Object pointer otherwise
  */
kms_obj_keyhead_t *KMS_Objects_GetPointer(CK_OBJECT_HANDLE hKey)
{
  uint32_t min_slot;
  uint32_t max_slot;
  kms_obj_keyhead_t *p_object = NULL;

  /* Check that the key has not been Locked */
  if (KMS_CheckKeyIsNotLocked(hKey) == CKR_OK)
  {
    /* Read the available static slots from the platform */
    KMS_PlatfObjects_EmbeddedRange(&min_slot, &max_slot);
    /* If hKey is in the range of the embedded keys */
    if ((hKey <= max_slot) && (hKey >= min_slot))
    {
      p_object = KMS_PlatfObjects_EmbeddedObject(hKey);
    }

#ifdef KMS_NVM_ENABLED
    /* Read the available nvm slots from the platform */
    KMS_PlatfObjects_NvmStaticRange(&min_slot, &max_slot);
    /* If hKey is in the range of nvm keys */
    if ((hKey <= max_slot) && (hKey >= min_slot))
    {
      p_object = KMS_PlatfObjects_NvmStaticObject(hKey);
    }

#ifdef KMS_NVM_DYNAMIC_ENABLED
    /* Read the available nvm slots from the platform */
    KMS_PlatfObjects_NvmDynamicRange(&min_slot, &max_slot);
    /* If hKey is in the range of nvm keys */
    if ((hKey <= max_slot) && (hKey >= min_slot))
    {
      p_object = KMS_PlatfObjects_NvmDynamicObject(hKey);
    }
#endif  /* KMS_NVM_DYNAMIC_ENABLED */
#endif  /* KMS_NVM_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
    /* Read the available vm slots from the platform */
    KMS_PlatfObjects_VmDynamicRange(&min_slot, &max_slot);
    /* If hKey is in the range of vm keys */
    if ((hKey <= max_slot) && (hKey >= min_slot))
    {
      p_object = KMS_PlatfObjects_VmDynamicObject(hKey);
    }
#endif  /* KMS_VM_DYNAMIC_ENABLED */
  }

  /* Double Check to avoid basic fault injection : check that the key has not been Locked */
  if (KMS_CheckKeyIsNotLocked(hKey) == CKR_OK)
  {
    return p_object;
  }
  else
  {
    /* hKey not in embedded nor in nvm nor in vm, or Locked */
    return NULL;
  }
}

/**
  * @brief  This function returns object range identification from key handle
  * @param  hKey key handle
  * @retval Value within @ref kms_obj_range_t
  */
kms_obj_range_t  KMS_Objects_GetRange(CK_OBJECT_HANDLE hKey)
{
  uint32_t MinSlot;
  uint32_t MaxSlot;

  /* Read the available static slots from the platform */
  KMS_PlatfObjects_EmbeddedRange(&MinSlot, &MaxSlot);
  /* If hKey is in the range of the embedded keys */
  if ((hKey <= MaxSlot) && (hKey >= MinSlot))
  {
    return (KMS_OBJECT_RANGE_EMBEDDED);
  }

#ifdef KMS_NVM_ENABLED
  /* Read the available nvm slots from the platform */
  KMS_PlatfObjects_NvmStaticRange(&MinSlot, &MaxSlot);
  /* If hKey is in the range of nvm keys */
  if ((hKey <= MaxSlot) && (hKey >= MinSlot))
  {
    return (KMS_OBJECT_RANGE_NVM_STATIC_ID);
  }

#ifdef KMS_NVM_DYNAMIC_ENABLED
  /* Read the available nvm slots from the platform */
  KMS_PlatfObjects_NvmDynamicRange(&MinSlot, &MaxSlot);
  /* If hKey is in the range of nvm keys */
  if ((hKey <= MaxSlot) && (hKey >= MinSlot))
  {
    return (KMS_OBJECT_RANGE_NVM_DYNAMIC_ID);
  }
#endif  /* KMS_NVM_DYNAMIC_ENABLED */
#endif  /* KMS_NVM_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
  /* Read the available vm slots from the platform */
  KMS_PlatfObjects_VmDynamicRange(&MinSlot, &MaxSlot);
  /* If hKey is in the range of vm keys */
  if ((hKey <= MaxSlot) && (hKey >= MinSlot))
  {
    return (KMS_OBJECT_RANGE_VM_DYNAMIC_ID);
  }
#endif  /* KMS_VM_DYNAMIC_ENABLED */

#ifdef KMS_EXT_TOKEN_ENABLED
  /* Read the available external token slots from the platform */
  KMS_PlatfObjects_ExtTokenStaticRange(&MinSlot, &MaxSlot);
  /* If hKey is in the range of external token keys */
  if ((hKey <= MaxSlot) && (hKey >= MinSlot))
  {
    return (KMS_OBJECT_RANGE_EXT_TOKEN_STATIC_ID);
  }

  /* Read the available external token slots from the platform */
  KMS_PlatfObjects_ExtTokenDynamicRange(&MinSlot, &MaxSlot);
  /* If hKey is in the range of external token keys */
  if ((hKey <= MaxSlot) && (hKey >= MinSlot))
  {
    return (KMS_OBJECT_RANGE_EXT_TOKEN_DYNAMIC_ID);
  }
#endif  /* KMS_EXT_TOKEN_ENABLED */

  /* hKey not in known ranges */
  return KMS_OBJECT_RANGE_UNKNOWN;
}

/**
  * @brief  This function allow to Lock key usage.
  * @param  hKey key handle
  * @retval CKR_OK if lock successful,
  *         CKR_CANT_LOCK if lock not possible
  *         CKR_FUNCTION_NOT_SUPPORTED
  */
CK_RV  KMS_LockKeyHandle(CK_OBJECT_HANDLE hKey)
{
#if defined(KMS_SE_LOCK_KEYS)
  CK_RV e_ret_status = CKR_CANT_LOCK;
  uint32_t  lock_table_index;

  /* Check that the Handle is not already registered in the table */
  for (lock_table_index = 0; lock_table_index < kms_locked_key_index; lock_table_index++)
  {
    /* If the hKey is already registered, then just set status to stop processing */
    if (kms_locked_key_handles[lock_table_index] == hKey)
    {
      e_ret_status = CKR_OK;
    }
  }
  if (e_ret_status != CKR_OK)
  {
    /* hKey is not already locked,try to lock it */
    /* if the table is full, then return */
    if (kms_locked_key_index >= KMS_SE_LOCK_KEYS_MAX)
    {
      e_ret_status = CKR_CANT_LOCK;
    }
    else
    {
      kms_locked_key_handles[lock_table_index] = hKey;
      kms_locked_key_index++;
      e_ret_status = CKR_OK;
    }
  }

  return e_ret_status;
#else /* KMS_SE_LOCK_KEYS */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SE_LOCK_KEYS */
}

/**
  * @brief  This function allow to check if a key has not been locked.
  * @param  hKey key handle
  * @retval CKR_OK if not Locked
  *         CKR_KEY_HANDLE_INVALID if key not allowed.
  */
CK_RV  KMS_CheckKeyIsNotLocked(CK_OBJECT_HANDLE hKey)
{
#if defined(KMS_SE_LOCK_KEYS)
  CK_RV e_ret_status = CKR_OK;
  uint32_t  lock_table_index;

  /* Check that the Handle is not already registered in the table */
  for (lock_table_index = 0; lock_table_index < kms_locked_key_index; lock_table_index++)
  {
    /* If the hKey is registered, then exit on error */
    if (kms_locked_key_handles[lock_table_index] == hKey)
    {
      e_ret_status = CKR_KEY_HANDLE_INVALID;
    }
  }
  return e_ret_status;
#else /* KMS_SE_LOCK_KEYS */
  return CKR_OK;
#endif /* KMS_SE_LOCK_KEYS */
}

/**
  * @brief  This function allow to Lock service usage.
  * @param  fctId service function ID
  * @retval CKR_OK if lock successful,
  *         CKR_CANT_LOCK if lock not possible
  *         CKR_FUNCTION_NOT_SUPPORTED
  */
CK_RV  KMS_LockServiceFctId(CK_ULONG fctId)
{
#if defined(KMS_SE_LOCK_SERVICES)
  CK_RV e_ret_status = CKR_CANT_LOCK;
  uint32_t  lock_table_index;

  /* Check that the Handle is not already registered in the table */
  for (lock_table_index = 0; lock_table_index < kms_locked_function_id_index; lock_table_index++)
  {
    /* If the fctId is already registered,  then just set status to stop processing */
    if (kms_locked_function_id[lock_table_index] == fctId)
    {
      e_ret_status = CKR_OK;
    }
  }
  if (e_ret_status != CKR_OK)
  {
    /* Service is not already locked,try to lock it */
    /* if the table is full, then return */
    if (kms_locked_function_id_index >= KMS_SE_LOCK_SERVICES_MAX)
    {
      e_ret_status = CKR_CANT_LOCK;
    }
    else
    {
      kms_locked_function_id[lock_table_index] = fctId;
      kms_locked_function_id_index++;
      e_ret_status = CKR_OK;
    }
  }

  return e_ret_status;
#else /* KMS_SE_LOCK_SERVICES */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SE_LOCK_SERVICES */
}

/**
  * @brief  This function allow to check if a service function ID has not been locked.
  * @param  fctId service function ID
  * @retval CKR_OK if not Locked
  *         CKR_ACTION_PROHIBITED if service not allowed.
  */
CK_RV  KMS_CheckServiceFctIdIsNotLocked(CK_ULONG fctId)
{
#if defined(KMS_SE_LOCK_SERVICES)
  CK_RV e_ret_status = CKR_OK;
  uint32_t  lock_table_index;

  /* Check that the Handle is not already registered in the table */
  for (lock_table_index = 0; lock_table_index < kms_locked_function_id_index; lock_table_index++)
  {
    /* If the fctId is registered, then exit on error */
    if (kms_locked_function_id[lock_table_index] == fctId)
    {
      e_ret_status = CKR_ACTION_PROHIBITED;
    }
  }
  return e_ret_status;
#else /* KMS_SE_LOCK_SERVICES */
  return CKR_OK;
#endif /* KMS_SE_LOCK_SERVICES */
}

/**
  * @brief  Search the template for a specific attribute type
  * @param  pTemplate       the template
  * @param  ulCount         size of the template.
  * @param  type            attribute type to search for
  * @param  ppAttr          pointer to found attribute
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_GENERAL_ERROR
  */
CK_RV KMS_FindAttributeInTemplate(CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_ATTRIBUTE_TYPE type,
                                  CK_ATTRIBUTE_PTR *ppAttr)
{
  CK_RV e_ret_status = CKR_GENERAL_ERROR;
  if ((pTemplate == NULL_PTR) || (ulCount == 0UL) || (ppAttr == NULL_PTR))
  {
    e_ret_status = CKR_ARGUMENTS_BAD;
  }
  else
  {
    /* Search within template attributes the given attribute type */
    for (uint32_t i = 0; i < ulCount; i++)
    {
      if (pTemplate[i].type == type)
      {
        *ppAttr = &(pTemplate[i]);
        e_ret_status = CKR_OK;
        break;
      }
    }
  }
  return e_ret_status;
}

/**
  * @brief  Get the object handle of a specific template.
  * @note   look for a byte-to-byte correspondence of the template.
  * @param  hSession        session handle.
  * @param  phObject        gets the object handles.
  * @param  ulMaxCount      max number of objects to return.
  * @param  pulObjectCount  gets the number of objects found.
  * @note   only = 1 is supported if ulCount!=0.
  * @param  pTemplate       the template
  * @param  ulCount         size of the template.
  * @note   if ulCount == 0, retrieve all objects.
  * @retval CKR_OK
  *         CKR_GENERAL_ERROR
  */
CK_RV KMS_FindObjectsFromTemplate(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxCount,
                                  CK_ULONG_PTR pulObjectCount, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
  CK_RV e_ret_status = CKR_OK;
  kms_attr_t *p_attribute;  /* Attribute off an object */
  kms_obj_range_t state;
  CK_ULONG ul_attributes_found_count;
  CK_OBJECT_HANDLE h_object;
  CK_OBJECT_HANDLE h_emb_obj_min, h_emb_obj_max;
#if defined(KMS_NVM_ENABLED)
  CK_OBJECT_HANDLE h_nvms_obj_min, h_nvms_obj_max;
#if defined(KMS_NVM_DYNAMIC_ENABLED)
  CK_OBJECT_HANDLE h_nvmd_obj_min, h_nvmd_obj_max;
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#endif /* KMS_NVM_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
  CK_OBJECT_HANDLE h_vmd_obj_min, h_vmd_obj_max;
#endif /* KMS_VM_DYNAMIC_ENABLED */
  kms_obj_keyhead_t *p_pkms_object;
  uint32_t template_index;
  CK_ULONG_PTR p_working_obj_count;

  (void)hSession;

  /* Init */
  p_working_obj_count = pulObjectCount;
  *p_working_obj_count = 0;

  /* Get Objects handles for all objects allocated (embedded and NVM) */
  KMS_PlatfObjects_EmbeddedRange(&h_emb_obj_min, &h_emb_obj_max);
#if defined(KMS_NVM_ENABLED)
  KMS_PlatfObjects_NvmStaticRange(&h_nvms_obj_min, &h_nvms_obj_max);
#if defined(KMS_NVM_DYNAMIC_ENABLED)
  KMS_PlatfObjects_NvmDynamicRange(&h_nvmd_obj_min, &h_nvmd_obj_max);
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#endif /* KMS_NVM_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
  KMS_PlatfObjects_VmDynamicRange(&h_vmd_obj_min, &h_vmd_obj_max);
#endif /* KMS_VM_DYNAMIC_ENABLED */

  h_object = h_emb_obj_min;
  state = KMS_OBJECT_RANGE_EMBEDDED;
  /* Loop on all local objects */
  while ((*p_working_obj_count < ulMaxCount)
         && (h_object != KMS_HANDLE_KEY_NOT_KNOWN)
         && (e_ret_status == CKR_OK))
  {
    /* Read the key value from the Key Handle                 */
    /* Key Handle is the index to one of static or nvm        */
    p_pkms_object = KMS_Objects_GetPointer(h_object);

    if ((p_pkms_object != NULL) && (e_ret_status == CKR_OK))
    {
      /* User is looking for objects with specific templates */
      if (ulCount > 0UL)
      {
        /* Reset the counter */
        ul_attributes_found_count = 0;

        /* Loop on all the attributes of the input template and
         * look for the corresponding attribute in the embedded template */
        for (template_index = 0; template_index < ulCount; template_index++)
        {
          /* Check for the specific attribute  */
          if (KMS_Objects_SearchAttributes(pTemplate[template_index].type, p_pkms_object, &p_attribute) == CKR_OK)
          {
            /* CKA ATTRIBUTE attribute found in the embedded object - now compare the two values */
            if ((p_attribute->size == pTemplate[template_index].ulValueLen)
                && (memcmp(p_attribute->data, pTemplate[template_index].pValue, p_attribute->size) == 0))
            {
              /* Attribute found and the value is identical - increase the number of attributes found */
              ul_attributes_found_count++;
            }
          }
        }

        /* Check if all the attributes were found  meaning a match has been found */
        if (ul_attributes_found_count == ulCount)
        {
          /* Increment number of objects found */
          phObject[*p_working_obj_count] = h_object;
          *p_working_obj_count = *p_working_obj_count + 1;
        }
      }
      else
      {
        /* Return all objects */
        phObject[*p_working_obj_count] = h_object;
        *p_working_obj_count = *p_working_obj_count + 1;
      }
    }

    h_object++;
    /* Change object range if needed */
    switch (state)
    {
      case KMS_OBJECT_RANGE_EMBEDDED:
        if (h_object > h_emb_obj_max)
        {
#if defined(KMS_NVM_ENABLED)
          /* Reached end of range, go to next one */
          state = KMS_OBJECT_RANGE_NVM_STATIC_ID;
          h_object = h_nvms_obj_min;
#elif defined(KMS_VM_DYNAMIC_ENABLED)
          /* Reached end of range, go to next one */
          state = KMS_OBJECT_RANGE_VM_DYNAMIC_ID;
          h_object = h_vmd_obj_min;
#else /* KMS_NVM_ENABLED */
          /* Reached end of range, stop loop */
          h_object = KMS_HANDLE_KEY_NOT_KNOWN;
#endif /* KMS_NVM_ENABLED */
        }
        break;
#if defined(KMS_NVM_ENABLED)
      case KMS_OBJECT_RANGE_NVM_STATIC_ID:
        if (h_object > h_nvms_obj_max)
        {
#if defined(KMS_NVM_DYNAMIC_ENABLED)
          /* Reached end of range, go to next one */
          state = KMS_OBJECT_RANGE_NVM_DYNAMIC_ID;
          h_object = h_nvmd_obj_min;
#elif defined(KMS_VM_DYNAMIC_ENABLED)
          /* Reached end of range, go to next one */
          state = KMS_OBJECT_RANGE_VM_DYNAMIC_ID;
          h_object = h_vmd_obj_min;
#else /* KMS_NVM_DYNAMIC_ENABLED */
          /* Reached end of range, stop loop */
          h_object = KMS_HANDLE_KEY_NOT_KNOWN;
#endif /* KMS_NVM_DYNAMIC_ENABLED */
        }
        break;
#if defined(KMS_NVM_DYNAMIC_ENABLED)
      case KMS_OBJECT_RANGE_NVM_DYNAMIC_ID:
        if (h_object > h_nvmd_obj_max)
        {
#ifdef KMS_VM_DYNAMIC_ENABLED
          /* Reached end of range, go to next one */
          state = KMS_OBJECT_RANGE_VM_DYNAMIC_ID;
          h_object = h_vmd_obj_min;
#else /* KMS_VM_DYNAMIC_ENABLED */
          /* Reached end of range, stop loop */
          h_object = KMS_HANDLE_KEY_NOT_KNOWN;
#endif /* KMS_VM_DYNAMIC_ENABLED */
        }
        break;
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#endif /* KMS_NVM_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
      case KMS_OBJECT_RANGE_VM_DYNAMIC_ID:
        if (h_object > h_vmd_obj_max)
        {
          /* Reached end of range, stop loop */
          h_object = KMS_HANDLE_KEY_NOT_KNOWN;
        }
        break;
#endif /* KMS_VM_DYNAMIC_ENABLED */
      default:
        e_ret_status = CKR_GENERAL_ERROR;
        break;
    }
  }

  return CKR_OK;
}

/**
  * @brief  This function search for an attribute in an blob
  * @param  SearchedId attribute ID to search
  * @param  pKmsKeyHead blob object to search in
  * @param  pAttribute found attribute
  * @retval CKR_OK if attribute is found
  *         CKR_ATTRIBUTE_TYPE_INVALID otherwise
  */
CK_RV KMS_Objects_SearchAttributes(uint32_t SearchedId, kms_obj_keyhead_t *pKmsKeyHead, kms_attr_t **pAttribute)
{
  CK_RV e_ret_status = CKR_ATTRIBUTE_TYPE_INVALID;
  kms_attr_t *pkms_blob_current = (kms_attr_t *)(uint32_t)(pKmsKeyHead->blobs);
  uint8_t  *pkms_blob;
  uint32_t blob_index;
  uint32_t current_attribute_size;

  /* The blob containing the attributes is described by the kms_obj_keyhead_t */
  for (blob_index = 0; blob_index < pKmsKeyHead->blobs_count; blob_index++)
  {
    /* Parse the attributes from the blob to find the id */
    if (pkms_blob_current->id == SearchedId)
    {
      *pAttribute = pkms_blob_current;

      /* ID is found */
      e_ret_status = CKR_OK;
      break;
    }
    current_attribute_size = pkms_blob_current->size ;
    /* When size is not a multiple of 4, we have to consider 4 bytes  alignment */
    if ((current_attribute_size % 4UL) != 0UL)
    {
      current_attribute_size += 4UL - (current_attribute_size % 4UL);
    }

    /* Point to next attribute */
    pkms_blob = &((uint8_t *)pkms_blob_current)[4UL + 4UL + current_attribute_size];

    /* Point to the next Attribute */
    pkms_blob_current = (kms_attr_t *)(uint32_t)pkms_blob;
  }
  return e_ret_status;
}

/**
  * @brief  This function transposes a value from u8[] to an u32[] value
  * @note   Storage is as follow:
  *         u8[]  = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}
  *         u32[] = {0x01020304, 0x05060708}
  * @param  pU8 u8 buffer containing the value to translate
  * @param  u8Size u8 buffer size (in bytes)
  * @param  pU32 translated buffer
  * @retval None
  */
void KMS_Objects_u8ptr_2_BlobU32(uint8_t *pU8, uint32_t u8Size, uint32_t *pU32)
{
  uint32_t index;
  uint32_t reste = u8Size & 0x3UL;
  for (index = 0; index < ((u8Size) / 4UL); index++)
  {
    pU32[index] = ((uint32_t)pU8[(index * 4UL) + 0UL] << 24) + \
                  ((uint32_t)pU8[(index * 4UL) + 1UL] << 16) + \
                  ((uint32_t)pU8[(index * 4UL) + 2UL] << 8) + \
                  ((uint32_t)pU8[(index * 4UL) + 3UL]);
  }
  if (reste != 0UL)     /* check that we have a multiple of 4 bytes */
  {
    if (reste == 1UL)
    {
      /* One byte remains, convention is as follow:
       *         u8[]  = {0x01}
       *         u32[] = {0x00000001} */
      pU32[index] = ((uint32_t)pU8[(index * 4UL) + 0UL]);
    }
    if (reste == 2UL)
    {
      /* Two bytes remains, convention is as follow:
       *         u8[]  = {0x01, 0x02}
       *         u32[] = {0x00000102} */
      pU32[index] = ((uint32_t)pU8[(index * 4UL) + 0UL] << 8) + \
                    ((uint32_t)pU8[(index * 4UL) + 1UL]);
    }
    if (reste == 3UL)
    {
      /* Three bytes remains, convention is as follow:
       *         u8[]  = {0x01, 0x02, 0x03}
       *         u32[] = {0x00010203} */
      pU32[index] = ((uint32_t)pU8[(index * 4UL) + 0UL] << 16) + \
                    ((uint32_t)pU8[(index * 4UL) + 1UL] << 8) + \
                    ((uint32_t)pU8[(index * 4UL) + 2UL]);
    }
  }
}

/**
  * @brief  This function transposes a value from u32[] to an u8[] value
  * @note   Storage is as follow:
  *         u8[]  = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}
  *         u32[] = {0x01020304, 0x05060708}
  * @param  pU32 u32 buffer containing the value to translate (in bytes)
  * @param  u32Size u32 buffer size
  * @param  pU8 translated buffer
  * @retval None
  */
void  KMS_Objects_BlobU32_2_u8ptr(uint32_t *pU32, uint32_t u32Size, uint8_t *pU8)
{
  uint32_t index_key;
  uint32_t reste = u32Size & 0x3UL;

  for (index_key = 0; index_key < (u32Size / sizeof(uint32_t)); index_key++)
  {
    pU8[(index_key * sizeof(uint32_t))]       = (uint8_t)(pU32[index_key] >> 24);
    pU8[(index_key * sizeof(uint32_t)) + 1UL] = (uint8_t)(pU32[index_key] >> 16);
    pU8[(index_key * sizeof(uint32_t)) + 2UL] = (uint8_t)(pU32[index_key] >> 8);
    pU8[(index_key * sizeof(uint32_t)) + 3UL] = (uint8_t)(pU32[index_key]);
  }

  if (reste != 0UL)     /* check that we have a multiple of 4 bytes */
  {
    if (reste == 1UL)
    {
      /* One byte remains, convention is as follow:
       *         u8[]  = {0x01}
       *         u32[] = {0x00000001} */
      pU8[(index_key * sizeof(uint32_t))]   = (uint8_t)(pU32[index_key]);
    }

    if (reste == 2UL)
    {
      /* Two bytes remains, convention is as follow:
       *         u8[]  = {0x01, 0x02}
       *         u32[] = {0x00000102} */
      pU8[(index_key * sizeof(uint32_t))]   = (uint8_t)(pU32[index_key] >> 8);
      pU8[(index_key * sizeof(uint32_t)) + 1UL] = (uint8_t)(pU32[index_key]);
    }

    if (reste == 3UL)
    {
      /* Three bytes remains, convention is as follow:
       *         u8[]  = {0x01, 0x02, 0x03}
       *         u32[] = {0x00010203} */
      pU8[(index_key * sizeof(uint32_t))]       = (uint8_t)(pU32[index_key] >> 16);
      pU8[(index_key * sizeof(uint32_t)) + 1UL] = (uint8_t)(pU32[index_key] >> 8);
      pU8[(index_key * sizeof(uint32_t)) + 2UL] = (uint8_t)(pU32[index_key]);
    }
  }
}

/**
  * @brief  This function imports an encrypted blob into the NVM storage
  * @note   It ensures authentication, verification and decryption of the blob
  * @param  pHdr is the pointer to the encrypted blob header
  * @param  pFlash is the pointer to the blob location in flash
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_GENERAL_ERROR
  *         CKR_OPERATION_ACTIVE
  *         @ref authenticate_blob_header returned values
  *         @ref authenticate_blob returned values
  *         @ref install_blob returned values
  */
CK_RV  KMS_Objects_ImportBlob(CK_BYTE_PTR pHdr, CK_BYTE_PTR pFlash)
{
#if defined(KMS_IMPORT_BLOB)
  CK_RV e_ret_status = CKR_GENERAL_ERROR;
  CK_RV e_install_status = CKR_GENERAL_ERROR;
  uint32_t session_index;
  kms_importblob_ctx_t *p_ctx;

  if (!KMS_IS_INITIALIZED())
  {
    e_ret_status = CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  else
  {
    /* First ensure there is no session in use prior to update keys with blob contents */
    session_index = 1;
    do
    {
      if (KMS_GETSESSION(session_index).state != KMS_SESSION_NOT_USED)
      {
        break;
      }
      session_index++;
    } while (session_index <= KMS_NB_SESSIONS_MAX); /* Session index are going from 1 to KMS_NB_SESSIONS_MAX */

    if (session_index <= KMS_NB_SESSIONS_MAX)
    {
      /* Session(s) in use, return error */
      e_ret_status = CKR_OPERATION_ACTIVE;
    }
    else
    {
      p_ctx = KMS_Alloc(KMS_SESSION_ID_INVALID, sizeof(kms_importblob_ctx_t));
      if (p_ctx == NULL)
      {
        e_ret_status = CKR_DEVICE_MEMORY;
      }
      else
      {
        /* Blob header Authentication */
        e_ret_status = authenticate_blob_header(p_ctx, (KMS_BlobRawHeaderTypeDef *)(uint32_t)pHdr, pFlash);
        if (e_ret_status == CKR_OK)
        {
          /* Blob Authentication */
          e_ret_status = authenticate_blob(p_ctx, (KMS_BlobRawHeaderTypeDef *)(uint32_t)pHdr, pFlash);

          /* Key Install */
          /* Check that Blob Authentication is OK */
          if (e_ret_status == CKR_OK)
          {
            /* Double Check to avoid basic fault injection */
            e_ret_status = authenticate_blob_header(p_ctx, (KMS_BlobRawHeaderTypeDef *)(uint32_t)pHdr, pFlash);

            /* Check the Blob header Authentication */
            if (e_ret_status == CKR_OK)
            {
              /* Read the Blob & Install it in NVM */
              e_install_status = install_blob(p_ctx, (KMS_BlobRawHeaderTypeDef *)(uint32_t)pHdr, pFlash);
            }
          }
        }
        KMS_Free(KMS_SESSION_ID_INVALID, p_ctx);
      }
    }
  }
  return e_install_status;
#else /* KMS_IMPORT_BLOB */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_IMPORT_BLOB */
}

/**
  * @brief  This function locks the specified keys
  * @param  pKeys Pointer to key handles to be locked
  * @param  ulCount Number of keys to lock
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_FUNCTION_NOT_SUPPORTED
  */
CK_RV KMS_Objects_LockKeys(CK_OBJECT_HANDLE_PTR pKeys, CK_ULONG ulCount)
{
#if defined(KMS_SE_LOCK_KEYS)
  if ((pKeys == NULL_PTR) || (ulCount == 0UL))
  {
    return CKR_ARGUMENTS_BAD;
  }
  for (uint32_t i = 0; i < ulCount; i++)
  {
    (void)KMS_LockKeyHandle(pKeys[i]);
  }
  return CKR_OK;
#else /* KMS_SE_LOCK_KEYS */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SE_LOCK_KEYS */
}

/**
  * @brief  This function locks the specified services
  * @param  pServices Pointer to services function identifier to be locked
  * @param  ulCount Number of services to lock
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_FUNCTION_NOT_SUPPORTED
  */
CK_RV KMS_Objects_LockServices(CK_ULONG_PTR pServices, CK_ULONG ulCount)
{
#if defined(KMS_SE_LOCK_SERVICES)
  if ((pServices == NULL_PTR) || (ulCount == 0UL))
  {
    return CKR_ARGUMENTS_BAD;
  }
  for (uint32_t i = 0; i < ulCount; i++)
  {
    (void)KMS_LockServiceFctId(pServices[i]);
  }
  return CKR_OK;
#else /* KMS_SE_LOCK_SERVICES */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SE_LOCK_SERVICES */
}

#if defined(KMS_NVM_DYNAMIC_ENABLED) || defined(KMS_VM_DYNAMIC_ENABLED)
/**
  * @brief  Allocate and create a blob object from one or two template(s)
  * @param  hSession session handle
  * @param  pTemplate1 object template #1
  * @param  ulCount1 attributes count in the template #1
  * @param  pTemplate2 object template #2 (optional)
  * @param  ulCount2 attributes count in the template #2 (optional)
  * @param  phObject created object handle
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_DEVICE_MEMORY
  *         @ref KMS_PlatfObjects_AllocateAndStore returned values
  */
CK_RV KMS_Objects_CreateNStoreBlobFromTemplates(CK_SESSION_HANDLE hSession,
                                                CK_ATTRIBUTE_PTR pTemplate1,
                                                CK_ULONG ulCount1,
                                                CK_ATTRIBUTE_PTR pTemplate2,
                                                CK_ULONG ulCount2,
                                                CK_OBJECT_HANDLE_PTR phObject)
{
  CK_RV e_ret_status;
  uint32_t blob_size;
  uint32_t offset;
  uint32_t *ptr;
  uint32_t tmp;
  kms_obj_keyhead_no_blob_t *p_blob;

  if ((pTemplate1 == NULL_PTR) || (ulCount1 == 0UL) || (phObject == NULL_PTR)
      || ((pTemplate2 == NULL_PTR) && (ulCount2 != 0UL)) || ((pTemplate2 != NULL_PTR) && (ulCount2 == 0UL)))
  {
    return CKR_ARGUMENTS_BAD;
  }

  /* Calculate blob size */
  blob_size = 0;
  for (uint32_t i = 0; i < ulCount1; i++)
  {
    /* pValue size align on 4 bytes */
    blob_size += (pTemplate1[i].ulValueLen & 0xFFFFFFFCUL)
                 + (((pTemplate1[i].ulValueLen & 0x3UL) != 0UL) ? 4UL : 0UL);
  }

  /* Add for each attribute type & length fields */
  blob_size += ulCount1 * 2UL * sizeof(uint32_t);
  if ((ulCount2 != 0UL) && (pTemplate2 != NULL_PTR))
  {
    for (uint32_t i = 0; i < ulCount2; i++)
    {
      /* pValue size align on 4 bytes */
      blob_size += (pTemplate2[i].ulValueLen & 0xFFFFFFFCUL)
                   + (((pTemplate2[i].ulValueLen & 0x3UL) != 0UL) ? 4UL : 0UL);
    }
    /* Add for each attribute type & length fields */
    blob_size += ulCount2 * 2UL * sizeof(uint32_t);
  }

  /* Allocate blob object */
  p_blob = (kms_obj_keyhead_no_blob_t *)KMS_Alloc(hSession, sizeof(kms_obj_keyhead_no_blob_t) + blob_size);
  if (p_blob == NULL_PTR)
  {
    e_ret_status = CKR_DEVICE_MEMORY;
  }
  else
  {
    p_blob->version = KMS_ABI_VERSION_CK_2_40;
    p_blob->configuration = KMS_ABI_CONFIG_KEYHEAD;
    p_blob->blobs_size = blob_size;
    p_blob->blobs_count = ulCount1 + ulCount2;
    p_blob->object_id = KMS_HANDLE_KEY_NOT_KNOWN;    /* Updated when inserting object in NVM / VM */

    offset = 0;
    tmp = (uint32_t)(p_blob);
    ptr = (uint32_t *)(tmp + sizeof(kms_obj_keyhead_no_blob_t));

    /* Copy Template in blob, size and pValue are reversed, memcpy is impossible */
    for (uint32_t i = 0; i < ulCount1; i++)
    {
      ptr[(3UL * i) + offset] = pTemplate1[i].type;
      ptr[(3UL * i) + 1UL + offset] = pTemplate1[i].ulValueLen;
      (void)memcpy((uint8_t *) & (ptr[(3UL * i) + 2UL + offset]), (uint8_t *)(pTemplate1[i].pValue),
                   pTemplate1[i].ulValueLen);
      offset += (pTemplate1[i].ulValueLen - 1UL) / 4UL;
    }

    /* Object template #2 management when required by the caller */
    if ((ulCount2 != 0UL) && (pTemplate2 != NULL_PTR))
    {
      for (uint32_t i = 0; i < ulCount2; i++)
      {
        ptr[(3UL * (i + ulCount1)) + offset] = pTemplate2[i].type;
        ptr[(3UL * (i + ulCount1)) + 1UL + offset] = pTemplate2[i].ulValueLen;
        (void)memcpy((uint8_t *) & (ptr[(3UL * (i + ulCount1)) + 2UL + offset]), (uint8_t *)(pTemplate2[i].pValue),
                     pTemplate2[i].ulValueLen);
        offset += (pTemplate2[i].ulValueLen - 1UL) / 4UL;
      }
    }

    e_ret_status = KMS_PlatfObjects_AllocateAndStore(p_blob, phObject);
    KMS_Free(hSession, p_blob);
  }

  return e_ret_status;
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED || KMS_VM_DYNAMIC_ENABLED */

#if defined(KMS_NVM_DYNAMIC_ENABLED) || defined(KMS_VM_DYNAMIC_ENABLED)
/**
  * @brief  Create and store blob for AES key
  * @param  hSession session handle
  * @param  pKey AES key
  * @param  keySize AES key size
  * @param  pTemplate additional template to concatenate
  * @param  ulCount additional template attribute ulCount
  * @param  phObject created object handle
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_DEVICE_MEMORY
  *         @ref KMS_Objects_CreateNStoreBlobFromTemplates returned values
  */
CK_RV KMS_Objects_CreateNStoreBlobForAES(CK_SESSION_HANDLE hSession,
                                         uint8_t *pKey,
                                         uint32_t keySize,
                                         CK_ATTRIBUTE_PTR pTemplate,
                                         CK_ULONG ulCount,
                                         CK_OBJECT_HANDLE_PTR phObject)
{
  static CK_OBJECT_CLASS vCKO_SECRET_KEY = CKO_SECRET_KEY;
  static CK_KEY_TYPE vCKK_AES = CKK_AES;
  /* AES template must contain:
   *  - CKA_CLASS
   *  - CKA_KEY_TYPE
   *  - CKA_VALUE
   *  - CKA_DESTROYABLE (Optional: default = TRUE)
   *  - CKA_EXTRACTABLE (Optional: default = FALSE)
   */
  CK_RV e_ret_status;
  CK_ATTRIBUTE template[3];
  uint32_t *trans_key;

  if ((pKey == NULL_PTR) || (keySize == 0UL) || (phObject == NULL_PTR)
      || ((pTemplate == NULL_PTR) && (ulCount != 0UL))
      || ((pTemplate != NULL_PTR) && (ulCount == 0UL)))
  {
    e_ret_status = CKR_ARGUMENTS_BAD;
  }
  else
  {
    trans_key = KMS_Alloc(hSession, keySize); /* AES keys shall be multiple of 4 length */
    if (trans_key == NULL_PTR)
    {
      e_ret_status = CKR_DEVICE_MEMORY;
    }
    else
    {
      /* Convert key from u8 to u32 */
      KMS_Objects_u8ptr_2_BlobU32(pKey, keySize, trans_key);

      /* Prepare AES standard template */
      fill_TLV(&(template[0]), CKA_CLASS, (void *)&vCKO_SECRET_KEY, sizeof(CK_OBJECT_CLASS));
      fill_TLV(&(template[1]), CKA_KEY_TYPE, (void *)&vCKK_AES, sizeof(CK_KEY_TYPE));
      fill_TLV(&(template[2]), CKA_VALUE, trans_key, keySize);

      /* Create and store blob object including additional user template */
      e_ret_status = KMS_Objects_CreateNStoreBlobFromTemplates(hSession,
                                                               &(template[0]),
                                                               3,
                                                               pTemplate,
                                                               ulCount,
                                                               phObject);

      KMS_Free(hSession, trans_key);
    }
  }
  return e_ret_status;
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED || KMS_VM_DYNAMIC_ENABLED */

#if defined(KMS_NVM_DYNAMIC_ENABLED) || defined(KMS_VM_DYNAMIC_ENABLED)
/**
  * @brief  Create and store blob for ECC key pair
  * @param  hSession session handle
  * @param  pKeyPair key pair
  * @param  pPubTemplate additional public key template to concatenate
  * @param  ulPubCount additional public key template attribute count
  * @param  pPrivTemplate additional private key template to concatenate
  * @param  ulPrivCount additional private key template attribute count
  * @param  phPubObject created object handle for public key
  * @param  phPrivObject created object handle for private key
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_DEVICE_MEMORY
  *         @ref KMS_Objects_CreateNStoreBlobFromTemplates returned values
  */
CK_RV KMS_Objects_CreateNStoreBlobForECCPair(CK_SESSION_HANDLE hSession,
                                             kms_obj_key_pair_t *pKeyPair,
                                             CK_ATTRIBUTE_PTR pPubTemplate,
                                             CK_ULONG ulPubCount,
                                             CK_ATTRIBUTE_PTR pPrivTemplate,
                                             CK_ULONG ulPrivCount,
                                             CK_OBJECT_HANDLE_PTR phPubObject,
                                             CK_OBJECT_HANDLE_PTR phPrivObject)
{
  static CK_OBJECT_CLASS vCKO_PUBLIC_KEY = CKO_PUBLIC_KEY;
  static CK_OBJECT_CLASS vCKO_PRIVATE_KEY = CKO_PRIVATE_KEY;
  static CK_KEY_TYPE vCKK_EC = CKK_EC;
  static CK_BBOOL    vCK_TRUE = CK_TRUE;
  /* ECC public key template must contain:
   *  - CKA_CLASS
   *  - CKA_KEY_TYPE
   *  - CKA_EC_POINT
   *  - CKA_LOCAL (=TRUE)
   *
   * ECC private key template must contain:
   *  - CKA_CLASS
   *  - CKA_KEY_TYPE
   *  - CKA_EC_PARAMS
   *  - CKA_VALUE
   *  - CKA_LOCAL (=TRUE)
   */
  CK_RV e_ret_status;
  CK_ATTRIBUTE template[5];
  uint32_t *trans_key;

  if ((pKeyPair->pPub == NULL_PTR) || (pKeyPair->pubSize == 0UL)
      || (pKeyPair->pPriv == NULL_PTR) || (pKeyPair->privSize == 0UL)
      || (pPubTemplate == NULL_PTR) || (ulPubCount == 0UL)
      || (pPrivTemplate == NULL_PTR) || (ulPrivCount == 0UL)
      || (phPubObject == NULL_PTR) || (phPrivObject == NULL_PTR))
  {
    e_ret_status = CKR_ARGUMENTS_BAD;
  }
  else
  {
    /* Process public key - ensure 4 bytes granularity allocation to store u32 */
    trans_key = KMS_Alloc(hSession, pKeyPair->pubSize + (((pKeyPair->pubSize & 0x3UL) != 0UL) ? 4UL : 0UL));
    if (trans_key == NULL_PTR)
    {
      e_ret_status = CKR_DEVICE_MEMORY;
    }
    else
    {
      /* Convert key from u8 to u32 */
      KMS_Objects_u8ptr_2_BlobU32(pKeyPair->pPub, pKeyPair->pubSize, trans_key);

      /* Prepare public key standard template */
      fill_TLV(&(template[0]), CKA_CLASS, (void *)&vCKO_PUBLIC_KEY, sizeof(CK_OBJECT_CLASS));
      fill_TLV(&(template[1]), CKA_KEY_TYPE, (void *)&vCKK_EC, sizeof(CK_KEY_TYPE));
      fill_TLV(&(template[2]), CKA_EC_POINT, trans_key, pKeyPair->pubSize);
      fill_TLV(&(template[3]), CKA_LOCAL, (void *)&vCK_TRUE, sizeof(CK_BBOOL));

      /* Create and store blob object including additional user template */
      e_ret_status = KMS_Objects_CreateNStoreBlobFromTemplates(hSession,
                                                               &(template[0]),
                                                               4,
                                                               pPubTemplate,
                                                               ulPubCount,
                                                               phPubObject);

      KMS_Free(hSession, trans_key);
    }
    if (e_ret_status == CKR_OK)
    {
      /* Process private key - ensure 4 bytes granularity allocation to store u32 */
      trans_key = KMS_Alloc(hSession, pKeyPair->privSize + (((pKeyPair->privSize & 0x3UL) != 0UL) ? 4UL : 0UL));
      if (trans_key == NULL_PTR)
      {
        e_ret_status = CKR_DEVICE_MEMORY;
      }
      else
      {
        /* Convert key from u8 to u32 */
        KMS_Objects_u8ptr_2_BlobU32(pKeyPair->pPriv, pKeyPair->privSize, trans_key);

        /* Prepare private key standard template */
        fill_TLV(&(template[0]), CKA_CLASS, (void *)&vCKO_PRIVATE_KEY, sizeof(CK_OBJECT_CLASS));
        fill_TLV(&(template[1]), CKA_KEY_TYPE, (void *)&vCKK_EC, sizeof(CK_KEY_TYPE));
        for (uint32_t i = 0; i < ulPubCount; i++)
        {
          if (pPubTemplate[i].type == CKA_EC_PARAMS)
          {
            fill_TLV(&(template[2]), CKA_EC_PARAMS, pPubTemplate[i].pValue, pPubTemplate[i].ulValueLen);
            break;
          }
        }
        fill_TLV(&(template[3]), CKA_VALUE, trans_key, pKeyPair->privSize);
        fill_TLV(&(template[4]), CKA_LOCAL, (void *)&vCK_TRUE, sizeof(CK_BBOOL));

        /* Create and store blob object including additional user template */
        e_ret_status = KMS_Objects_CreateNStoreBlobFromTemplates(hSession,
                                                                 &(template[0]),
                                                                 5,
                                                                 pPrivTemplate,
                                                                 ulPrivCount,
                                                                 phPrivObject);

        KMS_Free(hSession, trans_key);
      }
    }
  }
  return e_ret_status;
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED || KMS_VM_DYNAMIC_ENABLED */

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
