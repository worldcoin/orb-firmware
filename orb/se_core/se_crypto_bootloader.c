/**
  ******************************************************************************
  * @file    se_crypto_bootloader.c
  * @author  MCD Application Team
  * @brief   Secure Engine CRYPTO module.
  *          This file provides set of firmware functions to manage SE Crypto
  *          functionalities. These services are used by the bootloader.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <string.h>               /* added for memcpy */
#include "se_crypto_bootloader.h"
#include "se_crypto_common.h"     /* re-use common crypto code (wrapper to cryptolib in this example)  */
#include "se_low_level.h"         /* required for assert_param */
#include "se_key.h"               /* required to access the keys when not provided as input parameter (metadata
                                     authentication) */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#include "mapping_sbsfu.h"
#elif defined (__ICCARM__) || defined(__GNUC__)
#include "mapping_export.h"
#endif /* __ICCARM__ || __GNUC__ */

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup  SE_CRYPTO SE Crypto
  * @brief Crypto services (used by the bootloader and common crypto functions)
  * @{
  */

/** @defgroup  SE_CRYPTO_BOOTLOADER SE Crypto for Bootloader
  * @brief Crypto functions used by the bootloader.
  * @note In this example: \li the AES GCM crypto scheme is supported (@ref SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM),
  *                        \li the ECC DSA without encryption crypto scheme is supported
  *                        \li (@ref SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256),
  *                        \li the ECC DSA+AES-CBC crypto scheme is supported
  *                        \li (@ref SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256).
  * @{
  */

/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Variables Private Variables
  * @{
  */

#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) )
/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Variables_Symmeric_Key Symmetric Key Handling
  *  @brief Variable(s) used to handle the symmetric key(s).
  *  @note All these variables must be located in protected memory.
  *  @{
  */

static uint8_t m_aSE_FirmwareKey[SE_SYMKEY_LEN];        /* Variable used to store the Key inside the protected area */


/**
  * @}
  */

#endif /* SECBOOT_CRYPTO_SCHEME */

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Variables_AES_GCM AES GCM Private Variables
  * @Private variables used for AES GCM symmetric crypto
  * @{
  */
extern AESGCMctx_stt m_xSE_AESGCMCtx; /*!<Variable used to store the AES128 context */

/**
  * @}
  */

#endif /* SECBOOT_CRYPTO_SCHEME */

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Variables_AES_CBC AES CBC variables
  *  @brief  Advanced Encryption Standard (AES), CBC (Cipher-Block Chaining) with support for Ciphertext Stealing
  *  @note   We do not use local variable(s) because we want this to be in the protected area (and the stack is not
  protected),
  *          and also because the contexts are used to store internal states.
  *  @{
  */
static AESCBCctx_stt m_AESCBCctx; /*!<Variable used to store the AES CBC context */
/**
  * @}
  */
#endif /* SECBOOT_CRYPTO_SCHEME */

#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Variables_SHA256 SHA256 variables
  *  @brief  Secure Hash Algorithm SHA-256.
  *  @note   We do not use local variable(s) because the context is used to store internal states.
  *  @{
  */
static SHA256ctx_stt m_SHA256ctx; /*!< Variable used to store the SHA256 context (this one does not really need to be
                                       protected as it is a HASH) */

/**
  * @}
  */
#endif /* SECBOOT_CRYPTO_SCHEME */



/**
  * @}
  */

/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Macros Private Macros
  * @{
  */


/**
  * @brief Clean up the RAM area storing the Firmware key.
  *        This applies only to the secret symmetric key loaded with SE_ReadKey_x().
  */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
#define SE_CLEAN_UP_FW_KEY() \
  do { \
    (void)memcpy(m_aSE_FirmwareKey, (void const *)(SE_STARTUP_REGION_ROM_START + (SysTick->VAL % 0xFFFU)), \
                 sizeof(m_aSE_FirmwareKey)); \
    (void)memcpy((void *)&m_AESCBCctx, (void const *)(SE_STARTUP_REGION_ROM_START + (SysTick->VAL % 0xFFFU)), \
                 sizeof(m_AESCBCctx)); \
  } while(0)

#elif (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)

#define SE_CLEAN_UP_FW_KEY() \
  do { \
    (void)memcpy(m_aSE_FirmwareKey, (void const *)(SE_STARTUP_REGION_ROM_START + (SysTick->VAL % 0xFFFU)), \
                 sizeof(m_aSE_FirmwareKey)); \
    (void)memcpy((void *)&m_xSE_AESGCMCtx, (void const *)(SE_STARTUP_REGION_ROM_START + (SysTick->VAL % 0xFFFU)), \
                 sizeof(m_xSE_AESGCMCtx)); \
  } while(0)

#else

#define SE_CLEAN_UP_FW_KEY() do { /* do nothing */; } while(0)

#endif /* SECBOOT_CRYPTO_SCHEME */

/**
  * @brief Clean up the RAM area storing the ECC Public Key.
  *        This applies only to the public asymmetric key loaded with SE_ReadKey_Pub().
  */
#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )

#define SE_CLEAN_UP_PUB_KEY() \
  do { \
    (void)memcpy(m_aSE_PubKey, (void const *)(SE_STARTUP_REGION_ROM_START + (SysTick->VAL % 0xFFFU)), \
                 sizeof(m_aSE_PubKey)); \
    (void)memcpy((void *)&m_SHA256ctx, (void const *)(SE_STARTUP_REGION_ROM_START + (SysTick->VAL % 0xFFFU)), \
                 sizeof(m_SHA256ctx)); \
  } while(0)

#else

#define SE_CLEAN_UP_PUB_KEY() do { /* do nothing */; } while(0)

#endif /* SECBOOT_CRYPTO_SCHEME */

/**
  * @}
  */

/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Functions Private Functions
  *  @brief These are private functions used internally by the crypto services provided to the bootloader
  *         i.e. the exported functions implemented in this file (@ref SE_CRYPTO_BOOTLOADER_Exported_Functions).
  *  @note  These functions are not part of the common crypto code because they are used by the bootloader services
  *         only.
  * @{
  */

/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Functions_AES AES Functions
  *  @brief Helpers for AES: high level wrappers for the Cryptolib.
  *  @note This group of functions is empty because we do not provide high level wrappers for AES.
  * @{
  */

/* no high level wrappers provided for AES primitives: the Cryptolib APIs are called directly from  the services
  (@ref SE_CRYPTO_BOOTLOADER_Exported_Functions) */

/**
  * @}
  */

/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Functions_Readkey Readkey Functions
  *  @brief Read the relevant AES symmetric key.
  * @{
  */

#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM) )
/**
  * @brief  AES readkey function.
  * @param  pxSE_Metadata: Firmware metadata.
  * @retval none
  */
static void SE_CRYPTO_AES_ReadKey(SE_FwRawHeaderTypeDef *pxSE_Metadata)
{
  /* Read the AES Symmetric Key :
     - SFU1 / SE_ReadKey_1() ==> read key for SLOT_ACTIVE_1
     - SFU2 / SE_ReadKey_2() ==> read key for SLOT_ACTIVE_2
     - SFU3 / SE_ReadKey_3() ==> read key for SLOT_ACTIVE_3 */
  if (memcmp(pxSE_Metadata->SFUMagic, SFUM_1, strlen(SFUM_1)) == 0)
  {
    SE_ReadKey_1(&(m_aSE_FirmwareKey[0]));
  }
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  else if (memcmp(pxSE_Metadata->SFUMagic, SFUM_2, strlen(SFUM_2)) == 0)
  {
    SE_ReadKey_2(&(m_aSE_FirmwareKey[0]));
  }
#endif  /* (NB_FW_IMAGES > 1) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  else if (memcmp(pxSE_Metadata->SFUMagic, SFUM_3, strlen(SFUM_3)) == 0)
  {
    SE_ReadKey_3(&(m_aSE_FirmwareKey[0]));
  }
#endif  /* (NB_FW_IMAGES > 2) */
  else
  {
    /* nothing to do */
    ;
  }
}
#endif /* SECBOOT_CRYPTO_SCHEME */


#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
/**
  * @brief  Read public key function.
  * @param  pxSE_Metadata: Firmware metadata.
  * @retval none
  */
static void SE_CRYPTO_ReadKey_Pub(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint8_t *paSE_PubKey)
{
  /* Read the AES Symmetric Key :
     - SFU1 / SE_ReadKey_1() ==> read key for SLOT_ACTIVE_1
     - SFU2 / SE_ReadKey_2() ==> read key for SLOT_ACTIVE_2
     - SFU3 / SE_ReadKey_3() ==> read key for SLOT_ACTIVE_3 */
  if (memcmp(pxSE_Metadata->SFUMagic, SFUM_1, strlen(SFUM_1)) == 0)
  {
    SE_ReadKey_1_Pub(paSE_PubKey);
  }
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  else if (memcmp(pxSE_Metadata->SFUMagic, SFUM_2, strlen(SFUM_2)) == 0)
  {
    SE_ReadKey_2_Pub(paSE_PubKey);
  }
#endif  /* (NB_FW_IMAGES > 1) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  else if (memcmp(pxSE_Metadata->SFUMagic, SFUM_3, strlen(SFUM_3)) == 0)
  {
    SE_ReadKey_3_Pub(paSE_PubKey);
  }
#endif  /* (NB_FW_IMAGES > 2) */
  else
  {
    /* nothing to do */
    ;
  }
}
#endif /* SECBOOT_CRYPTO_SCHEME */

/**
  * @}
  */

/** @defgroup SE_CRYPTO_BOOTLOADER_Private_Functions_HASH Hash Functions
  *  @brief Hash algorithm(s): high level wrapper(s) for the Cryptolib.
  * @{
  */


#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
/**
  * @brief  SHA256 HASH digest compute example.
  * @param  InputMessage: pointer to input message to be hashed.
  * @param  InputMessageLength: input data message length in byte.
  * @param  MessageDigest: pointer to output parameter that will handle message digest
  * @param  MessageDigestLength: pointer to output digest length.
  * @retval error status: can be HASH_SUCCESS if success or one of
  *         HASH_ERR_BAD_PARAMETER, HASH_ERR_BAD_CONTEXT,
  *         HASH_ERR_BAD_OPERATION if an error occurred.
  */
static int32_t SE_CRYPTO_SHA256_HASH_DigestCompute(const uint8_t *InputMessage, const int32_t InputMessageLength,
                                                   uint8_t *MessageDigest, int32_t *MessageDigestLength)
{
  SHA256ctx_stt P_pSHA256ctx;
  int32_t error_status;

  /* Set the size of the desired hash digest */
  P_pSHA256ctx.mTagSize = CRL_SHA256_SIZE;

  /* Set flag field to default value */
  P_pSHA256ctx.mFlags = E_HASH_DEFAULT;

  error_status = SHA256_Init(&P_pSHA256ctx);

  /* check for initialization errors */
  if (error_status == HASH_SUCCESS)
  {
    /* Add data to be hashed */
    error_status = SHA256_Append(&P_pSHA256ctx,
                                 InputMessage,
                                 InputMessageLength);

    if (error_status == HASH_SUCCESS)
    {
      /* retrieve */
      error_status = SHA256_Finish(&P_pSHA256ctx, MessageDigest, MessageDigestLength);
    }
  }

  return error_status;
}


#endif /* SECBOOT_CRYPTO_SCHEME */

/**
  * @}
  */
/**
  * @}
  */


/** @defgroup SE_CRYPTO_BOOTLOADER_Exported_Functions Exported Functions
  * @brief The implementation of these functions is crypto-dependent (but the API is crypto-agnostic).
  *        These functions use the generic SE_FwRawHeaderTypeDef structure to fill crypto specific structures.
  * @{
  */

/**
  * @brief Secure Engine Encrypt Init function.
  *        It is a wrapper of the Cryptolib Encrypt_Init function included in the protected area.
  * @param pxSE_Metadata: Firmware metadata.
  * @param SE_FwType: Type of Fw Image.
  *        This parameter can be SE_FW_IMAGE_COMPLETE or SE_FW_IMAGE_PARTIAL.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Encrypt_Init(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType)
{
  SE_ErrorStatus e_ret_status;

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  SE_GCMInitTypeDef se_gcm_init;
  /* Variables to handle FW image */
  uint32_t fw_size;
  uint8_t *fw_tag;
#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  /* The bootloader does not need the encrypt service in this crypto scheme: reject this request later on */
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM */

  /* Check the pointers allocation */
  if (pxSE_Metadata == NULL)
  {
    return SE_ERROR;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Check the parameters value and set fw_size and fw_tag to check */
  if (SE_FwType == SE_FW_IMAGE_COMPLETE)
  {
    fw_size = pxSE_Metadata->FwSize;
    fw_tag = pxSE_Metadata->FwTag;
  }
  else if (SE_FwType == SE_FW_IMAGE_PARTIAL)
  {
    fw_size = pxSE_Metadata->PartialFwSize;
    fw_tag = pxSE_Metadata->PartialFwTag;
  }
  else
  {
    return SE_ERROR;
  }

  /* Read the Symmetric Key */
  SE_CRYPTO_AES_ReadKey(pxSE_Metadata);

  /* Prepare a local structure with appropriate types (this step could be skipped) */
  se_gcm_init.HeaderSize = 0;
  se_gcm_init.PayloadSize = (int32_t)fw_size;
  se_gcm_init.pNonce = (uint8_t *)pxSE_Metadata->Nonce;
  se_gcm_init.NonceSize = SE_NONCE_LEN;
  se_gcm_init.pTag = (uint8_t *)fw_tag;
  se_gcm_init.TagSize = (int32_t)SE_TAG_LEN;


  /* Check the pointers allocation */
  if ((se_gcm_init.pNonce == NULL) || (se_gcm_init.pTag == NULL))
  {
    return SE_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_SE_CRYPTO_AES_GCM_NONCE_SIZE(se_gcm_init.NonceSize));
  assert_param(IS_SE_CRYPTO_AES_GCM_TAG_SIZE(se_gcm_init.TagSize));

  /* Common Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Encrypt_Init(m_aSE_FirmwareKey, &se_gcm_init);


#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )

  /* The bootloader does not need the encrypt service in this crypto scheme: reject this request */
  /* Prevent unused argument(s) compilation warning */
  UNUSED(SE_FwType);
  e_ret_status = SE_ERROR;
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine Header Append function.
  *        It is a wrapper of the Cryptolib Header_Append function included in the protected area.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Header_Append(const uint8_t *pInputBuffer, int32_t InputSize)
{
  SE_ErrorStatus e_ret_status;

  /* Check the pointers allocation */
  if (pInputBuffer == NULL)
  {
    return SE_ERROR;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Common Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Header_Append(pInputBuffer, InputSize);
#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  /* The bootloader does not need this service in this crypto scheme: reject this request */
  e_ret_status = SE_ERROR;

  /* Prevent unused argument(s) compilation warning */
  UNUSED(InputSize);
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM */
  /* Return status*/
  return e_ret_status;
} /* SECBOOT_CRYPTO_SCHEME */

/**
  * @brief Secure Engine Encrypt Append function.
  *        It is a wrapper of the Cryptolib Encrypt_Append function included in the protected area.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Encrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                        int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;

  /* Check the pointers allocation */
  if ((pInputBuffer == NULL) || (pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    return SE_ERROR;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)

  /* Common Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Encrypt_Append(pInputBuffer, InputSize, pOutputBuffer, pOutputSize);

#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  /* The bootloader does not need the encrypt service in this crypto scheme: reject this request */
  e_ret_status = SE_ERROR;

  /* Prevent unused argument(s) compilation warning */
  UNUSED(InputSize);
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM */

  /* Return status*/
  return e_ret_status;
} /* SECBOOT_CRYPTO_SCHEME */

/**
  * @brief Secure Engine Encrypt Finish function.
  *        It is a wrapper of the Cryptolib Encrypt_Finish function included in the protected area.
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Encrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;

  /* Check the pointers allocation */
  if ((pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    /* Clean-up the key in RAM */
    SE_CLEAN_UP_FW_KEY();

    return SE_ERROR;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)

  /* Common Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Encrypt_Finish(pOutputBuffer, pOutputSize);

#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  /* The bootloader does not need the encrypt service in this crypto scheme: reject this request */
  e_ret_status = SE_ERROR;
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Clean-up the key in RAM */
  SE_CLEAN_UP_FW_KEY();

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine Decrypt Init function.
  *        It is a wrapper of the Cryptolib Decrypt_Init function included in the protected area.
  * @param pxSE_Metadata: Firmware metadata.
  * @param SE_FwType: Type of Fw Image.
  *        This parameter can be SE_FW_IMAGE_COMPLETE or SE_FW_IMAGE_PARTIAL.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Decrypt_Init(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType)
{
  SE_ErrorStatus e_ret_status;

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Variables to handle partial/complete FW image */
  uint32_t fw_size;
  uint8_t *fw_tag;
  SE_GCMInitTypeDef se_gcm_init;
#elif (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  /*
   * No need for a local struct like se_gcm_init because we do not rely on the SE common crypto code (as AES CBC is not
   * available for the UserApp).
   * We call directly the Cryptolib.
   */
  int32_t cryptolib_status;
#elif  SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256
  /*
   * In this crypto scheme the Firmware is not encrypted or does not need to be decrypted.
   * The Decrypt operation is called anyhow before installing the firmware.
   * Indeed, it allows moving the Firmware image blocks in FLASH.
   * These moves are mandatory to create the appropriate mapping in FLASH
   * allowing the swap procedure to run without using the swap area at each and every move.
   *
   * See in SB_SFU project: @ref SFU_IMG_PrepareCandidateImageForInstall.
   */
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM */

  /* Check the pointers allocation */
  if (pxSE_Metadata == NULL)
  {
    return SE_ERROR;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Check the parameters value and set fw_size and fw_tag to check */
  if (SE_FwType == SE_FW_IMAGE_COMPLETE)
  {
    fw_size = pxSE_Metadata->FwSize;
    fw_tag = pxSE_Metadata->FwTag;
  }
  else if (SE_FwType == SE_FW_IMAGE_PARTIAL)
  {
    fw_size = pxSE_Metadata->PartialFwSize;
    fw_tag = pxSE_Metadata->PartialFwTag;
  }
  else
  {
    return SE_ERROR;
  }

  /* Read the Symmetric Key */
  SE_CRYPTO_AES_ReadKey(pxSE_Metadata);

  /*
   * Please note that the init below is hard-coded to consider that there is NO additional data.
   */
  se_gcm_init.HeaderSize = 0; /* no Additional authenticated data considered */
  se_gcm_init.PayloadSize = (int32_t)fw_size;
  se_gcm_init.pNonce = (uint8_t *)pxSE_Metadata->Nonce;
  se_gcm_init.NonceSize = SE_NONCE_LEN;
  se_gcm_init.pTag = (uint8_t *)fw_tag;
  se_gcm_init.TagSize = (int32_t)SE_TAG_LEN;

  /* Check the pointers allocation */
  if ((se_gcm_init.pNonce == NULL) || (se_gcm_init.pTag == NULL))
  {
    return SE_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_SE_CRYPTO_AES_GCM_NONCE_SIZE(se_gcm_init.NonceSize));
  assert_param(IS_SE_CRYPTO_AES_GCM_TAG_SIZE(se_gcm_init.TagSize));

  /* Common Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Decrypt_Init(m_aSE_FirmwareKey, &se_gcm_init);


#elif (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  /* Prevent unused argument(s) compilation warning */
  UNUSED(SE_FwType);
  /* Read the Symmetric Key */
  SE_CRYPTO_AES_ReadKey(pxSE_Metadata);

  /* Set flag field to default value */
  m_AESCBCctx.mFlags = E_SK_DEFAULT;

  /* Set key size to 16 (corresponding to AES-128) */
  m_AESCBCctx.mKeySize = (int32_t) SE_SYMKEY_LEN;

  /* Set iv size field to IvLength*/
  m_AESCBCctx.mIvSize = (int32_t) SE_IV_LEN;

  /* Initialize the operation, by passing the key and IV */
  cryptolib_status = AES_CBC_Decrypt_Init(&m_AESCBCctx, m_aSE_FirmwareKey, pxSE_Metadata->InitVector);

  /* map the return code */
  if (AES_SUCCESS == cryptolib_status)
  {
    e_ret_status = SE_SUCCESS;
  }
  else
  {
    e_ret_status = SE_ERROR;
  }

#elif  SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256
  /* Nothing to do as we won't decrypt anything */
  /* Prevent unused argument(s) compilation warning */
  UNUSED(SE_FwType);
  e_ret_status = SE_SUCCESS;
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine Decrypt Append function.
  *        It is a wrapper of the Cryptolib Decrypt_Append function included in the protected area.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Decrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                        int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  int32_t cryptolib_status;
#endif /* SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256 */

  /* DecryptImageInDwlSlot() always starts by calling the Decrypt service with a 0 byte buffer */
  if (0 == InputSize)
  {
    /* Nothing to do but we must return a success for the decrypt operation to continue */
    return (SE_SUCCESS);
  }

  /* Check the pointers allocation */
  if ((pInputBuffer == NULL) || (pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    return SE_ERROR;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Decrypt_Append(pInputBuffer, InputSize, pOutputBuffer, pOutputSize);

#elif (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  /* Crypto function call */
  cryptolib_status = AES_CBC_Decrypt_Append(&m_AESCBCctx, pInputBuffer, InputSize, pOutputBuffer, pOutputSize);

  /* map the return code */
  if (AES_SUCCESS == cryptolib_status)
  {
    e_ret_status = SE_SUCCESS;
  }
  else
  {
    e_ret_status = SE_ERROR;
  }

#elif  SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256
  /*
   * The firmware is not encrypted or does not need to be decrypted.
   * The only thing we need to do is to recopy the input buffer in the output buffer
   */
  (void)memcpy(pOutputBuffer, pInputBuffer, (uint32_t)InputSize);
  *pOutputSize = InputSize;
  e_ret_status = SE_SUCCESS;
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine Decrypt Finish function.
  *        It is a wrapper of the Cryptolib Decrypt_Finish function included in the protected area.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Decrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  int32_t cryptolib_status;
#elif (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
#endif /* SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256 */

  /* Check the pointers allocation */
  if ((pOutputBuffer == NULL) || (pOutputSize == NULL))
  {
    /* Clean-up the key in RAM */
    SE_CLEAN_UP_FW_KEY();

    return e_ret_status;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)

  /* Crypto function call*/
  e_ret_status = SE_CRYPTO_AES_GCM_Decrypt_Finish(pOutputBuffer, pOutputSize);

#elif (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)
  /* Crypto function call */
  cryptolib_status = AES_CBC_Decrypt_Finish(&m_AESCBCctx, pOutputBuffer, pOutputSize);

  /* map the return code */
  if (AES_SUCCESS == cryptolib_status)
  {
    e_ret_status = SE_SUCCESS;
  }
  else
  {
    e_ret_status = SE_ERROR;
  }
#elif  SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256
  /* Nothing to do */
  e_ret_status = SE_SUCCESS;
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Clean-up the key in RAM */
  SE_CLEAN_UP_FW_KEY();

  /* Return status*/
  return e_ret_status;
}


/**
  * @brief Secure Engine AuthenticateFW Init function.
  *        It is a wrapper of the Cryptolib function included in the protected area used to initialize the FW
  *        authentication procedure.
  * @param pKey: pointer to the key.
  * @param pxSE_Metadata: Firmware metadata.
  * @param SE_FwType: Type of Fw Image.
  *        This parameter can be SE_FW_IMAGE_COMPLETE or SE_FW_IMAGE_PARTIAL.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AuthenticateFW_Init(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType)
{
  SE_ErrorStatus e_ret_status;

  /*
   * Depending on the crypto scheme, the Firmware Tag (signature) can be:
   *   - either an AES GCM tag
   *   - or a SHA256 digest (encapsulated in the authenticated FW metadata)
   */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  e_ret_status = SE_CRYPTO_Encrypt_Init(pxSE_Metadata, SE_FwType);
#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  int32_t error_status;

  /* Prevent unused argument(s) compilation warning */
  UNUSED(pxSE_Metadata);
  UNUSED(SE_FwType);

  /* Set the size of the desired hash digest: SHA-256 */
  m_SHA256ctx.mTagSize = CRL_SHA256_SIZE;

  /* Set flag field to default value */
  m_SHA256ctx.mFlags = E_HASH_DEFAULT;

  /* Initialize the HASH context */
  error_status = SHA256_Init(&m_SHA256ctx);

  if (HASH_SUCCESS == error_status)
  {
    e_ret_status = SE_SUCCESS;
  }
  else
  {
    e_ret_status = SE_ERROR;
  }
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine AuthenticateFW Append function.
  *        It is a wrapper of the Cryptolib Append function included in the protected area used during the FW
  *        authentication procedure.
  * @param pInputBuffer: pointer to Input Buffer.
  * @param InputSize: Input Size (bytes).
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AuthenticateFW_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                               int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
  /*
   * Depending on the crypto scheme, the Firmware Tag (signature) can be:
   *   - either an AES GCM tag
   *   - or a SHA256 digest
   */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  e_ret_status = SE_CRYPTO_Encrypt_Append(pInputBuffer, InputSize, pOutputBuffer, pOutputSize);
#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  int32_t error_status;

  /* The parameters below are useless for the HASH but are needed for API compatibility with other procedures */
  (void)pOutputBuffer;
  (void)pOutputSize;

  /* Add data to be hashed */
  error_status = SHA256_Append(&m_SHA256ctx, pInputBuffer, InputSize);

  if (HASH_SUCCESS == error_status)
  {
    e_ret_status = SE_SUCCESS;
  }
  else
  {
    e_ret_status = SE_ERROR;
  }
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine AuthenticateFW Finish function.
  *        It is a wrapper of the Cryptolib Encrypt_Finish function included in the protected area used during the FW
  *        authentication procedure..
  * @param pOutputBuffer: pointer to Output Buffer.
  * @param pOutputSize: pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_AuthenticateFW_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
  /*
   * Depending on the crypto scheme, the Firmware Tag (signature) can be:
   *   - either an AES GCM tag
   *   - or a SHA256 digest
   */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  e_ret_status = SE_CRYPTO_Encrypt_Finish(pOutputBuffer, pOutputSize);
#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  int32_t error_status;

  /* Add data to be hashed */
  error_status = SHA256_Finish(&m_SHA256ctx, pOutputBuffer, pOutputSize);

  if (HASH_SUCCESS == error_status)
  {
    e_ret_status = SE_SUCCESS;
  }
  else
  {
    e_ret_status = SE_ERROR;
  }
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
}

/**
  * @brief Secure Engine Authenticate Metadata function.
  *        Authenticates the header containing the Firmware metadata.
  * @param pxSE_Metadata: Firmware metadata.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CRYPTO_Authenticate_Metadata(SE_FwRawHeaderTypeDef *pxSE_Metadata)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;

  /*
   * Module variables for crypto key handling.
   */
#if ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )

  /******************************************************************************/
  /******** Parameters for Elliptic Curve P-256 SHA-256 from FIPS 186-3**********/
  /******************************************************************************/
  static const uint8_t P_256_a[] __attribute__((aligned(4))) =
  {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
  };
  static const uint8_t P_256_b[] __attribute__((aligned(4))) =
  {
    0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd, 0x55, 0x76,
    0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53, 0xb0, 0xf6, 0x3b, 0xce,
    0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b
  };
  static const uint8_t P_256_p[] __attribute__((aligned(4))) =
  {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
  };
  static const uint8_t P_256_n[] __attribute__((aligned(4))) =
  {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3, 0xB9,
    0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
  };
  static const uint8_t P_256_Gx[] __attribute__((aligned(4))) =
  {
    0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47, 0xF8, 0xBC, 0xE6, 0xE5, 0x63,
    0xA4, 0x40, 0xF2, 0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0, 0xF4, 0xA1,
    0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96
  };
  static const uint8_t P_256_Gy[] __attribute__((aligned(4))) =
  {
    0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B, 0x8E, 0xE7, 0xEB, 0x4A, 0x7C,
    0x0F, 0x9E, 0x16, 0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE, 0xCB, 0xB6,
    0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5
  };

  /** pre-allocated memory buffer used to store the internal values required by ECCDSA computation */
  static uint8_t preallocated_buffer[2048];

#endif /* SECBOOT_CRYPTO_SCHEME */

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)

#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  /*
    * Variable used to store the Asymmetric Key inside the protected area (even if this is a public key it is stored
    * like the secret key).
    * Please make sure this variable is placed in protected SRAM1.
    */
  static uint8_t m_aSE_PubKey[SE_ASYM_PUBKEY_LEN];
#endif /* SECBOOT_CRYPTO_SCHEME */

  /*
    * Local variables for authentication procedure.
    */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  SE_GCMInitTypeDef se_gcm_init;
  uint8_t fw_raw_header_output[SE_FW_HEADER_TOT_LEN];
  int32_t fw_raw_header_output_length;

  /* the key to be used for crypto operations (as this is a pointer to m_aSE_FirmwareKey or m_aSE_PubKey it can be a
     local variable, the pointed data is protected) */
  uint8_t *pKey;
#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  int32_t status;
  ECDSAsignature_stt *sign = NULL;
  /* structure require for initialization */
  membuf_stt Crypto_Buffer;
  /* Proceed with ECC signature generation */
  EC_stt EC_st;
  /* Structure that will contain the public key, please note that the public key
    * is just a point on the curve, hence the name ECpoint_stt
    */
  ECpoint_stt *PubKey = NULL;
  /* Structure context used to call the ECDSAverify */
  ECDSAverifyCtx_stt verctx;
  const uint8_t *pSign_r;
  const uint8_t *pSign_s;
  /* Firmware metadata to be authenticated and reference MAC */
  const uint8_t *pPayload;    /* Metadata payload */
  int32_t payloadSize;        /* Metadata length to be considered for hash */
  uint8_t *pSign;             /* Reference MAC (ECCDSA signed SHA256 of the FW metadata) */
  const uint8_t *pPub_x;
  const uint8_t *pPub_y;
  /* buffer for sha256 computing */
  uint8_t MessageDigest[CRL_SHA256_SIZE];
  int32_t MessageDigestLength = 0;

  /* the key to be used for crypto operations (as this is a pointer to m_aSE_FirmwareKey or m_aSE_PubKey it can be a
     local variable, the pointed data is protected) */
  uint8_t *pKey;
#endif /* SECBOOT_CRYPTO_SCHEME */

  if (NULL == pxSE_Metadata)
  {
    return e_ret_status;
  }

#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Initialize the key */
  SE_CRYPTO_AES_ReadKey(pxSE_Metadata);
  pKey = &(m_aSE_FirmwareKey[0]);

  /*
    * Call the DECRYPT primitive to verify the HeaderSignature:
    *   1. Payload size is set to 0 because there is no encrypted data to decrypt
    *   2. Header data is processed as Additional authenticated data (authentication only, no encrypt/decrypt)
    *   3. The HeaderSignature is the tag to be verified to authenticate the header (at the FINISH stage)
    */

  se_gcm_init.HeaderSize = (int32_t)SE_FW_AUTH_LEN; /* Authenticated part of the header */
  se_gcm_init.PayloadSize = (int32_t) 0U;
  se_gcm_init.pNonce = (uint8_t *)pxSE_Metadata->Nonce;
  se_gcm_init.NonceSize = SE_NONCE_LEN;
  se_gcm_init.pTag = (uint8_t *) pxSE_Metadata->HeaderSignature;
  se_gcm_init.TagSize = SE_TAG_LEN;

  /* Check the parameters */
  assert_param(IS_SE_CRYPTO_AES_GCM_NONCE_SIZE(se_gcm_init.NonceSize));
  assert_param(IS_SE_CRYPTO_AES_GCM_TAG_SIZE(se_gcm_init.TagSize));

  /*
    * Common Crypto function calls to:
    *    initiate decrypt service
    *    provide the header data (additional authenticated data)
    *    finish the decrypt to check the header tag
    */

  /* Initialize the operation */
  e_ret_status = SE_CRYPTO_AES_GCM_Decrypt_Init(pKey, &se_gcm_init);

  /* check for initialization errors */
  if (e_ret_status == SE_SUCCESS)
  {
    /* Process header */
    e_ret_status = SE_CRYPTO_AES_GCM_Header_Append((uint8_t *)pxSE_Metadata, se_gcm_init.HeaderSize);

    /* Authenticate */
    if (e_ret_status == SE_SUCCESS)
    {
      /* Finalize data */
      e_ret_status = SE_CRYPTO_AES_GCM_Decrypt_Finish(fw_raw_header_output, &fw_raw_header_output_length);
    }
  }

#elif ( (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256) || (SECBOOT_CRYPTO_SCHEME == SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256) )
  e_ret_status = SE_ERROR;
  /* Retrieve the ECC Public Key */
  SE_CRYPTO_ReadKey_Pub(pxSE_Metadata, &m_aSE_PubKey[0U]);
  pKey = &(m_aSE_PubKey[0]);

  /* Set the local variables required to handle the Firmware Metadata during the authentication procedure */
  pPayload = (const uint8_t *)pxSE_Metadata;
  payloadSize = (int32_t) SE_FW_AUTH_LEN;                            /* Authenticated part of the header */
  pSign = pxSE_Metadata->HeaderSignature;

  /* Set the local variables dealing with the public key */
  pPub_x  =  pKey;
  pPub_y = (uint8_t *)(pKey + 32);

  /* signature to be verified with r and s components */
  pSign_r = pSign;
  pSign_s = (uint8_t *)(pSign + 32);

  /* Compute the SHA256 of the Firmware Metadata */
  status = SE_CRYPTO_SHA256_HASH_DigestCompute(pPayload,
                                               payloadSize,
                                               (uint8_t *)MessageDigest,
                                               &MessageDigestLength);

  if (status == HASH_SUCCESS)
  {
    /* We prepare the memory buffer structure */
    Crypto_Buffer.pmBuf =  preallocated_buffer;
    Crypto_Buffer.mUsed = 0;
    Crypto_Buffer.mSize = (int16_t) sizeof(preallocated_buffer);
    EC_st.pmA = P_256_a;
    EC_st.pmB = P_256_b;
    EC_st.pmP = P_256_p;
    EC_st.pmN = P_256_n;
    EC_st.pmGx = P_256_Gx;
    EC_st.pmGy = P_256_Gy;
    EC_st.mAsize = (int32_t)sizeof(P_256_a);
    EC_st.mBsize = (int32_t)sizeof(P_256_b);
    EC_st.mNsize = (int32_t)sizeof(P_256_n);
    EC_st.mPsize = (int32_t)sizeof(P_256_p);
    EC_st.mGxsize = (int32_t)sizeof(P_256_Gx);
    EC_st.mGysize = (int32_t)sizeof(P_256_Gy);

    status = ECCinitEC(&EC_st, &Crypto_Buffer);

    if (status == ECC_SUCCESS)
    {
      status = ECCinitPoint(&PubKey, &EC_st, &Crypto_Buffer);
    }

    if (status == ECC_SUCCESS)
    {
      /* Point is initialized, now import the public key */
      (void)ECCsetPointCoordinate(PubKey, E_ECC_POINT_COORDINATE_X, pPub_x, 32);
      (void)ECCsetPointCoordinate(PubKey, E_ECC_POINT_COORDINATE_Y, pPub_y, 32);
      /* Try to validate the Public Key. */
      status = ECCvalidatePubKey(PubKey, &EC_st, &Crypto_Buffer);
    }

    if (status == ECC_SUCCESS)
    {
      /* Public Key is validated, Initialize the signature object */
      status = ECDSAinitSign(&sign, &EC_st, &Crypto_Buffer);
    }

    if (status == ECC_SUCCESS)
    {
      /* Import the signature values */
      (void)ECDSAsetSignature(sign, E_ECDSA_SIGNATURE_R_VALUE, pSign_r, 32);
      (void)ECDSAsetSignature(sign, E_ECDSA_SIGNATURE_S_VALUE, pSign_s, 32);

      /* Prepare the structure for the ECDSA signature verification */
      verctx.pmEC = &EC_st;
      verctx.pmPubKey = PubKey;

      /* Verify it */
      status = ECDSAverify(MessageDigest, MessageDigestLength, sign, &verctx, &Crypto_Buffer);
      if (status == SIGNATURE_VALID)
      {
        e_ret_status = SE_SUCCESS;
      }
      /* release resource ...*/
      (void)ECDSAfreeSign(&sign, &Crypto_Buffer);
      (void)ECCfreePoint(&PubKey, &Crypto_Buffer);
      (void)ECCfreeEC(&EC_st, &Crypto_Buffer);
    }
  }
#else
#error "The current example does not support the selected crypto scheme."
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Clean-up the key in RAM */
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM)
  /* Symmetric key */
  SE_CLEAN_UP_FW_KEY();
#else
  /* ECC public key */
  SE_CLEAN_UP_PUB_KEY();
#endif /* SECBOOT_CRYPTO_SCHEME */

  /* Return status*/
  return e_ret_status;
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

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
