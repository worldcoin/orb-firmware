/**
  ******************************************************************************
  * @file    kms_dyn_obj.c
  * @author  MCD Application Team
  * @brief   This file contains implementation for Key Management Services (KMS)
  *          module dynamic object management functionalities.
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
#include "kms_dyn_obj.h"                  /* KMS object services */

#include "kms_objects.h"                  /* KMS object management services */
#include "kms_nvm_storage.h"              /* KMS NVM object storage services */
#include "kms_vm_storage.h"               /* KMS VM storage services */
#include "kms_platf_objects.h"            /* KMS platf objects services */
#include "kms_mem.h"                      /* KMS memory utilities */

#include "kms_platf_objects_config.h"     /* KMS embedded objects definitions */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_DYNOBJ Objects services
  * @{
  */

/* Private types -------------------------------------------------------------*/
/** @addtogroup KMS_DYNOBJ_Private_Types Private Types
  * @{
  */
#if defined(KMS_SEARCH)
/**
  * @brief  Object search context structure
  */
typedef struct
{
#if defined(KMS_NVM_ENABLED) && defined(KMS_NVM_DYNAMIC_ENABLED) && !defined(KMS_VM_DYNAMIC_ENABLED)
  /* Embedded keys + Both NVM flavor keys */
  CK_OBJECT_HANDLE searchHandles[KMS_INDEX_MAX_EMBEDDED_OBJECTS - KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1 \
                                 + KMS_INDEX_MAX_NVM_STATIC_OBJECTS - KMS_INDEX_MIN_NVM_STATIC_OBJECTS + 1 \
                                 + KMS_INDEX_MAX_NVM_DYNAMIC_OBJECTS - KMS_INDEX_MIN_NVM_DYNAMIC_OBJECTS + 1];
  /*!< Found object handles list  */
#elif defined(KMS_NVM_ENABLED) && !defined(KMS_NVM_DYNAMIC_ENABLED) && defined(KMS_VM_DYNAMIC_ENABLED)
  /* Embedded keys + NVM static ID keys + VM dynamic keys */
  CK_OBJECT_HANDLE searchHandles[KMS_INDEX_MAX_EMBEDDED_OBJECTS - KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1 \
                                 + KMS_INDEX_MAX_NVM_STATIC_OBJECTS - KMS_INDEX_MIN_NVM_STATIC_OBJECTS + 1 \
                                 + KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS + 1];
#elif defined(KMS_NVM_ENABLED) && !defined(KMS_NVM_DYNAMIC_ENABLED) && !defined(KMS_VM_DYNAMIC_ENABLED)
  /* Embedded keys + NVM static ID keys */
  CK_OBJECT_HANDLE searchHandles[KMS_INDEX_MAX_EMBEDDED_OBJECTS - KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1 \
                                 + KMS_INDEX_MAX_NVM_STATIC_OBJECTS - KMS_INDEX_MIN_NVM_STATIC_OBJECTS + 1];
  /*!< Found object handles list  */
#elif !defined(KMS_NVM_ENABLED) && !defined(KMS_NVM_DYNAMIC_ENABLED) && defined(KMS_VM_DYNAMIC_ENABLED)
  /* Embedded keys + VM dynamic keys */
  CK_OBJECT_HANDLE searchHandles[KMS_INDEX_MAX_EMBEDDED_OBJECTS - KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1 \
                                 + KMS_INDEX_MAX_VM_DYNAMIC_OBJECTS - KMS_INDEX_MIN_VM_DYNAMIC_OBJECTS + 1];
  /*!< Found object handles list  */
#elif !defined(KMS_NVM_ENABLED) && !defined(KMS_NVM_DYNAMIC_ENABLED)&& !defined(KMS_VM_DYNAMIC_ENABLED)
  /* Embedded keys only */
  CK_OBJECT_HANDLE searchHandles[KMS_INDEX_MAX_EMBEDDED_OBJECTS - KMS_INDEX_MIN_EMBEDDED_OBJECTS + 1];
  /*!< Found object handles list  */
#else /* KMS_NVM_ENABLED && KMS_NVM_DYNAMIC_ENABLED && KMS_VM_DYNAMIC_ENABLED */
#error "Unsupported object search context structure"
#endif /* KMS_NVM_ENABLED && KMS_NVM_DYNAMIC_ENABLED && KMS_VM_DYNAMIC_ENABLED */
  uint32_t searchIndex;          /*!< Index in found list for reading procedure */
} kms_find_ctx_t;
#endif /* KMS_SEARCH */
/**
  * @}
  */

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_DYNOBJ_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  This function is called upon @ref C_CreateObject call
  * @note   Refer to @ref C_CreateObject function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  pTemplate object template
  * @param  ulCount attributes count in the template
  * @param  phObject object to be created
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_SESSION_HANDLE_INVALID
  *         CKR_TEMPLATE_INCOMPLETE
  *         @ref KMS_Objects_CreateNStoreBlobFromTemplates returned values
  */
CK_RV  KMS_CreateObject(CK_SESSION_HANDLE hSession,
                        CK_ATTRIBUTE_PTR pTemplate,
                        CK_ULONG ulCount,
                        CK_OBJECT_HANDLE_PTR phObject)
{
#if defined(KMS_OBJECTS)
  CK_ATTRIBUTE_PTR  p_attribut_value = NULL_PTR;
  CK_RV e_ret_status;

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

  /* Control CLASS ATTRIBUTE */
  if (CKR_OK != KMS_FindAttributeInTemplate(pTemplate, ulCount, CKA_CLASS, &p_attribut_value))
  {
    /* Class attribute not found, object not valid */
    return CKR_TEMPLATE_INCOMPLETE;
  }

  /* The provided creation template should at least include one of the following:
   * CKA_CERTIFICATE_TYPE, CKA_HW_FEATURE_TYPE or CKA_KEY_TYPE
   */
  if (KMS_FindAttributeInTemplate(pTemplate, ulCount, CKA_CERTIFICATE_TYPE, &p_attribut_value) != CKR_OK)
  {
    if (KMS_FindAttributeInTemplate(pTemplate, ulCount, CKA_HW_FEATURE_TYPE, &p_attribut_value) != CKR_OK)
    {
      if (KMS_FindAttributeInTemplate(pTemplate, ulCount, CKA_KEY_TYPE, &p_attribut_value) != CKR_OK)
      {
        /* type attribute not found, object not valid */
        return CKR_TEMPLATE_INCOMPLETE;
      }
    }
  }

  /* Allocate blob object to fill it with template data */
  e_ret_status = KMS_Objects_CreateNStoreBlobFromTemplates(hSession, pTemplate, ulCount, NULL_PTR, 0, phObject);

  return e_ret_status;
#else /* KMS_OBJECTS */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_OBJECTS */
}

/**
  * @brief  This function is called upon @ref C_DestroyObject call
  * @note   Refer to @ref C_DestroyObject function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  hObject object handle
  * @retval CKR_OK
  *         CKR_ACTION_PROHIBITED
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_OBJECT_HANDLE_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  *         @ref KMS_Objects_SearchAttributes returned values
  *         @ref KMS_PlatfObjects_NvmRemoveObject returned values
  *         @ref KMS_PlatfObjects_VmRemoveObject returned values
  */
CK_RV  KMS_DestroyObject(CK_SESSION_HANDLE hSession,
                         CK_OBJECT_HANDLE hObject)
{
#if defined(KMS_OBJECTS)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  kms_obj_keyhead_t *pkms_object;
  kms_attr_t  *pAttribute;
  kms_obj_range_t  ObjectRange;

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

  /* Verify that the object is removable, embedded objects are not */
  ObjectRange = KMS_Objects_GetRange(hObject);
  if (ObjectRange == KMS_OBJECT_RANGE_EMBEDDED)
  {
    return (CKR_ACTION_PROHIBITED);
  }

  /* Verify that the object is removable, reading the attributes */

  /* Read the key value from the Key Handle                 */
  /* Key Handle is the index to one of static or nvm        */
  pkms_object = KMS_Objects_GetPointer(hObject);

  /* Check that hKey is valid */
  if (pkms_object != NULL_PTR)
  {
    /* Check the CKA_DESTROYABLE attribute = CK_TRUE      */
    e_ret_status = KMS_Objects_SearchAttributes(CKA_DESTROYABLE, pkms_object, &pAttribute);

    if (e_ret_status == CKR_OK)
    {
      if (*pAttribute->data != CK_TRUE)
      {
        /* Object destruction not permitted for the selected object  */
        return (CKR_ACTION_PROHIBITED);
      }
    }

    /* Object is removable */
#ifdef KMS_VM_DYNAMIC_ENABLED
    e_ret_status = KMS_PlatfObjects_VmRemoveObject(hObject);
#else /* KMS_VM_DYNAMIC_ENABLED */
    e_ret_status = KMS_PlatfObjects_NvmRemoveObject(hObject);
#endif /* KMS_VM_DYNAMIC_ENABLED */
  }
  else
  {
    e_ret_status = CKR_OBJECT_HANDLE_INVALID;
  }

  return e_ret_status;
#else /* KMS_OBJECTS */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_OBJECTS */
}

/**
  * @brief  This function is called upon @ref C_GetAttributeValue call
  * @note   Refer to @ref C_GetAttributeValue function description
  *         for more details on the APIs, parameters and possible returned values
  * @param  hSession session handle
  * @param  hObject object handle
  * @param  pTemplate template containing attributes to retrieve
  * @param  ulCount number of attributes in the template
  * @retval CKR_OK
  *         CKR_ATTRIBUTE_SENSITIVE
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_FAILED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_OBJECT_HANDLE_INVALID
  *         CKR_SESSION_HANDLE_INVALID
  *         @ref KMS_Objects_SearchAttributes returned values
  */
CK_RV KMS_GetAttributeValue(CK_SESSION_HANDLE hSession,  CK_OBJECT_HANDLE  hObject,
                            CK_ATTRIBUTE_PTR  pTemplate, CK_ULONG          ulCount)
{
#if defined(KMS_ATTRIBUTES)
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  kms_obj_keyhead_t *pkms_object;

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

  /* GetAttribute() is only authorised for objects with attribute EXTRACTABLE = TRUE or without EXTRACTABLE attribute */
  /* Verify that the object is extractable, reading the attributes */

  /* Read the key value from the Key Handle                 */
  /* Key Handle is the index to one of static or nvm        */
  pkms_object = KMS_Objects_GetPointer(hObject);

  /* Check that hObject is valid */
  if (pkms_object != NULL_PTR)
  {
    kms_attr_t  *pAttribute;

    /* Check the CKA_EXTRACTABLE attribute = CK_TRUE      */
    e_ret_status = KMS_Objects_SearchAttributes(CKA_EXTRACTABLE, pkms_object, &pAttribute);

    /* If attribute not found or object not extractable */
    if ((e_ret_status == CKR_OK) && (*pAttribute->data != CK_TRUE))
    {
      for (uint32_t i = 0; i < ulCount; i++)
      {
        pTemplate[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;
      }
      /* Object cannot be extracted  */
      return (CKR_ATTRIBUTE_SENSITIVE);
    }

    /* Double check to avoid fault attack: Check the CKA_EXTRACTABLE attribute = CK_TRUE */
    e_ret_status = KMS_Objects_SearchAttributes(CKA_EXTRACTABLE, pkms_object, &pAttribute);

    /* If attribute not found or object not extractable */
    if ((e_ret_status == CKR_OK) && (*pAttribute->data != CK_TRUE))
    {
      for (uint32_t i = 0; i < ulCount; i++)
      {
        pTemplate[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;
      }
      /* Object cannot be extracted  */
      return (CKR_ATTRIBUTE_SENSITIVE);
    }

    /* Object is extractable */
    /* Let's loop the Attributes passed in the Template, to extract the values of the matching Types */

    /*
    Description (extract from PKCS11 OASIS spec v2.40 :
    For each (type, pValue, ulValueLen) triple in the template, C_GetAttributeValue performs the following
    algorithm:
        1. If the specified attribute (i.e., the attribute specified by the type field) for the object cannot be
    revealed because the object is sensitive or unextractable, then the ulValueLen field in that triple is modified to
    hold the value CK_UNAVAILABLE_INFORMATION.
        2. Otherwise, if the specified value for the object is invalid (the object does not possess such an
    attribute), then the ulValueLen field in that triple is modified to hold the value
    CK_UNAVAILABLE_INFORMATION.
        3. Otherwise, if the pValue field has the value NULL_PTR, then the ulValueLen field is modified to hold
    the exact length of the specified attribute for the object.
        4. Otherwise, if the length specified in ulValueLen is large enough to hold the value of the specified
    attribute for the object, then that attribute is copied into the buffer located at pValue, and the
    ulValueLen field is modified to hold the exact length of the attribute.
        5. Otherwise, the ulValueLen field is modified to hold the value CK_UNAVAILABLE_INFORMATION.

    If case 1 applies to any of the requested attributes, then the call should return the value
    CKR_ATTRIBUTE_SENSITIVE. If case 2 applies to any of the requested attributes, then the call should
    return the value CKR_ATTRIBUTE_TYPE_INVALID. If case 5 applies to any of the requested attributes,
    then the call should return the value CKR_BUFFER_TOO_SMALL. As usual, if more than one of these
    error codes is applicable, Cryptoki may return any of them. Only if none of them applies to any of the
    requested attributes will CKR_OK be returned.
    */
    {
      kms_attr_t  *pfound_attribute;
      uint32_t index;
      CK_ATTRIBUTE_PTR ptemp;

      for (index = 0; index < ulCount; index++)
      {
        ptemp = &(pTemplate[index]);
        /* Search for the type of attribute from the Template */
        e_ret_status = KMS_Objects_SearchAttributes(ptemp->type, pkms_object, &pfound_attribute);

        if (e_ret_status == CKR_OK)
        {
          if (ptemp->pValue == NULL_PTR)
          {
            /* case 3 */
            ptemp->ulValueLen = pfound_attribute->size;
          }
          else if (ptemp->ulValueLen >= pfound_attribute->size)
          {
            /* case 4 */
            if ((ptemp->type == CKA_VALUE) || (ptemp->type == CKA_EC_POINT))
            {
              KMS_Objects_BlobU32_2_u8ptr(&(pfound_attribute->data[0]),
                                          pfound_attribute->size,
                                          (uint8_t *)(ptemp->pValue));
            }
            else
            {
              (void)memcpy((uint8_t *)ptemp->pValue, (uint8_t *)(pfound_attribute->data), pfound_attribute->size);
            }
            ptemp->ulValueLen = pfound_attribute->size;
          }
          else
          {
            /* case 4 - section 5.2 */
            ptemp->ulValueLen = CKR_BUFFER_TOO_SMALL;
          }
        }
        else
        {
          /* case 2 */
          ptemp->ulValueLen = CK_UNAVAILABLE_INFORMATION;
        }

        ptemp++;
      }
    }

  }
  else
  {
    e_ret_status = CKR_OBJECT_HANDLE_INVALID;
  }

  return e_ret_status;
#else /* KMS_ATTRIBUTES */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_ATTRIBUTES */
}

/**
  * @brief  This function is called upon @ref C_FindObjectsInit call
  * @note   Refer to @ref C_FindObjectsInit function description for more
  *         details on the APIs, parameters and possible returned values
  * @param  hSession   the session's handle
  * @param  pTemplate  attribute values to match
  * @param  ulCount    number of attribute in search template
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_DEVICE_MEMORY
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_SESSION_HANDLE_INVALID
  *         @ref KMS_FindObjectsFromTemplate returned values
  */
CK_RV KMS_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount)
{
#if defined(KMS_SEARCH)
  CK_ULONG tmp;
  CK_RV e_ret_status;
  kms_find_ctx_t *p_ctx;

  /* ========== Check active operation status ========== */

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

  /* Check input parameters */
  /* Note: pTemplate = NULL_PTR is ok only if ulCount = 0 */
  if (((pTemplate == NULL_PTR) && (ulCount > 0U)) || ((pTemplate != NULL_PTR) && (ulCount == 0U)))
  {
    return CKR_ARGUMENTS_BAD;
  }

  p_ctx = KMS_Alloc(hSession, sizeof(kms_find_ctx_t));
  if (p_ctx == NULL_PTR)
  {
    return CKR_DEVICE_MEMORY;
  }

  /* ========== Look for the objects  ========== */
  for (uint32_t i = 0; i < (sizeof(p_ctx->searchHandles) / sizeof(CK_OBJECT_HANDLE)); i++)
  {
    p_ctx->searchHandles[i] = KMS_HANDLE_KEY_NOT_KNOWN;
  }

  /* Note: if ulCount was 0, searchHandles will be filled with a list of all objects handles */
  e_ret_status = KMS_FindObjectsFromTemplate(hSession,
                                             &(p_ctx->searchHandles[0U]),
                                             sizeof(p_ctx->searchHandles) / sizeof(CK_OBJECT_HANDLE),
                                             &tmp,
                                             pTemplate,
                                             ulCount);

  if (e_ret_status == CKR_OK)
  {
    p_ctx->searchIndex = 0;
    KMS_GETSESSION(hSession).state = KMS_SESSION_SEARCH;
    KMS_GETSESSION(hSession).pCtx = p_ctx;
  }
  else
  {
    KMS_Free(hSession, p_ctx);
  }

  return e_ret_status;
#else /* KMS_SEARCH */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SEARCH */
}

/**
  * @brief  This function is called upon @ref C_FindObjects call
  * @note   Refer to @ref C_FindObjects function description for more
  *         details on the APIs, parameters and possible returned values
  * @param  hSession         session handle
  * @param  phObject         pointer that receives list of additional object handles
  * @param  ulMaxObjectCount maximum number of object to return
  * @param  pulObjectCount   number of object return
  * @retval CKR_OK
  *         CKR_ARGUMENTS_BAD
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV KMS_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
                      CK_ULONG ulMaxObjectCount,  CK_ULONG_PTR pulObjectCount)
{
#if defined(KMS_SEARCH)
  kms_find_ctx_t *p_ctx;
  uint32_t i;

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if there is a pending operation: i.e. FindObjectInit was called */
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_SEARCH)
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }

  /* Check input parameters */
  if ((phObject == NULL_PTR) || (pulObjectCount == NULL_PTR) || (ulMaxObjectCount <= 0UL))
  {
    return CKR_ARGUMENTS_BAD;
  }

  /* ========== Get active operation objects ========== */
  p_ctx = KMS_GETSESSION(hSession).pCtx;
  *pulObjectCount = 0;
  for (i = p_ctx->searchIndex; (i < (sizeof(p_ctx->searchHandles) / sizeof(CK_OBJECT_HANDLE)))
       && (*pulObjectCount < ulMaxObjectCount); i++)
  {
    if (p_ctx->searchHandles[i] != KMS_HANDLE_KEY_NOT_KNOWN)
    {
      phObject[*pulObjectCount] = p_ctx->searchHandles[i];
      *pulObjectCount += 1UL;
    }
  }
  /* Update searchIndex to filter out already transmitted values */
  p_ctx->searchIndex = i;

  return CKR_OK;
#else /* KMS_SEARCH */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SEARCH */
}

/**
  * @brief  This function is called upon @ref C_FindObjectsFinal call
  * @note   Refer to @ref C_FindObjectsFinal function description for more
  *         details on the APIs, parameters and possible returned values
  * @param  hSession   the session's handle
  * @retval CKR_OK
  *         CKR_CRYPTOKI_NOT_INITIALIZED
  *         CKR_FUNCTION_NOT_SUPPORTED
  *         CKR_SESSION_HANDLE_INVALID
  */
CK_RV KMS_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
#if defined(KMS_SEARCH)
  /* ========== Check active operation status ========== */

  if (!KMS_IS_INITIALIZED())
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }
  if (KMS_CheckSessionHdle(hSession) != CKR_OK)
  {
    return CKR_SESSION_HANDLE_INVALID;
  }
  /* Check if there is a pending operation: i.e. FindObjectInit was called*/
  if (KMS_GETSESSION(hSession).state != KMS_SESSION_SEARCH)
  {
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  }

  if (KMS_GETSESSION(hSession).pCtx != NULL_PTR)
  {
    KMS_Free(hSession, KMS_GETSESSION(hSession).pCtx);
    KMS_GETSESSION(hSession).pCtx = NULL_PTR;
  }

  KMS_SetStateIdle(hSession);

  return CKR_OK;
#else /* KMS_SEARCH */
  return CKR_FUNCTION_NOT_SUPPORTED;
#endif /* KMS_SEARCH */
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
