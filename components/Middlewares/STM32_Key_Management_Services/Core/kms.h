/**
  ******************************************************************************
  * @file    kms.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module functionalities.
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
#ifndef KMS_H
#define KMS_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/*
 * Cryptoki configuration macros
 */

/**
  *  @brief The indirection string for making a pointer to an object
  */
#define CK_PTR *

/**
  *  @brief A macro which makes an importable Cryptoki library function declaration out of
  *         a return type and a function name
  */
#define CK_DECLARE_FUNCTION(returnType, name) \
  returnType name

/**
  *  @brief A macro which makes a Cryptoki API function pointer declaration or function pointer
  *         type declaration out of a return type and a function name
  */
#define CK_DECLARE_FUNCTION_POINTER(returnType, name) \
  typedef returnType (* name)

/**
  *  @brief A macro which makes a function pointer type for an application callback out of
  *         a return type for the callback and a name for the callback
  */
#define CK_CALLBACK_FUNCTION(returnType, name) \
  returnType (* name)

#ifndef NULL_PTR
/**
  *  @brief This macro is the value of a NULL pointer
  */
#define NULL_PTR ((void *)NULL)
#endif /* NULL_PTR */

/*
 * PKCS#11 headers
 */
#include "pkcs11.h"

/*
 * KMS configuration & interface headers
 */
#include "kms_config.h"
#if defined(KMS_ENABLED)
#include "kms_blob_headers.h"
#include "kms_blob_metadata.h"
#include "kms_checkconfig.h"
#endif /* KMS_ENABLED */


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* KMS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
