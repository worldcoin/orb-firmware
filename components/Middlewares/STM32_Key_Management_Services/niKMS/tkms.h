/**
  ******************************************************************************
  * @file    tkms.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for trusted Key Management Services
  *          (tKMS) module of the PKCS11 APIs when called from the secure enclave
  *          or without any enclave
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
#ifndef TKMS_H
#define TKMS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "kms.h"
#include "kms_platf_objects_interface.h"
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#include "kms_enc_dec.h"
#include "kms_digest.h"
#include "kms_dyn_obj.h"
#include "kms_init.h"
#include "kms_key_mgt.h"
#include "kms_sign_verify.h"
#include "kms_objects.h"
#endif /* KMS_NIKMS_ROUTER_BYPASS */
#include "kms_interface.h"

/** @addtogroup tKMS User interface to Key Management Services (tKMS)
  * @{
  */

/** @addtogroup KMS_TKMS_PKCS11 tKMS APIs (PKCS#11 Standard Compliant)
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/

/**
  * @brief  Redirection of PKCS11 C_Initialize to access KMS service through Secure engine
  * @note   C_Initialize initializes the Cryptoki library
  * @param  pInitArgs either has the value NULL_PTR or points to a CK_C_INITIALIZE_ARGS
  *         structure containing information on how the library should deal with
  *         multi-threaded access.  If an application will not be accessing Cryptoki
  *         through multiple threads simultaneously, it can generally supply the
  *         value NULL_PTR to C_Initialize
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CANT_LOCK
  * @retval CKR_CRYPTOKI_ALREADY_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_NEED_TO_CREATE_THREADS
  * @retval CKR_OK
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Initialize( pInitArgs )\
  KMS_Initialize( pInitArgs )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Initialize( pInitArgs )\
  KMS_IF_Initialize( pInitArgs )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_Finalize to access KMS service through Secure engine
  * @note   C_Finalize is called to indicate that an application is finished with
  *         the Cryptoki library.  It should be the last Cryptoki call made by an
  *         application
  * @param  pReserved parameter is reserved for future versions
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Finalize( pReserved )\
  KMS_Finalize( pReserved )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Finalize( pReserved )\
  KMS_IF_Finalize( pReserved )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GetInfo to access KMS service through Secure engine
  * @note   C_GetInfo returns general information about Cryptoki
  * @param  pInfo points to the location that receives the information
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GetInfo( pInfo )\
  KMS_GetInfo( pInfo )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GetInfo( pInfo )\
  KMS_IF_GetInfo( pInfo )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GetFunctionList to access KMS service through Secure engine
  * @note   C_GetFunctionList obtains a pointer to the Cryptoki library list
  *         of function pointers
  * @param  ppFunctionList points to a value which will receive a pointer to the
  *         library CK_FUNCTION_LIST structure, which in turn contains function
  *         pointers for all the Cryptoki API routines in the library
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#define C_GetFunctionList( ppFunctionList )\
  KMS_IF_GetFunctionList( ppFunctionList )

/**
  * @brief  Redirection of PKCS11 C_GetSlotList to access KMS service through Secure engine
  * @note   C_GetSlotList is used to obtain a list of slots in the system
  * @param  tokenPresent indicates whether the list obtained includes only those
  *         slots with a token present (CK_TRUE), or all slots (CK_FALSE)
  * @param  pSlotList points to the location that receives the slot list
  * @param  pulCount points to the location that receives the number of slots
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GetSlotList( tokenPresent, pSlotList, pulCount )\
  KMS_GetSlotList( tokenPresent, pSlotList, pulCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GetSlotList( tokenPresent, pSlotList, pulCount )\
  KMS_IF_GetSlotList( tokenPresent, pSlotList, pulCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GetSlotInfo to access KMS service through Secure engine
  * @note   C_GetSlotInfo obtains information about a particular slot in the system
  * @param  slotID is the ID of the slot
  * @param  pInfo points to the location that receives the slot information
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_SLOT_ID_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GetSlotInfo( slotID, pInfo )\
  KMS_GetSlotInfo( slotID, pInfo )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GetSlotInfo( slotID, pInfo )\
  KMS_IF_GetSlotInfo( slotID, pInfo )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GetTokenInfo to access KMS service through Secure engine
  * @note   C_GetTokenInfo obtains information about a particular token in the system
  * @param  slotID is the ID of the token slot
  * @param  pInfo points to the location that receives the token information
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_SLOT_ID_INVALID
  * @retval CKR_TOKEN_NOT_PRESENT
  * @retval CKR_TOKEN_NOT_RECOGNIZED
  * @retval CKR_ARGUMENTS_BAD
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GetTokenInfo( slotID, pInfo )\
  KMS_GetTokenInfo( slotID, pInfo )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GetTokenInfo( slotID, pInfo )\
  KMS_IF_GetTokenInfo( slotID, pInfo )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GetMechanismInfo to access KMS service through Secure engine
  * @note   C_GetMechanismInfo obtains information about a particular mechanism possibly supported by a token
  * @param  slotID is the ID of the token slot
  * @param  type is the type of mechanism
  * @param  pInfo points to the location that receives the mechanism information
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_OK
  * @retval CKR_SLOT_ID_INVALID
  * @retval CKR_TOKEN_NOT_PRESENT
  * @retval CKR_TOKEN_NOT_RECOGNIZED
  * @retval CKR_ARGUMENTS_BAD
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GetMechanismInfo( slotID, type, pInfo )\
  KMS_GetMechanismInfo( slotID, type, pInfo )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GetMechanismInfo( slotID, type, pInfo )\
  KMS_IF_GetMechanismInfo( slotID, type, pInfo )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_OpenSession to access KMS service through Secure engine
  * @note   C_OpenSession opens a session between an application and a token in
  *         a particular slot
  * @param  slotID is the slot ID
  * @param  flags indicates the type of session
  * @param  pApplication is an application-defined pointer to be passed to the
  *         notification callback
  * @param  Notify is the address of the notification callback function
  * @param  phSession points to the location that receives the handle for the new session
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_SESSION_COUNT
  * @retval CKR_SESSION_PARALLEL_NOT_SUPPORTED
  * @retval CKR_SESSION_READ_WRITE_SO_EXISTS
  * @retval CKR_SLOT_ID_INVALID
  * @retval CKR_TOKEN_NOT_PRESENT
  * @retval CKR_TOKEN_NOT_RECOGNIZED
  * @retval CKR_TOKEN_WRITE_PROTECTED
  * @retval CKR_ARGUMENTS_BAD
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define  C_OpenSession( slotID, flags, pApplication, Notify, phSession ) \
  KMS_OpenSession( slotID, flags, pApplication, Notify,  phSession)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define  C_OpenSession( slotID, flags, pApplication, Notify, phSession ) \
  KMS_IF_OpenSession( slotID, flags, pApplication, Notify,  phSession)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_CloseSession to access KMS service through Secure engine
  * @note   C_CloseSession closes a session between an application and a token
  * @param  hSession is the handle of the session
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_CloseSession( hSession ) \
  KMS_CloseSession( hSession )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_CloseSession( hSession ) \
  KMS_IF_CloseSession( hSession )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_CreateObject to access KMS service through Secure engine
  * @note   C_CreateObject creates a new object
  * @param  hSession is the session handle
  * @param  pTemplate points to the object template
  * @param  ulCount is the number of attributes in the template
  * @param  phObject points to the location that receives the new object handle
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_ATTRIBUTE_READ_ONLY
  * @retval CKR_ATTRIBUTE_TYPE_INVALID
  * @retval CKR_ATTRIBUTE_VALUE_INVALID
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_CURVE_NOT_SUPPORTED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_DOMAIN_PARAMS_INVALID
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_SESSION_READ_ONLY
  * @retval CKR_TEMPLATE_INCOMPLETE
  * @retval CKR_TEMPLATE_INCONSISTENT
  * @retval CKR_TOKEN_WRITE_PROTECTED
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_CreateObject( hSession, pTemplate, ulCount, phObject )\
  KMS_CreateObject( hSession, pTemplate, ulCount, phObject )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_CreateObject( hSession, pTemplate, ulCount, phObject )\
  KMS_IF_CreateObject( hSession, pTemplate, ulCount, phObject )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DestroyObject to access KMS service through Secure engine
  * @note   C_DestroyObject destroys an object
  * @param  hSession is the session handle
  * @param  hObject is the object handle
  * @retval CKR_ACTION_PROHIBITED
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OBJECT_HANDLE_INVALID
  * @retval CKR_OK
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_SESSION_READ_ONLY
  * @retval CKR_TOKEN_WRITE_PROTECTED
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DestroyObject( hSession, hObject)\
  KMS_DestroyObject( hSession, hObject)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DestroyObject( hSession, hObject)\
  KMS_IF_DestroyObject( hSession, hObject)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GetAttributeValue to access KMS service through Secure engine
  * @note   C_GetAttributeValue obtains the value of one or more attributes of an object
  * @param  hSession is the session handle
  * @param  hObject is the object handle
  * @param  pTemplate points to a template that specifies which attribute values
  *         are to be obtained, and receives the attribute values
  * @param  ulCount is the number of attributes in the template
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_ATTRIBUTE_SENSITIVE
  * @retval CKR_ATTRIBUTE_TYPE_INVALID
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OBJECT_HANDLE_INVALID
  * @retval CKR_OK
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GetAttributeValue( hSession, hObject, pTemplate, ulCount )\
  KMS_GetAttributeValue( hSession, hObject, pTemplate, ulCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GetAttributeValue( hSession, hObject, pTemplate, ulCount )\
  KMS_IF_GetAttributeValue( hSession, hObject, pTemplate, ulCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_SetAttributeValue to access KMS service through Secure engine
  * @note   C_SetAttributeValue modifies the value of one or more attributes of an object
  * @param  hSession is the session handle
  * @param  hObject is the object handle
  * @param  pTemplate points to a template that specifies which attribute values
  *         are to be modified and their new values
  * @param  ulCount is the number of attributes in the template
  * @retval CKR_ACTION_PROHIBITED
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_ATTRIBUTE_READ_ONLY
  * @retval CKR_ATTRIBUTE_TYPE_INVALID
  * @retval CKR_ATTRIBUTE_VALUE_INVALID
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OBJECT_HANDLE_INVALID
  * @retval CKR_OK
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_SESSION_READ_ONLY
  * @retval CKR_TEMPLATE_INCONSISTENT
  * @retval CKR_TOKEN_WRITE_PROTECTED
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_SetAttributeValue( hSession, hObject, pTemplate, ulCount )\
  KMS_SetAttributeValue( hSession, hObject, pTemplate, ulCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_SetAttributeValue( hSession, hObject, pTemplate, ulCount )\
  KMS_IF_SetAttributeValue( hSession, hObject, pTemplate, ulCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_FindObjectsInit to access KMS service through Secure engine
  * @note   C_FindObjectsInit initializes a search for token and session objects
  *         that match a template
  * @param  hSession is the handle of the session
  * @param  pTemplate points to a search template that specifies the attribute
  *         values to match
  * @param  ulCount is the number of attributes in the search template
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_ATTRIBUTE_TYPE_INVALID
  * @retval CKR_ATTRIBUTE_VALUE_INVALID
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_FindObjectsInit( hSession, pTemplate, ulCount )\
  KMS_FindObjectsInit( hSession, pTemplate, ulCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_FindObjectsInit( hSession, pTemplate, ulCount )\
  KMS_IF_FindObjectsInit( hSession, pTemplate, ulCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_FindObjects to access KMS service through Secure engine
  * @note   C_FindObjects continues a search for token and session objects that
  *         match a template, obtaining additional object handles
  * @param  hSession is the handle of the session
  * @param  phObject points to the location that receives the list (array) of
  *         additional object handles
  * @param  ulMaxObjectCount is the maximum number of object handles to be returned
  * @param  pulObjectCount points to the location that receives the actual number
  *         of object handles returned
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_FindObjects( hSession, phObject, ulMaxObjectCount, pulObjectCount )\
  KMS_FindObjects( hSession, phObject, ulMaxObjectCount, pulObjectCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_FindObjects( hSession, phObject, ulMaxObjectCount, pulObjectCount )\
  KMS_IF_FindObjects( hSession, phObject, ulMaxObjectCount, pulObjectCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_FindObjectsFinal to access KMS service through Secure engine
  * @note   C_FindObjectsFinal terminates a search for token and session objects
  * @param  hSession is the handle of the session
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_FindObjectsFinal( hSession )\
  KMS_FindObjectsFinal( hSession )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_FindObjectsFinal( hSession )\
  KMS_IF_FindObjectsFinal( hSession )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_EncryptInit to access KMS service through Secure engine
  * @note   C_EncryptInit initializes an encryption operation
  * @param  hSession is the handle of the session
  * @param  pMechanism points to the encryption mechanism
  * @param  hKey is the handle of the encryption key
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_KEY_FUNCTION_NOT_PERMITTED
  * @retval CKR_KEY_HANDLE_INVALID
  * @retval CKR_KEY_SIZE_RANGE
  * @retval CKR_KEY_TYPE_INCONSISTENT
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_EncryptInit( hSession, pMechanism, hKey) \
  KMS_EncryptInit( hSession, pMechanism, hKey)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_EncryptInit( hSession, pMechanism, hKey) \
  KMS_IF_EncryptInit( hSession, pMechanism, hKey)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_Encrypt to access KMS service through Secure engine
  * @note   C_Encrypt encrypts single-part data
  * @param  hSession is the handle of the session
  * @param  pData points to the data
  * @param  ulDataLen is the length in bytes of the data
  * @param  pEncryptedData points to the location that receives the encrypted data
  * @param  pulEncryptedDataLen points to the location that holds the length in bytes
  *         of the encrypted data
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DATA_INVALID
  * @retval CKR_DATA_LEN_RANGE
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Encrypt( hSession, pData, ulDataLen, pEncryptedData, pulEncryptedDataLen)\
  KMS_Encrypt( hSession, pData, ulDataLen, pEncryptedData, pulEncryptedDataLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Encrypt( hSession, pData, ulDataLen, pEncryptedData, pulEncryptedDataLen)\
  KMS_IF_Encrypt( hSession, pData, ulDataLen, pEncryptedData, pulEncryptedDataLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_EncryptUpdate to access KMS service through Secure engine
  * @note   C_EncryptUpdate continues a multiple-part encryption operation,
  *         processing another data part
  * @param  hSession is the handle of the session
  * @param  pPart points to the data part
  * @param  ulPartLen is the length of the data part
  * @param  pEncryptedPart points to the location that receives the encrypted data part
  * @param  pulEncryptedPartLen points to the location that holds the length in
  *         bytes of the encrypted data part
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DATA_LEN_RANGE
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_EncryptUpdate( hSession, pPart, ulPartLen, pEncryptedPart, pulEncryptedPartLen)\
  KMS_EncryptUpdate( hSession, pPart, ulPartLen, pEncryptedPart, pulEncryptedPartLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_EncryptUpdate( hSession, pPart, ulPartLen, pEncryptedPart, pulEncryptedPartLen)\
  KMS_IF_EncryptUpdate( hSession, pPart, ulPartLen, pEncryptedPart, pulEncryptedPartLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_EncryptFinal to access KMS service through Secure engine
  * @note   C_EncryptFinal finishes a multiple-part encryption operation
  * @param  hSession is the handle of the session
  * @param  pLastEncryptedPart points to the location that receives the last
  *         encrypted data part, if any
  * @param  pulLastEncryptedPartLen points to the
  *         location that holds the length of the last encrypted data part
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DATA_LEN_RANGE
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_EncryptFinal( hSession, pLastEncryptedPart, pulLastEncryptedPartLen)\
  KMS_EncryptFinal( hSession, pLastEncryptedPart, pulLastEncryptedPartLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_EncryptFinal( hSession, pLastEncryptedPart, pulLastEncryptedPartLen)\
  KMS_IF_EncryptFinal( hSession, pLastEncryptedPart, pulLastEncryptedPartLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DecryptInit to access KMS service through Secure engine
  * @note   C_DecryptInit initializes a decryption operation
  * @param  hSession is the handle of the session
  * @param  pMechanism points to the decryption mechanism
  * @param  hKey is the handle of the decryption key
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_KEY_FUNCTION_NOT_PERMITTED
  * @retval CKR_KEY_HANDLE_INVALID
  * @retval CKR_KEY_SIZE_RANGE
  * @retval CKR_KEY_TYPE_INCONSISTENT
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DecryptInit( hSession, pMechanism, hKey)\
  KMS_DecryptInit( hSession, pMechanism, hKey)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DecryptInit( hSession, pMechanism, hKey)\
  KMS_IF_DecryptInit( hSession, pMechanism, hKey)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_Decrypt to access KMS service through Secure engine
  * @note   C_Decrypt decrypts encrypted data in a single part
  * @param  hSession is the handle of the session
  * @param  pEncryptedData points to the encrypted data
  * @param  ulEncryptedDataLen is the length of the encrypted data
  * @param  pData points to the location that receives the recovered data
  * @param  pulDataLen points to the location that holds the length of the recovered data
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_ENCRYPTED_DATA_INVALID
  * @retval CKR_ENCRYPTED_DATA_LEN_RANGE
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Decrypt( hSession, pEncryptedData, ulEncryptedDataLen, pData, pulDataLen)\
  KMS_Decrypt( hSession, pEncryptedData, ulEncryptedDataLen, pData, pulDataLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Decrypt( hSession, pEncryptedData, ulEncryptedDataLen, pData, pulDataLen)\
  KMS_IF_Decrypt( hSession, pEncryptedData, ulEncryptedDataLen, pData, pulDataLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DecryptUpdate to access KMS service through Secure engine
  * @note   C_DecryptUpdate continues a multiple-part decryption operation,
  *         processing another encrypted data part
  * @param  hSession is the handle of the session
  * @param  pEncryptedPart points to the encrypted data part
  * @param  ulEncryptedPartLen is the length of the encrypted data part
  * @param  pPart points to the location that receives the recovered data part
  * @param  pulPartLen points to the location that holds the length of the recovered data part
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_ENCRYPTED_DATA_INVALID
  * @retval CKR_ENCRYPTED_DATA_LEN_RANGE
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DecryptUpdate( hSession, pEncryptedPart, ulEncryptedPartLen, pPart, pulPartLen)\
  KMS_DecryptUpdate( hSession, pEncryptedPart, ulEncryptedPartLen, pPart, pulPartLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DecryptUpdate( hSession, pEncryptedPart, ulEncryptedPartLen, pPart, pulPartLen)\
  KMS_IF_DecryptUpdate( hSession, pEncryptedPart, ulEncryptedPartLen, pPart, pulPartLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DecryptFinal to access KMS service through Secure engine
  * @note   C_DecryptFinal finishes a multiple-part decryption operation
  * @param  hSession is the handle of the session
  * @param  pLastPart points to the location that receives the last recovered
  *         data part, if any
  * @param  pulLastPartLen points to the location that holds the length of the
  *         last recovered data part
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_ENCRYPTED_DATA_INVALID
  * @retval CKR_ENCRYPTED_DATA_LEN_RANGE
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DecryptFinal( hSession, pLastPart, pulLastPartLen)\
  KMS_DecryptFinal( hSession, pLastPart, pulLastPartLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DecryptFinal( hSession, pLastPart, pulLastPartLen)\
  KMS_IF_DecryptFinal( hSession, pLastPart, pulLastPartLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DigestInit to access KMS service through Secure engine
  * @note   C_DigestInit initializes a message-digesting operation
  * @param  hSession is the handle of the session
  * @param  pMechanism points to the digesting mechanism
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DigestInit( hSession, pMechanism)\
  KMS_DigestInit( hSession, pMechanism)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DigestInit( hSession, pMechanism)\
  KMS_IF_DigestInit( hSession, pMechanism)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_Digest to access KMS service through Secure engine
  * @note   C_Digest digests data in a single part
  * @param  hSession is the handle of the session
  * @param  pData points to the data
  * @param  ulDataLen is the length of the data
  * @param  pDigest points to the location that receives the message digest
  * @param  pulDigestLen points to the location that holds the length of the message digest
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Digest( hSession, pData, ulDataLen, pDigest, pulDigestLen)\
  KMS_Digest( hSession, pData, ulDataLen, pDigest, pulDigestLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Digest( hSession, pData, ulDataLen, pDigest, pulDigestLen)\
  KMS_IF_Digest( hSession, pData, ulDataLen, pDigest, pulDigestLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DigestUpdate to access KMS service through Secure engine
  * @note   C_DigestUpdate continues a multiple-part message-digesting operation,
  *         processing another data part
  * @param  hSession is the handle of the session
  * @param  pPart points to the data part
  * @param  ulPartLen is the length of the data part
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DigestUpdate( hSession, pPart, ulPartLen)\
  KMS_DigestUpdate( hSession, pPart, ulPartLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DigestUpdate( hSession, pPart, ulPartLen)\
  KMS_IF_DigestUpdate( hSession, pPart, ulPartLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DigestFinal to access KMS service through Secure engine
  * @note   C_DigestFinal finishes a multiple-part message-digesting operation,
  *         returning the message digest
  * @param  hSession is the handle of the session
  * @param  pDigest points to the location that receives the message digest
  * @param  pulDigestLen points to the location that holds the length of the message digest
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DigestFinal( hSession, pDigest, pulDigestLen)\
  KMS_DigestFinal( hSession, pDigest, pulDigestLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DigestFinal( hSession, pDigest, pulDigestLen)\
  KMS_IF_DigestFinal( hSession, pDigest, pulDigestLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_SignInit to access KMS service through Secure engine
  * @note   C_SignInit initializes a signature operation, where the signature is
  *         an appendix to the data
  * @param  hSession is the handle of the session
  * @param  pMechanism points to the signature mechanism
  * @param  hKey is the handle of the signature key
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_KEY_FUNCTION_NOT_PERMITTED,CKR_KEY_HANDLE_INVALID
  * @retval CKR_KEY_SIZE_RANGE
  * @retval CKR_KEY_TYPE_INCONSISTENT
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_SignInit( hSession, pMechanism, hKey)\
  KMS_SignInit( hSession, pMechanism, hKey)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_SignInit( hSession, pMechanism, hKey)\
  KMS_IF_SignInit( hSession, pMechanism, hKey)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_Sign to access KMS service through Secure engine
  * @note   C_Sign signs data in a single part, where the signature is an appendix
  *         to the data
  * @param  hSession is the handle of the session
  * @param  pData points to the data
  * @param  ulDataLen is the length of the data
  * @param  pSignature points to the location that receives the signature
  * @param  pulSignatureLen points to the location that holds the length of the signature
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_BUFFER_TOO_SMALL
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DATA_INVALID
  * @retval CKR_DATA_LEN_RANGE
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @retval CKR_FUNCTION_REJECTED
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Sign( hSession, pData, ulDataLen, pSignature, pulSignatureLen)\
  KMS_Sign( hSession, pData, ulDataLen, pSignature, pulSignatureLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Sign( hSession, pData, ulDataLen, pSignature, pulSignatureLen)\
  KMS_IF_Sign( hSession, pData, ulDataLen, pSignature, pulSignatureLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_VerifyInit to access KMS service through Secure engine
  * @note   C_VerifyInit initializes a verification operation, where the signature
  *         is an appendix to the data
  * @param  hSession is the handle of the session
  * @param  pMechanism points to the structure that specifies the verification mechanism
  * @param  hKey is the handle of the verification key
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_KEY_FUNCTION_NOT_PERMITTED
  * @retval CKR_KEY_HANDLE_INVALID
  * @retval CKR_KEY_SIZE_RANGE
  * @retval CKR_KEY_TYPE_INCONSISTENT
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_VerifyInit( hSession, pMechanism, hKey)\
  KMS_VerifyInit( hSession, pMechanism, hKey)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_VerifyInit( hSession, pMechanism, hKey)\
  KMS_IF_VerifyInit( hSession, pMechanism, hKey)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_Verify to access KMS service through Secure engine
  * @note   C_Verify verifies a signature in a single-part operation, where the
  *         signature is an appendix to the data
  * @param  hSession is the handle of the session
  * @param  pData points to the data
  * @param  ulDataLen is the length of the data
  * @param  pSignature points to the signature
  * @param  ulSignatureLen is the length of the signature
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DATA_INVALID
  * @retval CKR_DATA_LEN_RANGE
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_NOT_INITIALIZED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_SIGNATURE_INVALID
  * @retval CKR_SIGNATURE_LEN_RANGE
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_Verify( hSession, pData, ulDataLen, pSignature, ulSignatureLen)\
  KMS_Verify( hSession, pData, ulDataLen, pSignature, ulSignatureLen)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_Verify( hSession, pData, ulDataLen, pSignature, ulSignatureLen)\
  KMS_IF_Verify( hSession, pData, ulDataLen, pSignature, ulSignatureLen)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_DeriveKey to access KMS service through Secure engine
  * @note   C_DeriveKey derives a key from a base key, creating a new key object
  * @param  hSession is the handle of the session
  * @param  pMechanism points to a structure that specifies the key derivation mechanism
  * @param  hBaseKey is the handle of the base key
  * @param  pTemplate points to the template
  *         for the new key
  * @param  ulAttributeCount is the number of  attributes in the template
  * @param  phKey points to the location that receives the handle of the derived key
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_ATTRIBUTE_READ_ONLY
  * @retval CKR_ATTRIBUTE_TYPE_INVALID
  * @retval CKR_ATTRIBUTE_VALUE_INVALID
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_CURVE_NOT_SUPPORTED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_DOMAIN_PARAMS_INVALID
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_KEY_HANDLE_INVALID
  * @retval CKR_KEY_SIZE_RANGE
  * @retval CKR_KEY_TYPE_INCONSISTENT
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_SESSION_READ_ONLY
  * @retval CKR_TEMPLATE_INCOMPLETE
  * @retval CKR_TEMPLATE_INCONSISTENT
  * @retval CKR_TOKEN_WRITE_PROTECTED
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_DeriveKey( hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey)\
  KMS_DeriveKey( hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_DeriveKey( hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey)\
  KMS_IF_DeriveKey( hSession, pMechanism, hBaseKey, pTemplate, ulAttributeCount, phKey)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GenerateKeyPair to access KMS service through Secure engine
  * @note   C_GenerateKeyPair generates a public/private key pair, creating new key objects
  * @param  hSession is the handle of the session
  * @param  pMechanism points to the key generation mechanism
  * @param  pPublicKeyTemplate points to the template for the public key
  * @param  ulPublicKeyAttributeCount is the number of attributes in the public-key template
  * @param  pPrivateKeyTemplate points to the template for the private key
  * @param  ulPrivateKeyAttributeCount is the number of attributes in the private-key template
  * @param  phPublicKey points to the location that receives the handle of the new public key
  * @param  phPrivateKey points to the location that receives the handle of the new private key
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_ATTRIBUTE_READ_ONLY
  * @retval CKR_ATTRIBUTE_TYPE_INVALID
  * @retval CKR_ATTRIBUTE_VALUE_INVALID
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_CURVE_NOT_SUPPORTED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_DOMAIN_PARAMS_INVALID
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_MECHANISM_INVALID
  * @retval CKR_MECHANISM_PARAM_INVALID
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_PIN_EXPIRED
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_SESSION_READ_ONLY
  * @retval CKR_TEMPLATE_INCOMPLETE
  * @retval CKR_TEMPLATE_INCONSISTENT
  * @retval CKR_TOKEN_WRITE_PROTECTED
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GenerateKeyPair( hSession, pMechanism, pPublicKeyTemplate, \
                           ulPublicKeyAttributeCount,  pPrivateKeyTemplate,\
                           ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey)\
KMS_GenerateKeyPair( hSession, pMechanism, pPublicKeyTemplate, \
                     ulPublicKeyAttributeCount,  pPrivateKeyTemplate,\
                     ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey)
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GenerateKeyPair( hSession, pMechanism, pPublicKeyTemplate, \
                           ulPublicKeyAttributeCount,  pPrivateKeyTemplate,\
                           ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey)\
KMS_IF_GenerateKeyPair( hSession, pMechanism, pPublicKeyTemplate, \
                        ulPublicKeyAttributeCount,  pPrivateKeyTemplate,\
                        ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey)
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 C_GenerateRandom to access KMS service through Secure engine
  * @note   C_GenerateRandom generates random or pseudo-random data
  * @param  hSession is the handle of the session
  * @param  pRandomData points to the location that receives the random data
  * @param  ulRandomLen is the length in bytes of the random or pseudo-random data
  *         to be generated
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_DEVICE_ERROR
  * @retval CKR_DEVICE_MEMORY
  * @retval CKR_DEVICE_REMOVED
  * @retval CKR_FUNCTION_CANCELED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_RANDOM_NO_RNG
  * @retval CKR_SESSION_CLOSED
  * @retval CKR_SESSION_HANDLE_INVALID
  * @retval CKR_USER_NOT_LOGGED_IN
  * @note   Refer to
            <a href=http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-errata01-os-complete.html>
  *         PKCS #11 Cryptographic Token Interface Base Specification Version 2.40</a> for more details on this API
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_GenerateRandom( hSession, pRandomData, ulRandomLen )\
  KMS_GenerateRandom( hSession, pRandomData, ulRandomLen )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_GenerateRandom( hSession, pRandomData, ulRandomLen )\
  KMS_IF_GenerateRandom( hSession, pRandomData, ulRandomLen )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 vendor defined C_STM_ImportBlob to access KMS service through Secure engine
  * @note   C_STM_ImportBlob authenticate, verify and decrypt a blob to update NVM static ID keys
  * @param  pHdr is the pointer to the encrypted blob header
  * @param  pFlash is the pointer to the blob location in flash
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  * @retval CKR_SIGNATURE_INVALID
  * @retval CKR_DATA_INVALID
  * @retval CKR_OPERATION_ACTIVE
  * @retval CKR_DEVICE_ERROR
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_STM_ImportBlob( pHdr, pFlash )\
  KMS_Objects_ImportBlob( pHdr, pFlash )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_STM_ImportBlob( pHdr, pFlash )\
  KMS_IF_ImportBlob( pHdr, pFlash )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 vendor defined C_STM_LockKeys to access KMS service through Secure engine
  * @note   C_STM_LockKeys lock keys
  * @param  pKeys is the pointer to the key handles to be locked
  * @param  ulCount is the number of keys to be locked
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_STM_LockKeys( pKeys, ulCount )\
  KMS_LockKeys( pKeys, ulCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_STM_LockKeys( pKeys, ulCount )\
  KMS_IF_LockKeys( pKeys, ulCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @brief  Redirection of PKCS11 vendor defined C_STM_LockServices to access KMS service through Secure engine
  * @note   C_STM_LockServices lock services
  * @param  pServices is the pointer to the services function identifier to be locked
  * @param  ulCount is the number of services to be locked
  * @retval CKR_ARGUMENTS_BAD
  * @retval CKR_CRYPTOKI_NOT_INITIALIZED
  * @retval CKR_FUNCTION_FAILED
  * @retval CKR_GENERAL_ERROR
  * @retval CKR_HOST_MEMORY
  * @retval CKR_OK
  */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
#define C_STM_LockServices( pServices, ulCount )\
  KMS_LockServices( pServices, ulCount )
#else /* KMS_NIKMS_ROUTER_BYPASS */
#define C_STM_LockServices( pServices, ulCount )\
  KMS_IF_LockServices( pServices, ulCount )
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/**
  * @}
  */

/** @addtogroup KMS_TKMS_PKCS11 tKMS APIs (PKCS#11 Standard Compliant)
  * @{
  */

/* Exported functions -------------------------------------------------------*/
uint32_t tKMS_GetCluster(void);

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

#endif /* TKMS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

