/**
  ******************************************************************************
  * @file    sfu_scheme_x509_config.h
  * @author  MCD Application Team
  * @brief   This file is used to configure the crypto scheme used
  *          by the sbsfu firmware.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SFU_SCHEME_X509_CONFIG_H
#define SFU_SCHEME_X509_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
/*  Define SBSFU_X509_USE_PKCS11DIGEST to use PKCS11 for digest computation,
    otherwise MBEDTLS is used */
#define SBSFU_X509_USE_PKCS11DIGEST

/* Source of the lower certificates in the chain (root and CA)
 * Use certificates embedded in sbsfu firmware - may be useful during development
 * or use certificates located in secure storage using PKCS11
 */
#define SBSFU_X509_LOWER_CERTS_SOURCE SBSFU_X509_USE_PKCS11_CERTS
#define SBSFU_X509_USE_FW_EMBEDDED_CERTS  (1U)  /* certs embedded in sbsfu firmware */
#define SBSFU_X509_USE_PKCS11_CERTS       (2U)  /* certs obtained using PKCS11 */

/* Onboard certs (certs not delivered with the firmware package) encoding */
#define SBSFU_X509_ONBOARD_CERTS_ENCODING SBSFU_X509_CERTS_DER_ENCODED
/* Firmware delivered certs (certs embedded in the firmware package) encoding */
#define SBSFU_X509_FW_CERTS_ENCODING      SBSFU_X509_CERTS_DER_ENCODED
#define SBSFU_X509_CERTS_PEM_ENCODED (1U)
#define SBSFU_X509_CERTS_DER_ENCODED (2U)

/* Exported macros -----------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* SFU_SCHEME_X509_CONFIG_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
