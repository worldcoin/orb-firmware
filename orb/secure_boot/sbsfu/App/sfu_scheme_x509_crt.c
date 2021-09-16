/**
  ******************************************************************************
  * @file    sfu_scheme_x509_crt.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions to handle x509 certificates
  *          for the x509 ecdsa crypto scheme.
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

/* Includes ------------------------------------------------------------------*/
#include "sfu_scheme_x509_config.h"
#include "sfu_scheme_x509_crt.h"
#include "sfu_trace.h"
#include "mbedtls/platform.h"
#include "se_interface_kms.h"
#include "string.h"
#include "sfu_trace.h"

/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
/* These are pointers to the RootCA and 1st OEM Intermeduate CA certificates */
/* They should be populated by getting certs via PKCS11 */
uint8_t *p_CertChain_OEM = NULL;
uint8_t *p_CertChain_RootCA = NULL;

/* Private function prototypes -----------------------------------------------*/
static void SFU_SCHEME_X509_CRT_PrintCerts_Der(uint8_t *p_Cert);
static void SFU_SCHEME_X509_CRT_PrintCerts_Pem(char *p_Cert);

/* Functions Definition ------------------------------------------------------*/

/**
  * @brief  Returns the size of a DER encoded x509 certificate.
  * @param  p_Cert    Pointer to the der encoded certificate whose size is to
  *                  be determined
  * @retval int32_t  The size (in bytes) of the certificate or 0 if error
  */
int32_t SFU_SCHEME_X509_CRT_SizeOfDer(uint8_t *p_Cert)
{
  int32_t CertSize = 0;
  switch (p_Cert[1])
  {
    case 0x81:
      CertSize = p_Cert[2] + 3;
      break;

    case 0x82:
      CertSize = (((uint16_t) p_Cert[2]) << 8) + p_Cert[3] + 4;
      break;

    default:
      if (p_Cert[1] < 0x81)
      {
        CertSize = p_Cert[1];
      }
      break;
  }
  return CertSize;
}

/**
  * @brief  Print out a DER encoded x509 certificate.
  * @param  p_Cert   Pointer to the der encoded certificate.
  * @retval void    This function returns nothing
  */
static void SFU_SCHEME_X509_CRT_PrintCerts_Der(uint8_t *p_Cert)
{
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  int32_t CertSize = 0;
  if (NULL == p_Cert)
  {
    TRACE("Error - no certs to print\n\r");
  }
  else
  {
    CertSize = SFU_SCHEME_X509_CRT_SizeOfDer(p_Cert);
    for (int32_t i = 0; i < CertSize; i++)
    {
      TRACE("%02x", p_Cert[i]);
      if (!((i + 1) % 32))
      {
        TRACE("\n\r");
      }
    }
    TRACE("\n\n\r");
  }
#else
  UNUSED(p_Cert);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
}

/**
  * @brief  Print out a PEM encoded x509 certificate.
  * @param  p_Cert   Pointer to the pem encoded certificate.
  * @retval void    This function returns nothing
  */
static void SFU_SCHEME_X509_CRT_PrintCerts_Pem(char *p_Cert)
{
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  int32_t CertSize = 0;
  if (NULL == p_Cert)
  {
    TRACE("Error - no certs to print\n\r");
  }
  else
  {
    CertSize = strlen(p_Cert);
    for (int32_t i = 0; i < CertSize; i++)
    {
      TRACE("%c", p_Cert[i]);
    }
    TRACE("\n\r");
  }
#else
  UNUSED(p_Cert);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
}

/**
  * @brief  Print out a PEM or DER encoded x509 certificate.
  * @param  p_Cert   Pointer to the pem or der encoded certificate.
  * @retval void    This function returns nothing
  */
void SFU_SCHEME_X509_CRT_PrintCerts(uint8_t *p_Cert)
{
  if (NULL == p_Cert)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("Error - no certs to print\n\r");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
  }
  else
  {
    if (p_Cert[0] == '-')
    {
      SFU_SCHEME_X509_CRT_PrintCerts_Pem((char *) p_Cert);
    }
    else
    {
      SFU_SCHEME_X509_CRT_PrintCerts_Der(p_Cert);
    }
  }
}

/**
  * @brief  Verify the validity of an x509 certificate chain and return an
  *         mbedtls_x509_crt structure for the certificate chain.
  * @param  p_LeafIntermediateCertChain    Pointer to a chain comprising of the
  *         leaf certificate and any intermediate CA certificates (PEM encoded).
  * @param  p_LowerIntermediateCert   Pointer to a DER encoded
  *         intermediate CA certificate (or NULL if none)
  * @param  p_RootCACert    Pointer to a DER encoded Root CA certificate
  * @param  p_MbedCertChain Pointer to an mbedtls_x509_crt structure which will
  *         hold the verified certificate chain
  * @retval SFU_SUCCESS if certificate chain is verified successfully or
  *         SFU_ERROR if there is an error or the verification fails
  */
SFU_ErrorStatus SFU_SCHEME_X509_CRT_VerifyCert(unsigned char *p_LeafIntermediateCertChain, \
                                               unsigned char *p_LowerIntermediateCert, \
                                               unsigned char *p_RootCACert, mbedtls_x509_crt *p_MbedCertChain)
{
  int32_t Size_Leaf_Intermediate_Cert_Chain = 0; /* Leaf + Upper intermediate cert */
  int32_t Size_Lower_Intermediate_Cert_Chain = 0; /* Lower intermediate cert */
  int32_t Size_RootCA_Cert = 0; /* Root CA Cert */
  uint32_t crt_verif_flags = 0;
  int32_t ret_local = 0;
  mbedtls_x509_crt MbedCertChain_RootCA;

#if (SBSFU_X509_FW_CERTS_ENCODING == SBSFU_X509_CERTS_DER_ENCODED)
  uint8_t *p_IntermediateCert = (uint8_t *) NULL;
#endif /* SBSFU_X509_FW_CERTS_ENCODING */

  UNUSED(Size_Lower_Intermediate_Cert_Chain);

#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Leaf and Intermediate Cert Chain location: 0x%08x", p_LeafIntermediateCertChain);
  TRACE("\n\r= [SBOOT] Lower Intermediate Cert location: 0x%08x", p_LowerIntermediateCert);
  TRACE("\n\r= [SBOOT] RootCA Cert location: 0x%08x", p_RootCACert);
#endif /* SFU_VERBOSE_DEBUG_MODE */

  if ((NULL == p_LeafIntermediateCertChain) || (NULL == p_RootCACert))
  {
    return SFU_ERROR; /* Leaf and Root CA must not be NULL */
  }
#if (SBSFU_X509_FW_CERTS_ENCODING == SBSFU_X509_CERTS_PEM_ENCODED)
  if (memcmp(p_LeafIntermediateCertChain, CERT_BEGIN, strlen(CERT_BEGIN)))
  {
    TRACE("\n\r= [SBOOT] Error - no leaf and intermediate certs.\n\r");
    return SFU_ERROR; /* Leaf and intermediate certs are pem encoded, check if they are there */
  }
#endif /* SBSFU_X509_FW_CERTS_ENCODING */

#if (SBSFU_X509_FW_CERTS_ENCODING == SBSFU_X509_CERTS_PEM_ENCODED)
  Size_Leaf_Intermediate_Cert_Chain = strlen((char *) p_LeafIntermediateCertChain);
#else
  {
    Size_Leaf_Intermediate_Cert_Chain = SFU_SCHEME_X509_CRT_SizeOfDer(p_LeafIntermediateCertChain);
    p_IntermediateCert = (uint8_t *)((uint32_t) p_LeafIntermediateCertChain + Size_Leaf_Intermediate_Cert_Chain);
    Size_Leaf_Intermediate_Cert_Chain += SFU_SCHEME_X509_CRT_SizeOfDer(p_IntermediateCert);
  }
#endif /* SBSFU_X509_FW_CERTS_ENCODING */

  if (NULL != p_LowerIntermediateCert)
#if (SBSFU_X509_ONBOARD_CERTS_ENCODING == SBSFU_X509_CERTS_DER_ENCODED)
    Size_Lower_Intermediate_Cert_Chain = SFU_SCHEME_X509_CRT_SizeOfDer(p_LowerIntermediateCert);
  Size_RootCA_Cert = SFU_SCHEME_X509_CRT_SizeOfDer(p_RootCACert);
#else
    Size_Lower_Intermediate_Cert_Chain = strlen((char *) p_LowerIntermediateCert);
  Size_RootCA_Cert = strlen((char *) p_RootCACert);
#endif /* SBSFU_X509_ONBOARD_CERTS_ENCODING */

  if ((Size_Leaf_Intermediate_Cert_Chain <= 0) || (Size_RootCA_Cert <= 0))
  {
    return SFU_ERROR; /* Size must not be 0 */
  }

#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Size of Leaf and Intermediate cert chain: %d", Size_Leaf_Intermediate_Cert_Chain);
  TRACE("\n\r= [SBOOT] Size of Lower Intermediate cert chain: %d", Size_Lower_Intermediate_Cert_Chain);
  TRACE("\n\r= [SBOOT] Size of RootCA Cert: %d", Size_RootCA_Cert);

  /*
   * Display certs in console
   */
  TRACE("\n\r= [SBOOT] Leaf and Intermediate Certs:\n\r");
  SFU_SCHEME_X509_CRT_PrintCerts(p_LeafIntermediateCertChain);
  if (NULL != p_LowerIntermediateCert)
  {
    TRACE("\n\r= [SBOOT] Lower Intermediate cert chain:\n\r");
    SFU_SCHEME_X509_CRT_PrintCerts(p_LowerIntermediateCert);
  }
  TRACE("\n\r= [SBOOT] RootCA Cert:\n\r");
  SFU_SCHEME_X509_CRT_PrintCerts(p_RootCACert);
#endif /* SFU_VERBOSE_DEBUG_MODE */

  mbedtls_x509_crt_init(p_MbedCertChain); /* init cert chain */
  mbedtls_x509_crt_init(&MbedCertChain_RootCA); /* Prepare root cert */

  /*
   * parse cert chain
   */
#if (SBSFU_X509_FW_CERTS_ENCODING == SBSFU_X509_CERTS_PEM_ENCODED)
  ret_local = mbedtls_x509_crt_parse(p_MbedCertChain, p_LeafIntermediateCertChain, \
                                     Size_Leaf_Intermediate_Cert_Chain + 1);
#else
  ret_local = mbedtls_x509_crt_parse_der(p_MbedCertChain, p_LeafIntermediateCertChain, \
                                         SFU_SCHEME_X509_CRT_SizeOfDer(p_LeafIntermediateCertChain));
  if (0 == ret_local) /* check parsing was successful */
  {
    ret_local = mbedtls_x509_crt_parse_der(p_MbedCertChain, p_IntermediateCert, \
                                           SFU_SCHEME_X509_CRT_SizeOfDer(p_IntermediateCert));
  }
#endif /* SBSFU_X509_FW_CERTS_ENCODING */
  if (0 == ret_local) /* check parsing was successful */
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Parsing Leaf and Intermediate certs... OK");
#endif /* SFU_VERBOSE_DEBUG_MODE */
  }
  else
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Parsing Leaf and Intermediate certs FAILED with error %d\n\r", ret_local);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* Parsing Leaf and Intermediate certs FAILED, can't proceed */
  }

  if (NULL != p_LowerIntermediateCert)
  {
    /*
    * parse cert chain
    */
#if (SBSFU_X509_ONBOARD_CERTS_ENCODING == SBSFU_X509_CERTS_DER_ENCODED)
    ret_local = mbedtls_x509_crt_parse_der(p_MbedCertChain, p_LowerIntermediateCert, \
                                           SFU_SCHEME_X509_CRT_SizeOfDer(p_LowerIntermediateCert)); /* DER encoded */
#else
    ret_local = mbedtls_x509_crt_parse(p_MbedCertChain, p_LowerIntermediateCert, \
                                       Size_Lower_Intermediate_Cert_Chain + 1); /* PEM Encoded */
#endif /* SBSFU_X509_ONBOARD_CERTS_ENCODING */
    if (0 == ret_local)
    {
#if defined(SFU_VERBOSE_DEBUG_MODE)
      TRACE("\n\r= [SBOOT] Parsing Lower Intermediate certs OK");
#endif /* SFU_VERBOSE_DEBUG_MODE */
    }
    else
    {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
      TRACE("\n\r= [SBOOT] Parsing Lower Intermediate certs FAILED with error %d\n\r", ret_local);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
      return SFU_ERROR;
    }
  }

#if (SBSFU_X509_ONBOARD_CERTS_ENCODING == SBSFU_X509_CERTS_DER_ENCODED)
  ret_local = mbedtls_x509_crt_parse_der(&MbedCertChain_RootCA, p_RootCACert, \
                                         SFU_SCHEME_X509_CRT_SizeOfDer(p_RootCACert));
#else
  ret_local = mbedtls_x509_crt_parse(&MbedCertChain_RootCA, p_RootCACert, Size_RootCA_Cert + 1);
#endif /* SBSFU_X509_ONBOARD_CERTS_ENCODING */
  if (0 == ret_local)
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Parsing Root CA cert OK");
#endif /* SFU_VERBOSE_DEBUG_MODE */
  }
  else
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Parsing Root CA cert FAILED with error %d\n\r", ret_local);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR;
  }

#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Verifying the Certificate chain... ");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
  ret_local = mbedtls_x509_crt_verify(p_MbedCertChain, &MbedCertChain_RootCA, \
                                      NULL, NULL, &crt_verif_flags, NULL, NULL);
  if (0 == ret_local)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("OK");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_SUCCESS;
  }
  else
  {
    char Error[1024] = {'\0'};
    mbedtls_x509_crt_verify_info(Error, sizeof(Error), NULL, crt_verif_flags);
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] FAILED with error %d : %s", ret_local, Error);
    TRACE("\n\r= [SBOOT] Error Flags: 0x%08x\n\r", crt_verif_flags);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR;
  }
}


/**
  * @brief  Open a PKCS11 session.
  * @param  p_xP11Session   pointer to a CK_SESSION_HANDLE.
  * @note   The session handle will be populated if this function successfully
  *         opens a PKCS11 session.
  * @param  p_xP11FunctionList   pointer to CK_FUNCTION_LIST_PTR.
  * @note   The function list pointer will be populated if this function
  *         successfully opens a PKCS11 session.
  * @retval SFU_SUCCESS if the PKCS11 Session is successfully opened or
  *         SFU_ERROR if the session open fails.
  */
SFU_ErrorStatus SFU_SCHEME_X509_CRT_SEOpenSession(CK_SESSION_HANDLE *p_xP11Session,
                                                  CK_FUNCTION_LIST_PTR *p_xP11FunctionList)
{
  uint32_t xResult = 0;

  *p_xP11Session = (CK_SESSION_HANDLE) NULL;

  xResult = C_Initialize(NULL);
  if (0 != xResult)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] ERROR CALLING C_Initialize: %d", xResult);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR;
  }

  xResult = C_OpenSession(0, CKF_SERIAL_SESSION, NULL, NULL, p_xP11Session);
  if (0 != xResult)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] ERROR CALLING C_OpenSession: %d", xResult);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR;
  }
  else
  {
    return SFU_SUCCESS; /* Session opened OK */
  }
}

/**
  * @brief  Close a PKCS11 session.
  * @param  p_xP11Session   pointer to a CK_SESSION_HANDLE.
  * @note   The session handle will be set to NULL if this function successfully
  *         closes the PKCS11 session.
  * @param  p_xP11FunctionList  pointer to CK_FUNCTION_LIST_PTR.
  * @note   The function list pointer will be set to NULL if this function
  *         successfully closes the PKCS11 session.
  * @retval SFU_SUCCESS if the PKCS11 Session is successfully closed or
  *         SFU_ERROR if the session close fails.
  */
SFU_ErrorStatus SFU_SCHEME_X509_CRT_SECloseSession(CK_SESSION_HANDLE *p_xP11Session,
                                                   CK_FUNCTION_LIST_PTR *p_xP11FunctionList)
{
  uint32_t xResult = 0;
  xResult = C_CloseSession(*p_xP11Session); /* Close the session */
  if (0 != xResult)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] ERROR CALLING C_CloseSession: %d", xResult);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* Failed to close the session */
  }

  xResult = C_Finalize(NULL);
  if (0 != xResult)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] ERROR CALLING C_Finalize: %d", xResult);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* Finalize failed */
  }


#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  TRACE("OK");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
  *p_xP11Session = (CK_SESSION_HANDLE) NULL;
  *p_xP11FunctionList = (CK_FUNCTION_LIST_PTR) NULL;
  return SFU_SUCCESS;
}

/**
  * @brief  Retrieve a DER encoded certificate from the Secure Enclave.
  * @param  p_xP11Session       A CK_SESSION_HANDLE to a previously opened PKCS11
  *                           session.
  * @param  p_xP11FunctionList A CK_FUNCTION_LIST_PTR obtained when the PKCS11
  *                           session is opened.
  * @param  p_Label            Pointer to the label used to identify the
  *                           requested certificate
  * @param  p_p_Cert              pointer to a pointer to the buffer which will
  *                           contain the certificate material
  * @note                     The buffer will be allocated and populated
  *                           by this function if successful.
  * @retval SFU_ErrorStatus  SFU_SUCCESS if the certificate is successfully
  *                          obtained or SFU_ERROR if not
  */
SFU_ErrorStatus SFU_SCHEME_X509_CRT_GetSECert(CK_SESSION_HANDLE p_xP11Session, CK_FUNCTION_LIST_PTR p_xP11FunctionList,
                                              uint8_t *p_Label, uint8_t **p_p_Cert)
{
  uint32_t ulCount;
  CK_OBJECT_HANDLE hObject;
  CK_RV rv;
  CK_ATTRIBUTE Template;
  CK_ATTRIBUTE TemplateRead[4];
  CK_OBJECT_CLASS certificate_class_value;
  CK_CERTIFICATE_TYPE certificate_type_value;
  CK_CERTIFICATE_CATEGORY certificate_category_value;

  /*
   * Fill in the template with attribute to find
   * In this case we're looking for a certificate
   * Identified with a label pointed to be p_Label
   */
  Template.type       = CKA_LABEL;
  Template.pValue     = (CK_VOID_PTR) p_Label;
  Template.ulValueLen = (CK_ULONG) strlen((const char *)p_Label);

  rv = C_FindObjectsInit(p_xP11Session, &Template, 1U);

  if (rv != CKR_OK)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\rC_FindObjectsInit FAILED");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* Can't find cert, can't continue */
  }


  rv = C_FindObjects(p_xP11Session,  &hObject, 1, (CK_ULONG *) &ulCount);
  if (rv != CKR_OK)
  {
    return SFU_ERROR; /* FindObjects failed can't continue */
  }
  rv = C_FindObjectsFinal(p_xP11Session);

  if ((rv != CKR_OK) || (ulCount == 0))
  {
    return SFU_ERROR; /* FindObjectsFinal failed, can't continue  */
  }

  /* allocate buffer for certificate */
  *p_p_Cert = calloc(SB_CERT_MAX_SIZE, sizeof(CK_BYTE));
  if (NULL == p_p_Cert)
  {
    return SFU_ERROR; /* calloc failed */
  }

  /*
   * Fill in the template with attribute to retrieve
   * We're fetching the certificate data
   */

  TemplateRead[0].type       = CKA_CLASS;
  TemplateRead[0].pValue     = &certificate_class_value;
  TemplateRead[0].ulValueLen = sizeof(CK_OBJECT_CLASS);

  TemplateRead[1].type       = CKA_CERTIFICATE_TYPE;
  TemplateRead[1].pValue     = &certificate_type_value;
  TemplateRead[1].ulValueLen = sizeof(CK_CERTIFICATE_TYPE);

  TemplateRead[2].type       = CKA_CERTIFICATE_CATEGORY;
  TemplateRead[2].pValue     = &certificate_category_value;
  TemplateRead[2].ulValueLen = sizeof(CK_CERTIFICATE_CATEGORY);

  TemplateRead[3].type       = CKA_VALUE;
  TemplateRead[3].pValue     = *p_p_Cert;
  TemplateRead[3].ulValueLen = SB_CERT_MAX_SIZE;

  /* GetAttributeValue for the object found at previous step */
  rv = C_GetAttributeValue(p_xP11Session,  hObject, TemplateRead, sizeof(TemplateRead) / sizeof(TemplateRead[0]));
  if (rv == CKR_OK)
  {
    return SFU_SUCCESS; /* Got the cert */
  }
  else
  {
    return SFU_ERROR; /* Failed to read the certificate */
  }

}

#endif /* SCHEME_X509_ECDSA_WITHOUT_ENCRYPT_SHA256 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
