/**
  ******************************************************************************
  * @file    kms_platf_objects.c
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module platform objects management
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
#include "kms.h"                          /* PKCS11 definitions */
#if defined(KMS_ENABLED)
#include "kms_init.h"                     /* KMS session services */
#include "kms_platf_objects.h"            /* KMS platform objects services */
#include "kms_nvm_storage.h"              /* KMS NVM storage services */
#include "kms_vm_storage.h"               /* KMS VM storage services */
#define KMS_PLATF_OBJECTS_C
#include "kms_platf_objects_config.h"     /* KMS embedded objects definitions */

/*
 * Key ranges verification
 */
#if (KMS_INDEX_MIN_EMBEDDED_OBJECTS > KMS_INDEX_MAX_EMBEDDED_OBJECTS)
#error "Embedded objects index min and max are not well ordered"
#endif /* KMS_INDEX_MIN_EMBEDDED_OBJECTS > KMS_INDEX_MAX_EMBEDDED_OBJECTS */
#ifdef KMS_NVM_ENABLED
#if (KMS_INDEX_MIN_NVM_STATIC_OBJECTS > KMS_INDEX_MAX_NVM_STATIC_OBJECTS)
#error "NVM static ID objects index min and max are not well ordered"
#endif /* KMS_INDEX_MIN_NVM_STATIC_OBJECTS > KMS_INDEX_MAX_NVM_STATIC_OBJECTS */
#if (KMS_INDEX_MAX_EMBEDDED_OBJECTS >= KMS_INDEX_MIN_NVM_STATIC_OBJECTS)
#error "NVM static IDs & Embedded ranges are overlapping"
#endif /* KMS_INDEX_MAX_EMBEDDED_OBJECTS >= KMS_INDEX_MIN_NVM_STATIC_OBJECTS */
#if (KMS_NVM_SLOT_NUMBERS < (KMS_INDEX_MAX_NVM_STATIC_OBJECTS-KMS_INDEX_MIN_NVM_STATIC_OBJECTS+1))
#error "Not enough slot declared in KMS_NVM_SLOT_NUMBERS to store all allowed NVM Static IDs objects"
#endif /* KMS_NVM_SLOT_NUMBERS < (KMS_INDEX_MAX_NVM_STATIC_OBJECTS-KMS_INDEX_MIN_NVM_STATIC_OBJECTS+1) */
#ifdef KMS_NVM_DYNAMIC_ENABLED
#if (KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS > KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS)
#error "NVM dynamic ID objects index min and max are not well ordered"
#endif /* KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS > KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS */
#if (KMS_INDEX_MAX_NVM_STATIC_OBJECTS >= KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS)
#error "NVM static IDs & Dynamic IDs ranges are overlapping"
#endif /* KMS_INDEX_MAX_NVM_STATIC_OBJECTS >= KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS */

#if (KMS_NVM_SLOT_NUMBERS < (KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS-KMS_INDEX_MIN_NVM_STATIC_OBJECTS+1))
#error "Not enough slot declared in KMS_NVM_SLOT_NUMBERS to store all allowed NVM Static & dynamic IDs objects"
#endif /* KMS_NVM_SLOT_NUMBERS < (KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS-KMS_INDEX_MIN_NVM_STATIC_OBJECTS+1) */
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#endif /* KMS_NVM_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
#if (KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS > KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS)
#error "VM dynamic ID objects index min and max are not well ordered"
#endif /* KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS > KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS */
#if defined(KMS_NVM_ENABLED)
#if (KMS_INDEX_MAX_NVM_STATIC_OBJECTS >= KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS)
#error "NVM static IDs & VM Dynamic IDs ranges are overlapping"
#endif /* KMS_INDEX_MAX_NVM_STATIC_OBJECTS >= KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS */
#else /* KMS_NVM_DYNAMIC_ENABLED */
#if (KMS_INDEX_MAX_EMBEDDED_OBJECTS >= KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS)
#error "Embedded IDs & VM Dynamic IDs ranges are overlapping"
#endif /* KMS_INDEX_MAX_EMBEDDED_OBJECTS >= KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS */
#endif /* KMS_NVM_DYNAMIC_ENABLED */

#endif /* KMS_VM_DYNAMIC_ENABLED */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_PLATF Platform Objects
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @addtogroup KMS_PLATF_Private_Variables Private Variables
  * @{
  */
#ifdef KMS_NVM_ENABLED
/**
  * @brief  NVM initialization status
  */
static uint32_t kms_platf_nvm_initialisation_done = 0UL;

/**
  * @brief  NVM static objects access variables
  * @note   This "cache" table is used to speed up access to NVM static objects
  */
static kms_obj_keyhead_t *KMS_PlatfObjects_NvmStaticList[KMS_INDEX_MAX_NVM_STATIC_OBJECTS -
                                                         KMS_INDEX_MIN_NVM_STATIC_OBJECTS + 1];
#ifdef KMS_NVM_DYNAMIC_ENABLED
/**
  * @brief  NVM dynamic objects access variables
  * @note   This "cache" table is used to speed up access to NVM dynamic objects
  */
static kms_obj_keyhead_t *KMS_PlatfObjects_NvmDynamicList[KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS -
                                                          KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS + 1];
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#endif /* KMS_NVM_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
/**
  * @brief  VM initialization status
  */
static uint32_t kms_platf_vm_initialisation_done = 0UL;

/**
  * @brief  VM dynamic objects access variables
  * @note   This "cache" table is used to speed up access to VM dynamic objects
  */
static kms_obj_keyhead_t *KMS_PlatfObjects_VmDynamicList[KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS -
                                                         KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS + 1];
#endif /* KMS_VM_DYNAMIC_ENABLED */
/**
  * @}
  */
/* Private function prototypes -----------------------------------------------*/
#ifdef KMS_NVM_ENABLED
static void KMS_PlatfObjects_NvmStaticObjectList(void);
#endif  /* KMS_NVM_ENABLED */
#ifdef KMS_NVM_DYNAMIC_ENABLED
static void KMS_PlatfObjects_NvmDynamicObjectList(void);
#endif  /* KMS_NVM_DYNAMIC_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
static void KMS_PlatfObjects_VmDynamicObjectList(void);
#endif  /* KMS_VM_DYNAMIC_ENABLED */
/* Private function ----------------------------------------------------------*/
/** @addtogroup KMS_PLATF_Private_Functions Private Functions
  * @{
  */
#ifdef KMS_NVM_ENABLED
/**
  * @brief  Update @ref KMS_PlatfObjects_NvmStaticList with NVM contents
  * @retval None
  */
static void KMS_PlatfObjects_NvmStaticObjectList(void)
{
  nvms_error_t  nvms_rv;
  size_t nvms_data_size;
  kms_obj_keyhead_t *p_nvms_data;

  /* Load the KMS_PlatfObjects_NvmStaticList[], used to store buffer to NVM  */
  /* This should save processing time  */
  for (uint32_t i = KMS_INDEX_MIN_NVM_STATIC_OBJECTS; i < KMS_INDEX_MAX_NVM_STATIC_OBJECTS; i++)
  {
    /* Read values from NVM */
    nvms_rv = NVMS_GET_DATA(i - KMS_INDEX_MIN_NVM_STATIC_OBJECTS, &nvms_data_size, (uint8_t **)(uint32_t)&p_nvms_data);

    if ((nvms_data_size != 0UL) && (nvms_rv == NVMS_NOERROR))
    {
      KMS_PlatfObjects_NvmStaticList[i - KMS_INDEX_MIN_NVM_STATIC_OBJECTS] = p_nvms_data;
    }
    else
    {
      KMS_PlatfObjects_NvmStaticList[i - KMS_INDEX_MIN_NVM_STATIC_OBJECTS] = NULL;
    }
  }
}
#endif  /* KMS_NVM_ENABLED */

#ifdef KMS_NVM_DYNAMIC_ENABLED
/**
  * @brief  Update @ref KMS_PlatfObjects_NvmDynamicList with NVM contents
  * @retval None
  */
static void KMS_PlatfObjects_NvmDynamicObjectList(void)
{
  nvms_error_t  nvms_rv;
  size_t nvms_data_size;
  kms_obj_keyhead_t *p_nvms_data;

  /* Load the KMS_PlatfObjects_NvmDynamicList[], used to store buffer to NVM  */
  /* This should save processing time  */
  for (uint32_t i = KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS; i <= KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS; i++)
  {
    /* Read values from NVM */
    nvms_rv = NVMS_GET_DATA(i - KMS_INDEX_MIN_NVM_STATIC_OBJECTS, &nvms_data_size, (uint8_t **)(uint32_t)&p_nvms_data);

    if ((nvms_data_size != 0UL) && (nvms_rv == NVMS_NOERROR))
    {
      KMS_PlatfObjects_NvmDynamicList[i - KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS] = p_nvms_data;
    }
    else
    {
      KMS_PlatfObjects_NvmDynamicList[i - KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS] = NULL;
    }

  }
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
/**
  * @brief  Update @ref KMS_PlatfObjects_VmDynamicList with VM contents
  * @retval None
  */
static void KMS_PlatfObjects_VmDynamicObjectList(void)
{
  vms_error_t  vms_rv;
  size_t vms_data_size;
  kms_obj_keyhead_t *p_vms_data;

  /* Load the KMS_PlatfObjects_VmDynamicList[], used to store buffer to VM  */
  /* This should save processing time  */
  for (uint32_t i = KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS; i <= KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS; i++)
  {
    /* Read values from VM */
    vms_rv = VMS_GET_DATA(i - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS, &vms_data_size, (uint8_t **)(uint32_t)&p_vms_data);

    if ((vms_data_size != 0UL) && (vms_rv == VMS_NOERROR))
    {
      KMS_PlatfObjects_VmDynamicList[i - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS] = p_vms_data;
    }
    else
    {
      KMS_PlatfObjects_VmDynamicList[i - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS] = NULL;
    }

  }
}
#endif  /* KMS_VM_DYNAMIC_ENABLED */
/**
  * @}
  */
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_PLATF_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  Returns range of embedded objects
  * @param  pMin Embedded objects min ID
  * @param  pMax Embedded objects max ID
  * @retval None
  */
void KMS_PlatfObjects_EmbeddedRange(uint32_t *pMin, uint32_t *pMax)
{
  *pMin = KMS_INDEX_MIN_EMBEDDED_OBJECTS;
  *pMax = KMS_INDEX_MAX_EMBEDDED_OBJECTS;
}

/**
  * @brief  Returns embedded object corresponding to given key handle
  * @param  hKey key handle
  * @retval Corresponding object
  */
kms_obj_keyhead_t *KMS_PlatfObjects_EmbeddedObject(uint32_t hKey)
{
  return (kms_obj_keyhead_t *)(uint32_t)KMS_PlatfObjects_EmbeddedList[hKey - KMS_INDEX_MIN_EMBEDDED_OBJECTS];
}

#ifdef KMS_NVM_ENABLED
/**
  * @brief  Returns range of NVM static objects
  * @param  pMin NVM static objects min ID
  * @param  pMax NVM static objects max ID
  * @retval None
  */
void KMS_PlatfObjects_NvmStaticRange(uint32_t *pMin, uint32_t *pMax)
{
  *pMin = KMS_INDEX_MIN_NVM_STATIC_OBJECTS;
  *pMax = KMS_INDEX_MAX_NVM_STATIC_OBJECTS;
}
#endif  /* KMS_NVM_ENABLED */

#ifdef KMS_NVM_ENABLED
/**
  * @brief  Returns NVM static object corresponding to given key handle
  * @param  hKey key handle
  * @retval Corresponding object
  */
kms_obj_keyhead_t *KMS_PlatfObjects_NvmStaticObject(uint32_t hKey)
{
  return KMS_PlatfObjects_NvmStaticList[hKey - KMS_INDEX_MIN_NVM_STATIC_OBJECTS];
}
#endif  /* KMS_NVM_ENABLED */

#ifdef KMS_NVM_DYNAMIC_ENABLED
/**
  * @brief  Returns range of NVM dynamic objects
  * @param  pMin NVM dynamic objects min ID
  * @param  pMax NVM dynamic objects max ID
  * @retval None
  */
void KMS_PlatfObjects_NvmDynamicRange(uint32_t *pMin, uint32_t *pMax)
{
  *pMin = KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS;
  *pMax = KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS;
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED */

#ifdef KMS_NVM_DYNAMIC_ENABLED
/**
  * @brief  Returns NVM dynamic object corresponding to given key handle
  * @param  hKey key handle
  * @retval Corresponding object
  */
kms_obj_keyhead_t *KMS_PlatfObjects_NvmDynamicObject(uint32_t hKey)
{
  return KMS_PlatfObjects_NvmDynamicList[hKey - KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS];
}
#endif  /* KMS_NVM_DYNAMIC_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
/**
  * @brief  Returns range of VM dynamic objects
  * @param  pMin VM dynamic objects min ID
  * @param  pMax VM dynamic objects max ID
  * @retval None
  */
void KMS_PlatfObjects_VmDynamicRange(uint32_t *pMin, uint32_t *pMax)
{
  *pMin = KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS;
  *pMax = KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS;
}
#endif  /* KMS_VM_DYNAMIC_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
/**
  * @brief  Returns VM dynamic object corresponding to given key handle
  * @param  hKey key handle
  * @retval Corresponding object
  */
kms_obj_keyhead_t *KMS_PlatfObjects_VmDynamicObject(uint32_t hKey)
{
  return KMS_PlatfObjects_VmDynamicList[hKey - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS];
}
#endif  /* KMS_VM_DYNAMIC_ENABLED */

#if defined(KMS_NVM_DYNAMIC_ENABLED) || defined(KMS_VM_DYNAMIC_ENABLED)
/**
  * @brief  Search an available NVM / VM dynamic ID to store blob object
  * @param  pBlob  Blob object to store
  * @param  pObjId NVM / VM dynamic ID
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_DEVICE_MEMORY
  *         @ref KMS_PlatfObjects_NvmStoreObject returned values
  *         @ref KMS_PlatfObjects_VmStoreObject returned values
  */
CK_RV KMS_PlatfObjects_AllocateAndStore(kms_obj_keyhead_no_blob_t *pBlob, CK_OBJECT_HANDLE_PTR pObjId)
{
  CK_OBJECT_HANDLE Index;
  CK_RV e_ret_status;

  if ((pObjId == NULL_PTR) || (pBlob == NULL_PTR))
  {
    e_ret_status = CKR_ARGUMENTS_BAD;
  }
  else
  {
    *pObjId = KMS_HANDLE_KEY_NOT_KNOWN;
#ifdef KMS_NVM_DYNAMIC_ENABLED
    /* Find a Free place in nvm dynamic table */
    for (Index = 0; Index <= (KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS - KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS); Index++)
    {
      if (KMS_PlatfObjects_NvmDynamicList[Index] == NULL)
      {
        *pObjId = Index + KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS;
        break;
      }
    }
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
    /* Find a Free place in vm dynamic table */
    for (Index = 0; Index <= (KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS); Index++)
    {
      if (KMS_PlatfObjects_VmDynamicList[Index] == NULL)
      {
        *pObjId = Index + KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS;
        break;
      }
    }
#endif /* KMS_VM_DYNAMIC_ENABLED */
    if (*pObjId == KMS_HANDLE_KEY_NOT_KNOWN)
    {
      /* No place found in Dynamic List */
      e_ret_status = CKR_DEVICE_MEMORY;
    }
    else
    {
      /* Update object ID */
      pBlob->object_id = *pObjId;
#ifdef KMS_NVM_DYNAMIC_ENABLED
      /* Store in NVM storage */
      e_ret_status = KMS_PlatfObjects_NvmStoreObject(*pObjId,
                                                     (uint8_t *)pBlob,
                                                     pBlob->blobs_size + sizeof(kms_obj_keyhead_no_blob_t));
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#ifdef KMS_VM_DYNAMIC_ENABLED
      /* Store in VM storage */
      e_ret_status = KMS_PlatfObjects_VmStoreObject(*pObjId,
                                                    (uint8_t *)pBlob,
                                                    pBlob->blobs_size + sizeof(kms_obj_keyhead_no_blob_t));
#endif /* KMS_VM_DYNAMIC_ENABLED */
      /* A Garbage collection generate a WARNING ==> Not an error */
      if (e_ret_status != CKR_OK)
      {
        *pObjId = KMS_HANDLE_KEY_NOT_KNOWN;
      }
    }
  }
  return e_ret_status;
}
#endif /* KMS_NVM_DYNAMIC_ENABLED || KMS_NVM_DYNAMIC_ENABLED */

#ifdef KMS_EXT_TOKEN_ENABLED
/**
  * @brief  Returns range of External token static objects
  * @param  pMin External token static objects min ID
  * @param  pMax External token static objects max ID
  * @retval None
  */
void KMS_PlatfObjects_ExtTokenStaticRange(uint32_t *pMin, uint32_t *pMax)
{
  *pMin = KMS_INDEX_MIN_EXT_TOKEN_STATIC_OBJECTS;
  *pMax = KMS_INDEX_MAX_EXT_TOKEN_STATIC_OBJECTS;
}
/**
  * @brief  Returns range of External token dynamic objects
  * @param  pMin External token dynamic objects min ID
  * @param  pMax External token dynamic objects max ID
  * @retval None
  */
void KMS_PlatfObjects_ExtTokenDynamicRange(uint32_t *pMin, uint32_t *pMax)
{
  *pMin = KMS_INDEX_MIN_EXT_TOKEN_DYNAMIC_OBJECTS;
  *pMax = KMS_INDEX_MAX_EXT_TOKEN_DYNAMIC_OBJECTS;
}
#endif /* KMS_EXT_TOKEN_ENABLED */


/**
  * @brief  Initialize platform objects
  * @note   Initialize NVM / VM storage and fill "cache" buffers
  * @retval None
  */
void KMS_PlatfObjects_Init(void)
{
#ifdef KMS_NVM_ENABLED
  /* The NVMS_Init should be done only once */
  if (kms_platf_nvm_initialisation_done == 0UL)
  {
    /* Initialize the NVMS */
    (void)NVMS_Init();
    kms_platf_nvm_initialisation_done = 1UL;
  }

  KMS_PlatfObjects_NvmStaticObjectList();
#ifdef KMS_NVM_DYNAMIC_ENABLED
  KMS_PlatfObjects_NvmDynamicObjectList();
#endif /* KMS_NVM_DYNAMIC_ENABLED */

#endif /* KMS_NVM_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
  /* The VMS_Init should be done only once */
  if (kms_platf_vm_initialisation_done == 0UL)
  {
    /* Initialize the VMS */
    (void)VMS_Init();
    kms_platf_vm_initialisation_done = 1UL;
  }

  KMS_PlatfObjects_VmDynamicObjectList();
#endif /* KMS_VM_DYNAMIC_ENABLED */
}

/**
  * @brief  De-Initialize platform objects
  * @retval None
  */
void KMS_PlatfObjects_Finalize(void)
{
#ifdef KMS_NVM_ENABLED
  /* Finalize the NVMS */
  NVMS_Deinit();

  /* We must re-allow the call to NVMS_Init() */
  kms_platf_nvm_initialisation_done = 0UL;
#endif /* KMS_NVM_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
  /* Finalize the VMS */
  VMS_Deinit();

  /* We must re-allow the call to VMS_Init() */
  kms_platf_vm_initialisation_done = 0UL;
#endif /* KMS_VM_DYNAMIC_ENABLED */
}

#ifdef KMS_NVM_ENABLED
/**
  * @brief  Store object in NVM storage
  * @note   Either static or dynamic objects
  * @param  ObjectId Object ID
  * @param  pObjectToAdd Object to add
  * @param  ObjectSize Object size
  * @retval CKR_OK if storage is successful
  *         CKR_DEVICE_MEMORY otherwise
  */
CK_RV KMS_PlatfObjects_NvmStoreObject(uint32_t ObjectId, uint8_t *pObjectToAdd,  uint32_t ObjectSize)
{
  nvms_error_t  rv;
  CK_RV e_ret_status;

  /* It's a NVM STATIC object */
  if ((ObjectId >= KMS_INDEX_MIN_NVM_STATIC_OBJECTS) && (ObjectId <= KMS_INDEX_MAX_NVM_STATIC_OBJECTS))
  {
    rv = NVMS_WRITE_DATA(ObjectId - KMS_INDEX_MIN_NVM_STATIC_OBJECTS, ObjectSize, (const uint8_t *)pObjectToAdd);
  }
  else
  {
#ifdef KMS_NVM_DYNAMIC_ENABLED
    /* It's a NVM DYNAMIC object */
    if ((ObjectId >= KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS) && (ObjectId <= KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS))
    {
      rv = NVMS_WRITE_DATA(ObjectId - KMS_INDEX_MIN_NVM_STATIC_OBJECTS, ObjectSize, (const uint8_t *)pObjectToAdd);
    }
    else
    {
      rv = NVMS_SLOT_INVALID;
    }
#else /* KMS_NVM_DYNAMIC_ENABLED */
    rv = NVMS_SLOT_INVALID;
#endif /* KMS_NVM_DYNAMIC_ENABLED */
  }
  /* A Garbage collection generate a WARNING ==> Not an error */
  if ((rv == NVMS_NOERROR) || (rv == NVMS_WARNING))
  {
    e_ret_status = CKR_OK;
  }
  else
  {
    e_ret_status = CKR_DEVICE_MEMORY;
  }

  /* Refresh NVM lists */
  KMS_PlatfObjects_NvmStaticObjectList();
#ifdef KMS_NVM_DYNAMIC_ENABLED
  KMS_PlatfObjects_NvmDynamicObjectList();
#endif /* KMS_NVM_DYNAMIC_ENABLED */

  return e_ret_status;
}

#ifdef KMS_NVM_DYNAMIC_ENABLED
/**
  * @brief  Remove object from NVM storage
  * @note   Only dynamic objects
  * @param  ObjectId Object ID
  * @retval CKR_OK if removal is successful
  *         CKR_DEVICE_MEMORY otherwise
  */
CK_RV KMS_PlatfObjects_NvmRemoveObject(uint32_t ObjectId)
{
  nvms_error_t rv = NVMS_DATA_NOT_FOUND;
  CK_RV e_ret_status;

  /* Check that the ObjectID is in dynamic range */
  if ((ObjectId >= KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS) && (ObjectId <= KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS))
  {
    rv = NVMS_EraseData(ObjectId - KMS_INDEX_MIN_NVM_STATIC_OBJECTS);
  }
  /* A Garbage collection generate a WARNING ==> Not an error */
  if ((rv == NVMS_NOERROR) || (rv == NVMS_WARNING))
  {
    e_ret_status = CKR_OK;
  }
  else
  {
    e_ret_status = CKR_DEVICE_MEMORY;
  }

  /* Refresh NVM lists */
  KMS_PlatfObjects_NvmStaticObjectList();
#ifdef KMS_NVM_DYNAMIC_ENABLED
  KMS_PlatfObjects_NvmDynamicObjectList();
#endif /* KMS_NVM_DYNAMIC_ENABLED */

  return e_ret_status;
}
#endif /* KMS_NVM_DYNAMIC_ENABLED */

#if defined(KMS_IMPORT_BLOB)
/**
  * @brief  Returns Blob import verification key handle
  * @retval Key handle
  */
CK_ULONG KMS_PlatfObjects_GetBlobVerifyKey(void)
{
  return (CK_ULONG)KMS_INDEX_BLOBIMPORT_VERIFY;
}
#endif /* KMS_IMPORT_BLOB */

#if defined(KMS_IMPORT_BLOB)
/**
  * @brief  Returns Blob import decryption key handle
  * @retval Key handle
  */
CK_ULONG KMS_PlatfObjects_GetBlobDecryptKey(void)
{
  return (CK_ULONG)KMS_INDEX_BLOBIMPORT_DECRYPT;
}
#endif /* KMS_IMPORT_BLOB */
#endif /* KMS_NVM_ENABLED */

#ifdef KMS_VM_DYNAMIC_ENABLED
/**
  * @brief  Store object in VM storage
  * @note   Either static or dynamic objects
  * @param  ObjectId Object ID
  * @param  pObjectToAdd Object to add
  * @param  ObjectSize Object size
  * @retval CKR_OK if storage is successful
  *         CKR_DEVICE_MEMORY otherwise
  */
CK_RV KMS_PlatfObjects_VmStoreObject(uint32_t ObjectId, uint8_t *pObjectToAdd,  uint32_t ObjectSize)
{
  vms_error_t  rv;
  CK_RV e_ret_status;

  /* It's a VM DYNAMIC object */
  if ((ObjectId >= KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS) && (ObjectId <= KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS))
  {
    rv = VMS_WRITE_DATA(ObjectId - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS, ObjectSize, (const uint8_t *)pObjectToAdd);
  }
  else
  {
    rv = VMS_SLOT_INVALID;
  }

  /* A WARNING was generated ==> Not an error */
  if ((rv == VMS_NOERROR) || (rv == VMS_WARNING))
  {
    e_ret_status = CKR_OK;
  }
  else
  {
    e_ret_status = CKR_DEVICE_MEMORY;
  }

  KMS_PlatfObjects_VmDynamicObjectList();

  return e_ret_status;
}

/**
  * @brief  Remove object from VM storage
  * @note   Only dynamic objects
  * @param  ObjectId Object ID
  * @retval CKR_OK if removal is successful
  *         CKR_DEVICE_MEMORY otherwise
  */
CK_RV KMS_PlatfObjects_VmRemoveObject(uint32_t ObjectId)
{
  vms_error_t rv = VMS_DATA_NOT_FOUND;
  CK_RV e_ret_status;

  /* Check that the ObjectID is in dynamic range */
  if ((ObjectId >= KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS) && (ObjectId <= KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS))
  {
    rv = VMS_EraseData(ObjectId - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS);
  }
  /* A WARNING was generated ==> Not an error */
  if ((rv == VMS_NOERROR) || (rv == VMS_WARNING))
  {
    e_ret_status = CKR_OK;
  }
  else
  {
    e_ret_status = CKR_DEVICE_MEMORY;
  }

  KMS_PlatfObjects_VmDynamicObjectList();

  return e_ret_status;
}
#endif /* KMS_VM_DYNAMIC_ENABLED */

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
