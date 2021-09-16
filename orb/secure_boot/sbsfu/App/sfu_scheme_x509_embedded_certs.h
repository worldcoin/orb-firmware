/**
  ******************************************************************************
  * @file    sfu_scheme_x509_embedded_certs.h
  * @author  MCD Application Team
  * @brief   This file provides firmware embedded certificates for the x509
  *          scheme in case a secure element or kms is not available.
  *          SHOULD BE USED ONLY FOR DEVELOPMENT
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
#ifndef SFU_SCHEME_X509_EMBEDDED_CERTS_H
#define SFU_SCHEME_X509_EMBEDDED_CERTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "sfu_scheme_x509_config.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#if (SBSFU_X509_LOWER_CERTS_SOURCE == SBSFU_X509_USE_FW_EMBEDDED_CERTS)

uint8_t a_RootCACert[] = "\
-----BEGIN CERTIFICATE-----\r\n\
MIICBzCCAaygAwIBAgIJAJNPHCj07QH9MAoGCCqGSM49BAMCMFYxCzAJBgNVBAYT\r\n\
AlVTMR8wHQYDVQQKDBZTVE1pY3JvZWxlY3Ryb25pY3MgSW5jMSYwJAYDVQQDDB1T\r\n\
VE0tUE9DLVNCU0ZVLVJPT1QtVEVTVC1DQS0wMDAeFw0xODExMDIxNzE0MDdaFw0y\r\n\
MTA3MjkxNzE0MDdaMFYxCzAJBgNVBAYTAlVTMR8wHQYDVQQKDBZTVE1pY3JvZWxl\r\n\
Y3Ryb25pY3MgSW5jMSYwJAYDVQQDDB1TVE0tUE9DLVNCU0ZVLVJPT1QtVEVTVC1D\r\n\
QS0wMDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABO63eIPvswyDw532UuN+0SIX\r\n\
zwY6M/4m4M0nhZ0FqMuKiugHULvKIib6BcZoh1C20FS/tp6Jm/3dIOf+sdDvu9+j\r\n\
YzBhMB0GA1UdDgQWBBSDkV8NxYdCPT63l2TLdqrZiH3gAjAfBgNVHSMEGDAWgBSD\r\n\
kV8NxYdCPT63l2TLdqrZiH3gAjAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQE\r\n\
AwIBhjAKBggqhkjOPQQDAgNJADBGAiEAj8zxB3giL9yrZ9k/0OhhX0rqfH+SWdC4\r\n\
w+TZj/6S9p4CIQCWljFLFSAQPvT6kDt1cgiPnNtJoWDxZzlBX3Zqn5u4OA==\r\n\
-----END CERTIFICATE-----\r\n";

uint8_t a_OEMCACert[] = "\
-----BEGIN CERTIFICATE-----\r\n\
MIIB9jCCAZugAwIBAgICEAQwCgYIKoZIzj0EAwIwVjELMAkGA1UEBhMCVVMxHzAd\r\n\
BgNVBAoMFlNUTWljcm9lbGVjdHJvbmljcyBJbmMxJjAkBgNVBAMMHVNUTS1QT0Mt\r\n\
U0JTRlUtUk9PVC1URVNULUNBLTAwMB4XDTE4MTEwMjE3MTQzNVoXDTE5MTExMjE3\r\n\
MTQzNVowSTELMAkGA1UEBhMCVVMxEzARBgNVBAoMCk9FTS0wMCBJbmMxJTAjBgNV\r\n\
BAMMHFNUTS1QT0MtU0JTRlUtT0VNLVRFU1QtQ0EtMDAwWTATBgcqhkjOPQIBBggq\r\n\
hkjOPQMBBwNCAAR86s6FWychNP6ksok4BA6ljGhvqhdZp51n0tFiBNGQPMrUNDZE\r\n\
i1UmtL6samnZNL3CK4DFunyuUq29Dc9VVGPDo2YwZDAdBgNVHQ4EFgQU7utiXSF5\r\n\
l4Yy2uvuuJx8lZJXL6swHwYDVR0jBBgwFoAUg5FfDcWHQj0+t5dky3aq2Yh94AIw\r\n\
EgYDVR0TAQH/BAgwBgEB/wIBATAOBgNVHQ8BAf8EBAMCAYYwCgYIKoZIzj0EAwID\r\n\
SQAwRgIhAIF5fP88HNoTDc6/NMLecv04j8gix4Wgmr6DN2FMwl9+AiEAx9m57pEj\r\n\
IyJABlYnZ02n0SloVRKO1/gy6jmp9DjUH8s=\r\n\
-----END CERTIFICATE-----\r\n";

#endif /* SBSFU_X509_LOWER_CERTS_SOURCE */

/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* SFU_SCHEME_X509_EMBEDDED_CERTS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
