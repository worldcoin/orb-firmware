/**
  ******************************************************************************
  * @file    sfu_scheme_x509_core.h
  * @author  MCD Application Team
  * @brief   Header for sfu_scheme_x509_core.c module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */


/* Only include if using SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256 scheme */
#include "se_crypto_config.h"
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SFU_SCHEME_X509_CORE_H
#define SFU_SCHEME_X509_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"
#include "sfu_def.h"
#include "sfu_trace.h"

/* Exported types ------------------------------------------------------------*/
typedef SE_FwRawHeaderTypeDef SB_FWHeaderTypeDef ;

/* Exported constants --------------------------------------------------------*/
#define SB_HASH_LENGTH ((int32_t) 32)  /*!< SHA-256 hash of the FW */

/* External variables --------------------------------------------------------*/
extern uint8_t *p_CertChain_RootCA;
extern uint8_t *p_CertChain_OEM;

/* Exported macros -----------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */
SFU_ErrorStatus SFU_SCHEME_X509_CORE_Init(SE_FwRawHeaderTypeDef *pxFwRawHeader);
SFU_ErrorStatus SFU_SCHEME_X509_CORE_VerifyFWHeader(SB_FWHeaderTypeDef *p_FWHeader, uint8_t *p_CertChain_OEM,
                                                    uint8_t *p_CertChain_RootCA);

#ifdef __cplusplus
}
#endif

#endif /* SFU_SCHEME_X509_CORE_H */
#endif /* SECBOOT_CRYPTO_SCHEME */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
