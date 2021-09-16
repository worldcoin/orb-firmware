/**
  ******************************************************************************
  * @file    sfu_scheme_x509_crt.h
  * @author  MCD Application Team
  * @brief   Header for sfu_scheme_x509_crt.c module
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


#include "se_crypto_config.h"
#if (SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SFU_SCHEME_X509_CRT_H
#define SFU_SCHEME_X509_CRT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "sfu_def.h"
#include "tkms.h"
#include "se_interface_kms.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define SE_CERT_TEMPLATE_COUNT  2
#define SB_CERT_MAX_SIZE 600 /* Max certificate size in bytes */
#define CERT_BEGIN "-----BEGIN CERTIFICATE-----\r\n"
#define CERT_END "-----END CERTIFICATE-----\r\n"


/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
int32_t SFU_SCHEME_X509_CRT_SizeOfDer(uint8_t *p_Cert);
void SFU_SCHEME_X509_CRT_PrintCerts(uint8_t *p_Cert);
SFU_ErrorStatus SFU_SCHEME_X509_CRT_SEOpenSession(CK_SESSION_HANDLE *p_xP11Session,
                                                  CK_FUNCTION_LIST_PTR *p_xP11FunctionList);
SFU_ErrorStatus SFU_SCHEME_X509_CRT_SECloseSession(CK_SESSION_HANDLE *p_xP11Session,
                                                   CK_FUNCTION_LIST_PTR *p_xP11FunctionList);
SFU_ErrorStatus SFU_SCHEME_X509_CRT_GetSECert(CK_SESSION_HANDLE p_xP11Session, CK_FUNCTION_LIST_PTR p_xP11FunctionList,
                                              uint8_t *p_Label, uint8_t **p_p_Cert);
SFU_ErrorStatus SFU_SCHEME_X509_CRT_VerifyCert(unsigned char *p_LeafIntermediateCertChain, \
                                               unsigned char *p_LowerIntermediateCert, \
                                               unsigned char *p_RootCACert, mbedtls_x509_crt *p_MbedCertChain);

#ifdef __cplusplus
}
#endif

#endif /* SFU_SCHEME_X509_CRT_H */
#endif /* SECBOOT_CRYPTO_SCHEME */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
