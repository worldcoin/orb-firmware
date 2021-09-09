/**
  ******************************************************************************
  * @file    se_crypto_bootloader.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Engine CRYPTO module
  *          functionalities. These services are used by the bootloader.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SE_CRYPTO_BOOTLOADER_H
#define SE_CRYPTO_BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "se_crypto_config.h"
#include "crypto.h"
#include "se_def.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @addtogroup SE_CRYPTO SE Crypto
  * @{
  */

/** @addtogroup SE_CRYPTO_BOOTLOADER SE Crypto for Bootloader
  * @{
  */

/** @addtogroup SE_CRYPTO_BOOTLOADER_Exported_Functions
  * @{
  */

/*Low level functions*/
SE_ErrorStatus SE_CRYPTO_Encrypt_Init(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType);
SE_ErrorStatus SE_CRYPTO_Header_Append(const uint8_t *pInputBuffer, int32_t InputSize);
SE_ErrorStatus SE_CRYPTO_Encrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                        int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_Encrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_Decrypt_Init(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType);
SE_ErrorStatus SE_CRYPTO_Decrypt_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                        int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_Decrypt_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_AuthenticateFW_Init(SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType);
SE_ErrorStatus SE_CRYPTO_AuthenticateFW_Append(const uint8_t *pInputBuffer, int32_t InputSize, uint8_t *pOutputBuffer,
                                               int32_t *pOutputSize);
SE_ErrorStatus SE_CRYPTO_AuthenticateFW_Finish(uint8_t *pOutputBuffer, int32_t *pOutputSize);
void SE_CRYPTO_Lock_CKS_Keys(void);

/* High level function(s) */
SE_ErrorStatus SE_CRYPTO_Authenticate_Metadata(SE_FwRawHeaderTypeDef *pxSE_Metadata);


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

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* SE_CRYPTO_BOOTLOADER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

