/**
  ******************************************************************************
  * @file    kms_checkconfig.h
  * @author  MCD Application Team
  * @brief   This file contains various checks of the contents of kms_config.h
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
#ifndef KMS_CHECKCONFIG_H
#define KMS_CHECKCONFIG_H

/* Checks --------------------------------------------------------------------*/
/*
 * KMS_NB_SESSIONS_MAX
 */
#if !defined(KMS_NB_SESSIONS_MAX)
#error "KMS_NB_SESSIONS_MAX not defined"
#endif /* KMS_NB_SESSIONS_MAX */

/*
 * KMS_NVM_SLOT_NUMBERS
 */
#if defined(KMS_NVM_SLOT_NUMBERS)
/* No dependency */
#endif /* KMS_NVM_SLOT_NUMBERS */

/*
 * KMS_VM_SLOT_NUMBERS
 */
#if defined(KMS_VM_SLOT_NUMBERS)
/* No dependency */
#endif /* KMS_VM_SLOT_NUMBERS */

/*
 * KMS_EXT_TOKEN_ENABLED
 */
#if defined(KMS_EXT_TOKEN_ENABLED)
/* No dependency */
#endif /* KMS_EXT_TOKEN_ENABLED */

/*
 * KMS_NVM_ENABLED
 */
#if defined(KMS_NVM_ENABLED)
#if !defined(KMS_NVM_SLOT_NUMBERS)
#error "KMS_NVM_ENABLED requires KMS_NVM_SLOT_NUMBERS"
#endif /* KMS_NVM_SLOT_NUMBERS */
#endif /* KMS_NVM_ENABLED */

/*
 * KMS_IMPORT_BLOB
 */
#if defined(KMS_IMPORT_BLOB)
#if !defined(KMS_NVM_ENABLED) \
 || !defined(KMS_DECRYPT) || !defined(KMS_AES_CBC) \
 || !defined(KMS_DIGEST) || !defined(KMS_SHA256) \
 || !defined(KMS_VERIFY) || !defined(KMS_ECDSA) || !defined(KMS_EC_SECP256)
#error "KMS_IMPORT_BLOB requires KMS_NVM_ENABLED, KMS_DECRYPT, KMS_AES_CBC, KMS_DIGEST, KMS_SHA256, KMS_VERIFY, \
KMS_ECDSA and KMS_EC_SECP256"
#endif /* !KMS_NVM_ENABLED || !KMS_DECRYPT || !KMS_AES_CBC || !KMS_DIGEST || !KMS_SHA256 || !KMS_VERIFY \
|| !KMS_ECDSA || !KMS_EC_SECP256 */
#if !(KMS_AES_CBC & KMS_FCT_DECRYPT) || !(KMS_SHA256 & KMS_FCT_DIGEST) || !(KMS_ECDSA & KMS_FCT_VERIFY)
#error "KMS_IMPORT_BLOB requires KMS_AES_CBC & KMS_FCT_DECRYP && KMS_SHA256 & KMS_FCT_DIGEST \
&& KMS_ECDSA &KMS_FCT_VERIFY"
#endif /* KMS_AES_CBC & KMS_FCT_DECRYP || KMS_SHA256 & KMS_FCT_DIGEST || KMS_ECDSA & KMS_FCT_VERIFY */

#if defined(KMS_IMPORT_BLOB_CHUNK_SIZE)
#if (KMS_IMPORT_BLOB_CHUNK_SIZE & 0x3UL)
#error "KMS_IMPORT_BLOB_CHUNK_SIZE must be a multiple of 4"
#endif /* KMS_IMPORT_BLOB_CHUNK_SIZE & 0x3UL */
#endif /* KMS_IMPORT_BLOB_CHUNK_SIZE */

#endif /* KMS_IMPORT_BLOB */

/*
 * KMS_MEM_USE_HEAP_ALLOCATOR & KMS_MEM_USE_POOL_ALLOCATOR & KMS_MEM_USE_CUSTOM_ALLOCATOR
 */
#if !defined(KMS_MEM_USE_HEAP_ALLOCATOR) \
 && !defined(KMS_MEM_USE_POOL_ALLOCATOR) \
 && !defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#error "You need to specify at least one memory allocation method"
#endif /* !KMS_MEM_USE_HEAP_ALLOCATOR && !KMS_MEM_USE_POOL_ALLOCATOR && !KMS_MEM_USE_CUSTOM_ALLOCATOR */
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR)
#if defined(KMS_MEM_USE_POOL_ALLOCATOR) || defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#error "You need to specify only one memory allocation method"
#endif /* KMS_MEM_USE_POOL_ALLOCATOR && KMS_MEM_USE_CUSTOM_ALLOCATOR */
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR */
#if defined(KMS_MEM_USE_POOL_ALLOCATOR)
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR) || defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#error "You need to specify only one memory allocation method"
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR && KMS_MEM_USE_CUSTOM_ALLOCATOR */
#endif /* KMS_MEM_USE_POOL_ALLOCATOR */
#if defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#if defined(KMS_MEM_USE_POOL_ALLOCATOR) || defined(KMS_MEM_USE_HEAP_ALLOCATOR)
#error "You need to specify only one memory allocation method"
#endif /* KMS_MEM_USE_POOL_ALLOCATOR && KMS_MEM_USE_HEAP_ALLOCATOR */
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */

/*
 * KMS_MEM_DEBUGGING
 */
#if defined(KMS_MEM_DEBUGGING)
#if defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#error "KMS_MEM_DEBUGGING is not compatible with KMS_MEM_USE_CUSTOM_ALLOCATOR"
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */
#endif /* KMS_MEM_DEBUGGING */

/*
 * KMS_MEM_CLEANING
 */
#if defined(KMS_MEM_CLEANING)
#if defined(KMS_MEM_USE_HEAP_ALLOCATOR) && !defined(KMS_MEM_DEBUGGING)
#error "KMS_MEM_DEBUGGING required when using KMS_MEM_CLEANING with KMS_MEM_USE_HEAP_ALLOCATOR"
#endif /* KMS_MEM_USE_HEAP_ALLOCATOR && !KMS_MEM_DEBUGGING */
#if defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#error "KMS_MEM_CLEANING is not compatible with KMS_MEM_USE_CUSTOM_ALLOCATOR"
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */
#endif /* KMS_MEM_CLEANING */

/*
 * KMS_MEM_LOGGING
 */
#if defined(KMS_MEM_LOGGING)
#if defined(KMS_MEM_USE_CUSTOM_ALLOCATOR)
#error "KMS_MEM_LOGGING is not compatible with KMS_MEM_USE_CUSTOM_ALLOCATOR"
#endif /* KMS_MEM_USE_CUSTOM_ALLOCATOR */
#endif /* KMS_MEM_LOGGING */

/*
 * KMS_NVM_DYNAMIC_ENABLED
 */
#if defined(KMS_NVM_DYNAMIC_ENABLED)
#if !defined(KMS_NVM_ENABLED)
#error "KMS_NVM_DYNAMIC_ENABLED requires KMS_NVM_ENABLED"
#endif /* KMS_NVM_ENABLED */
#endif /* KMS_NVM_DYNAMIC_ENABLED */

/*
 * KMS_VM_DYNAMIC_ENABLED
 */
#if defined(KMS_VM_DYNAMIC_ENABLED)
#if !defined(KMS_VM_SLOT_NUMBERS)
#error "KMS_VM_DYNAMIC_ENABLED requires KMS_VM_SLOT_NUMBERS"
#endif /* KMS_VM_SLOT_NUMBERS */
#if defined(KMS_NVM_DYNAMIC_ENABLED)
#error "KMS_VM_DYNAMIC_ENABLED is not compatible with KMS_NVM_DYNAMIC_ENABLED"
#endif /* KMS_NVM_DYNAMIC_ENABLED */
#endif /* KMS_VM_DYNAMIC_ENABLED */

/*
 * KMS_ENCRYPT
 */
#if defined(KMS_ENCRYPT)
#if !defined(KMS_AES_CBC) && !defined(KMS_AES_CCM) && !defined(KMS_AES_ECB) && !defined(KMS_AES_GCM)
#error "KMS_ENCRYPT requires KMS_AES_CBC, KMS_AES_CCM, KMS_AES_ECB OR KMS_AES_GCM"
#endif /* !KMS_AES_CBC && !KMS_AES_CCM && !KMS_AES_ECB && !KMS_AES_GCM */
#if !(KMS_AES_CBC & KMS_FCT_ENCRYPT) && !(KMS_AES_CCM & KMS_FCT_ENCRYPT) \
 && !(KMS_AES_ECB & KMS_FCT_ENCRYPT) && !(KMS_AES_GCM & KMS_FCT_ENCRYPT)
#error "KMS_ENCRYPT requires KMS_FCT_ENCRYPT to be enabled for at lest one encryption algorithm"
#endif /* !(KMS_AES_CBC & KMS_FCT_ENCRYPT) && !(KMS_AES_CCM & KMS_FCT_ENCRYPT)
       && !(KMS_AES_ECB & KMS_FCT_ENCRYPT) && !(KMS_AES_GCM & KMS_FCT_ENCRYPT) */
#endif /* KMS_ENCRYPT */

/*
 * KMS_DECRYPT
 */
#if defined(KMS_DECRYPT)
#if !defined(KMS_AES_CBC) && !defined(KMS_AES_CCM) && !defined(KMS_AES_ECB) && !defined(KMS_AES_GCM)
#error "KMS_DECRYPT requires KMS_AES_CBC, KMS_AES_CCM, KMS_AES_ECB OR KMS_AES_GCM"
#endif /* !KMS_AES_CBC && !KMS_AES_CCM && !KMS_AES_ECB && !KMS_AES_GCM */
#if !(KMS_AES_CBC & KMS_FCT_DECRYPT) && !(KMS_AES_CCM & KMS_FCT_DECRYPT) \
 && !(KMS_AES_ECB & KMS_FCT_DECRYPT) && !(KMS_AES_GCM & KMS_FCT_DECRYPT)
#error "KMS_ENCRYPT requires KMS_FCT_DECRYPT to be enabled for at lest one encryption algorithm"
#endif /* !(KMS_AES_CBC & KMS_FCT_DECRYPT) && !(KMS_AES_CCM & KMS_FCT_DECRYPT)
       && !(KMS_AES_ECB & KMS_FCT_DECRYPT) && !(KMS_AES_GCM & KMS_FCT_DECRYPT) */
#endif /* KMS_DECRYPT */

/*
 * KMS_DIGEST
 */
#if defined(KMS_DIGEST)
#if !defined(KMS_SHA1) && !defined(KMS_SHA256)
#error "KMS_DIGEST requires KMS_SHA1 OR KMS_SHA256"
#endif /* !KMS_SHA1 && !KMS_SHA256 */
#if !(KMS_SHA1 & KMS_FCT_DIGEST) && !(KMS_SHA256 & KMS_FCT_DIGEST)
#error "KMS_DIGEST requires KMS_FCT_DIGEST to be enabled for at lest one digesting algorithm"
#endif /* !(KMS_SHA1 & KMS_FCT_DIGEST) && !(KMS_SHA256 & KMS_FCT_DIGEST) */
#endif /* KMS_DIGEST */

/*
 * KMS_SIGN
 */
#if defined(KMS_SIGN)
#if !defined(KMS_RSA) && !defined(KMS_AES_CMAC)
#error "KMS_SIGN requires KMS_RSA OR KMS_AES_CMAC"
#endif /* !KMS_RSA && !KMS_AES_CMAC */
#if !(KMS_RSA & KMS_FCT_SIGN) && !(KMS_AES_CMAC & KMS_FCT_SIGN)
#error "KMS_SIGN requires KMS_FCT_SIGN to be enabled for at lest one signature algorithm"
#endif /* !(KMS_RSA & KMS_FCT_SIGN) && !(KMS_AES_CMAC & KMS_FCT_SIGN) */
#endif /* KMS_SIGN */

/*
 * KMS_VERIFY
 */
#if defined(KMS_VERIFY)
#if !defined(KMS_RSA) && !defined(KMS_ECDSA) && !defined(KMS_AES_CMAC)
#error "KMS_VERIFY requires KMS_RSA, KMS_ECDSA OR KMS_AES_CMAC"
#endif /* !KMS_RSA && !KMS_ECDSA && !KMS_AES_CMAC */
#if !(KMS_RSA & KMS_FCT_VERIFY) && !(KMS_ECDSA & KMS_FCT_VERIFY) && !(KMS_AES_CMAC & KMS_FCT_VERIFY)
#error "KMS_SIGN requires KMS_FCT_SIGN to be enabled for at lest one signature algorithm"
#endif /* !(KMS_RSA & KMS_FCT_VERIFY) && !(KMS_ECDSA & KMS_FCT_VERIFY) && !(KMS_AES_CMAC & KMS_FCT_VERIFY) */
#endif /* KMS_VERIFY */

/*
 * KMS_DERIVE_KEY
 */
#if defined(KMS_DERIVE_KEY)
#if !defined(KMS_NVM_DYNAMIC_ENABLED) && !defined(KMS_VM_DYNAMIC_ENABLED)
#error "KMS_DERIVE_KEY requires at least one of KMS_NVM_DYNAMIC_ENABLED and KMS_VM_DYNAMIC_ENABLED"
#endif /* !KMS_NVM_DYNAMIC_ENABLED && !KMS_VM_DYNAMIC_ENABLED*/
#if !defined(KMS_AES_ECB) && !defined(KMS_ECDSA)
#error "KMS_DERIVE_KEY requires KMS_AES_ECB OR KMS_ECDSA"
#endif /* !KMS_AES_ECB && !KMS_ECDSA */
#if !(KMS_AES_ECB & KMS_FCT_DERIVE_KEY) && !(KMS_ECDSA & KMS_FCT_DERIVE_KEY)
#error "KMS_DERIVE_KEY requires KMS_FCT_DERIVE_KEY to be enabled for at lest one signature algorithm"
#endif /* !(KMS_AES_ECB & KMS_FCT_DERIVE_KEY) && !(KMS_ECDSA & KMS_FCT_DERIVE_KEY) */
#endif /* KMS_DERIVE_KEY */

/*
 * KMS_SEARCH
 */
#if defined(KMS_SEARCH)
/* No dependency */
#endif /* KMS_SEARCH */

/*
 * KMS_GENERATE_KEYS
 */
#if defined(KMS_GENERATE_KEYS)
#if (!defined(KMS_NVM_DYNAMIC_ENABLED) && !defined(KMS_VM_DYNAMIC_ENABLED)) || !defined(KMS_ECDSA)
#error "KMS_GENERATE_KEYS requires KMS_ECDSA AND at least one of KMS_NVM_DYNAMIC_ENABLED and KMS_VM_DYNAMIC_ENABLED"
#endif /* !KMS_NVM_DYNAMIC_ENABLED && !KMS_ECDSA */
#if !(KMS_ECDSA & KMS_FCT_GENERATE_KEYS)
#error "KMS_GENERATE_KEYS requires KMS_FCT_GENERATE_KEYS to be enabled for at lest one signature algorithm"
#endif /* !(KMS_ECDSA & KMS_FCT_GENERATE_KEYS) */
#endif /* KMS_GENERATE_KEYS */

/*
 * KMS_GENERATE_RANDOM
 */
#if defined(KMS_GENERATE_RANDOM)
/* No dependency */
#endif /* KMS_GENERATE_RANDOM */

/*
 * KMS_ATTRIBUTES
 */
#if defined(KMS_ATTRIBUTES)
/* No dependency */
#endif /* KMS_ATTRIBUTES */

/*
 * KMS_OBJECTS
 */
#if defined(KMS_OBJECTS)
#if !defined(KMS_NVM_DYNAMIC_ENABLED) && !defined(KMS_VM_DYNAMIC_ENABLED)
#error "KMS_OBJECTS requires at least one of KMS_NVM_DYNAMIC_ENABLED and KMS_VM_DYNAMIC_ENABLED"
#endif /* !KMS_NVM_DYNAMIC_ENABLED && !KMS_VM_DYNAMIC_ENABLED */
#endif /* KMS_OBJECTS */

/*
 * KMS_PKCS11_GET_FUNCTION_LIST_SUPPORT
 */
#if defined(KMS_PKCS11_GET_FUNCTION_LIST_SUPPORT)
/* No dependency */
#endif /* KMS_PKCS11_GET_FUNCTION_LIST_SUPPORT */

/*
 * KMS_PKCS11_COMPLIANCE
 */
#if defined(KMS_PKCS11_COMPLIANCE)
/* No dependency */
#endif /* KMS_PKCS11_COMPLIANCE */

/*
 * KMS_NIKMS_ROUTER_BYPASS
 */
#if defined(KMS_NIKMS_ROUTER_BYPASS)
/* No dependency */
#endif /* KMS_NIKMS_ROUTER_BYPASS */

/*
 * KMS_SE_CHECK_PARAMS
 */
#if defined(KMS_SE_CHECK_PARAMS)
/* No dependency */
#endif /* KMS_SE_CHECK_PARAMS */

/*
 * KMS_SE_LOCK_KEYS
 */
#if defined(KMS_SE_LOCK_KEYS)
#if !defined(KMS_SE_LOCK_KEYS_MAX) || (KMS_SE_LOCK_KEYS_MAX == 0)
#error "KMS_SE_LOCK_KEYS requires KMS_SE_LOCK_KEYS_MAX > 0"
#endif /* !KMS_SE_LOCK_KEYS_MAX || KMS_SE_LOCK_KEYS_MAX > 0 */
#endif /* KMS_SE_LOCK_KEYS */

/*
 * KMS_SE_LOCK_SERVICES
 */
#if defined(KMS_SE_LOCK_SERVICES)
#if !defined(KMS_SE_LOCK_SERVICES_MAX) || (KMS_SE_LOCK_SERVICES_MAX == 0)
#error "KMS_SE_LOCK_KEYS requires KMS_SE_LOCK_SERVICES_MAX > 0"
#endif /* !KMS_SE_LOCK_SERVICES_MAX || KMS_SE_LOCK_SERVICES_MAX > 0 */
#endif /* KMS_SE_LOCK_SERVICES */

/*
 * KMS_AES_CBC
 */
#if defined(KMS_AES_CBC)
#if !defined(KMS_ENCRYPT) && !defined(KMS_DECRYPT)
#error "KMS_AES_CBC requires KMS_ENCRYPT OR KMS_DECRYPT"
#endif /* !KMS_ENCRYPT && !KMS_DECRYPT */
#if (KMS_AES_CBC == 0)
#error "KMS_AES_CBC definition should include algorithm purpose information"
#endif /* KMS_AES_CBC == 0 */
#endif /* KMS_AES_CBC */

/*
 * KMS_AES_CCM
 */
#if defined(KMS_AES_CCM)
#if !defined(KMS_ENCRYPT) && !defined(KMS_DECRYPT)
#error "KMS_AES_CCM requires KMS_ENCRYPT OR KMS_DECRYPT"
#endif /* !KMS_ENCRYPT && !KMS_DECRYPT */
#if (KMS_AES_CCM == 0)
#error "KMS_AES_CCM definition should include algorithm purpose information"
#endif /* KMS_AES_CCM == 0 */
#endif /* KMS_AES_CCM */

/*
 * KMS_AES_CMAC
 */
#if defined(KMS_AES_CMAC)
#if !defined(KMS_SIGN) && !defined(KMS_VERIFY)
#error "KMS_AES_CMAC requires KMS_SIGN OR KMS_VERIFY"
#endif /* !KMS_SIGN && !KMS_VERIFY */
#if (KMS_AES_CMAC == 0)
#error "KMS_AES_CMAC definition should include algorithm purpose information"
#endif /* KMS_AES_CMAC == 0 */
#endif /* KMS_AES_CMAC */

/*
 * KMS_AES_ECB
 */
#if defined(KMS_AES_ECB)
#if !defined(KMS_ENCRYPT) && !defined(KMS_DECRYPT)
#error "KMS_AES_ECB requires KMS_ENCRYPT OR KMS_DECRYPT"
#endif /* !KMS_ENCRYPT && !KMS_DECRYPT */
#if (KMS_AES_ECB == 0)
#error "KMS_AES_ECB definition should include algorithm purpose information"
#endif /* KMS_AES_ECB == 0 */
#endif /* KMS_AES_ECB */

/*
 * KMS_AES_GCM
 */
#if defined(KMS_AES_GCM)
#if !defined(KMS_ENCRYPT) && !defined(KMS_DECRYPT)
#error "KMS_AES_GCM requires KMS_ENCRYPT OR KMS_DECRYPT"
#endif /* !KMS_ENCRYPT && !KMS_DECRYPT */
#if (KMS_AES_GCM == 0)
#error "KMS_AES_GCM definition should include algorithm purpose information"
#endif /* KMS_AES_GCM == 0 */
#endif /* KMS_AES_GCM */

/*
 * KMS_RSA
 */
#if defined(KMS_RSA)
#if !defined(KMS_SIGN) && !defined(KMS_VERIFY)
#error "KMS_RSA requires KMS_SIGN OR KMS_VERIFY"
#endif /* !KMS_SIGN && !KMS_VERIFY */
#if !defined(KMS_RSA_1024) && !defined(KMS_RSA_2048) && !defined(KMS_RSA_3072)
#error "KMS_RSA requires KMS_RSA_1024, KMS_RSA_2048 OR KMS_RSA_3072"
#endif /* !KMS_RSA_1024 && !KMS_RSA_2048 && !KMS_RSA_3072 */
#if (KMS_RSA == 0)
#error "KMS_RSA definition should include algorithm purpose information"
#endif /* KMS_RSA == 0 */
#endif /* KMS_RSA */

/*
 * KMS_RSA_1024
 */
#if defined(KMS_RSA_1024)
#if !defined(KMS_RSA)
#error "KMS_RSA_1024 requires KMS_RSA"
#endif /* KMS_RSA */
#endif /* KMS_RSA_1024 */

/*
 * KMS_RSA_2048
 */
#if defined(KMS_RSA_2048)
#if !defined(KMS_RSA)
#error "KMS_RSA_2048 requires KMS_RSA"
#endif /* KMS_RSA */
#endif /* KMS_RSA_2048 */

/*
 * KMS_RSA_3072
 */
#if defined(KMS_RSA_3072)
#if !defined(KMS_RSA)
#error "KMS_RSA_3072 requires KMS_RSA"
#endif /* KMS_RSA */
#endif /* KMS_RSA_3072 */

/*
 * KMS_ECDSA
 */
#if defined(KMS_ECDSA)
#if !defined(KMS_VERIFY) && !defined(KMS_DERIVE_KEY) && !defined(KMS_GENERATE_KEYS)
#error "KMS_ECDSA requires KMS_VERIFY, KMS_DERIVE_KEY OR KMS_GENERATE_KEYS"
#endif /* !KMS_VERIFY && !KMS_DERIVE_KEY && !KMS_GENERATE_KEYS */
#if !defined(KMS_EC_SECP192) && !defined(KMS_EC_SECP256) && !defined(KMS_EC_SECP384)
#error "KMS_ECDSA requires KMS_EC_SECP192, KMS_EC_SECP256 OR KMS_EC_SECP384"
#endif /* !KMS_EC_SECP192 && !KMS_EC_SECP256 && !KMS_EC_SECP384 */
#if (KMS_ECDSA == 0)
#error "KMS_ECDSA definition should include algorithm purpose information"
#endif /* KMS_ECDSA == 0 */
#endif /* KMS_ECDSA */

/*
 * KMS_EC_SECP192
 */
#if defined(KMS_EC_SECP192)
#if !defined(KMS_ECDSA)
#error "KMS_EC_SECP192 requires KMS_ECDSA"
#endif /* KMS_ECDSA */
#endif /* KMS_EC_SECP192 */

/*
 * KMS_EC_SECP256
 */
#if defined(KMS_EC_SECP256)
#if !defined(KMS_ECDSA)
#error "KMS_EC_SECP256 requires KMS_ECDSA"
#endif /* KMS_ECDSA */
#endif /* KMS_EC_SECP256 */

/*
 * KMS_EC_SECP384
 */
#if defined(KMS_EC_SECP384)
#if !defined(KMS_ECDSA)
#error "KMS_EC_SECP384 requires KMS_ECDSA"
#endif /* KMS_ECDSA */
#endif /* KMS_EC_SECP384 */

/*
 * KMS_SHA1
 */
#if defined(KMS_SHA1)
#if !defined(KMS_DIGEST)
#error "KMS_SHA1 requires KMS_DIGEST"
#endif /* KMS_DIGEST */
#if (KMS_SHA1 == 0)
#error "KMS_SHA1 definition should include algorithm purpose information"
#endif /* KMS_SHA1 == 0 */
#endif /* KMS_SHA1 */

/*
 * KMS_SHA256
 */
#if defined(KMS_SHA256)
#if !defined(KMS_DIGEST)
#error "KMS_SHA256 requires KMS_DIGEST"
#endif /* KMS_DIGEST */
#if (KMS_SHA256 == 0)
#error "KMS_SHA256 definition should include algorithm purpose information"
#endif /* KMS_SHA256 == 0 */
#endif /* KMS_SHA256 */

#endif /* KMS_CHECKCONFIG_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
