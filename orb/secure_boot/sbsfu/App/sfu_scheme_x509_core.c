/**
  ******************************************************************************
  * @file    sfu_scheme_x509_core.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions to implement the crypto
  * scheme secure boot header and firmware image state.
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
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform.h"
#include "mbedtls/asn1write.h"

#include "sfu_scheme_x509_core.h"
#include "sfu_trace.h"
#include "sfu_scheme_x509_config.h"
#include "sfu_scheme_x509_crt.h"
#include "sfu_scheme_x509_embedded_certs.h"
#include "se_def.h"
#include "se_def_metadata.h"
#include "string.h"
#include "kms_platf_objects_interface.h"
#include "sfu_fwimg_internal.h"

/* Private defines -----------------------------------------------------------*/
#define HDR_VERIFICATION_RECORDS_NUM 2
#define HDR_VERIFICATION_RECORDS_HASHSIZE 32

/* Private typedef -----------------------------------------------------------*/
static struct
{
  uint8_t Hash[HDR_VERIFICATION_RECORDS_NUM][HDR_VERIFICATION_RECORDS_HASHSIZE];
  uint8_t SlotsUsed;
} SB_HdrVerifiedRecord;


/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void   SB_HdrVerifiedInit(void);
static int8_t SB_HdrVerifiedCheck(uint8_t *p_Hash);
static void   SB_HdrVerifiedSet(uint8_t *p_Hash);

#if defined(SFU_VERBOSE_DEBUG_MODE)
static void   SFU_SCHEME_X509_CORE_PrintHeader(SB_FWHeaderTypeDef *p_FWHeader);
#endif /* SFU_VERBOSE_DEBUG_MODE */

/* Functions Definition ------------------------------------------------------*/

#if defined(SFU_VERBOSE_DEBUG_MODE)
/**
  * @brief  Print header to console
  * @param  p_FWHeader     Pointer to header data structure
  * @retval void This function does not return anything
  */
static void SFU_SCHEME_X509_CORE_PrintHeader(SB_FWHeaderTypeDef *p_FWHeader)
{
  TRACE("\n\rFW Header (0x%08x)\n\r", p_FWHeader);
  TRACE("SBMagic         : 0x%08x\n\r", p_FWHeader->SFUMagic);
  TRACE("Protocol Version: 0x%04x\n\r", p_FWHeader->ProtocolVersion);
  TRACE("FW Version      : 0x%08x\n\r", p_FWHeader->FwVersion);
  TRACE("FW Size         : 0x%08x\n\r", p_FWHeader->FwSize);
  TRACE("FW Tag          : ");
  for (int32_t i = 0; i < sizeof(p_FWHeader->FwTag); i++)
  {
    TRACE("%02X", p_FWHeader->FwTag[i]);
  }
  TRACE("\n\r");
  TRACE("Header Signature: ");
  for (int32_t i = 0; i < sizeof(p_FWHeader->HeaderSignature); i++)
  {
    TRACE("%02X", p_FWHeader->HeaderSignature[i]);
    if (!((i + 1) % 32))
    {
      TRACE("\n\r                  ");
    }
  }
  TRACE("\n\r");
};

#endif /* SFU_VERBOSE_DEBUG_MODE */

/**
  * @brief  Initialization function for Header signature verification tracking
    @param  None
  * @retval void This function does not return any value
  */
static void SB_HdrVerifiedInit()
{
  for (int32_t i = 0; i < HDR_VERIFICATION_RECORDS_NUM; i++)
  {
    memset(SB_HdrVerifiedRecord.Hash[i], 0x00, HDR_VERIFICATION_RECORDS_HASHSIZE);
  }
  SB_HdrVerifiedRecord.SlotsUsed = 0;
}

/**
  * @brief  Check if header has already been verified
  * @param  p_Hash     Pointer to 32 byte hash of header to verify
  * @retval int8_t    Returns 1 if Hash has been previously recorded or 0 if not
  */
static int8_t SB_HdrVerifiedCheck(uint8_t *p_Hash)
{
  /* Use of __IO to force recheck of this variable value */
  int32_t __IO e_ret_status = SFU_ERROR;
  uint8_t uSlot = SB_HdrVerifiedRecord.SlotsUsed;

  while ((e_ret_status == SFU_ERROR) && uSlot)
  {
    e_ret_status = MemoryCompare(SB_HdrVerifiedRecord.Hash[uSlot - 1], p_Hash, HDR_VERIFICATION_RECORDS_HASHSIZE);
    uSlot--;
  }
  if (SFU_SUCCESS != e_ret_status)
  {
    return 0; /* no match */
  }
  else
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Found preverification tag in slot %d", uSlot);
#endif /* SFU_VERBOSE_DEBUG_MODE */
    /* Double check in case of fault injection on the first one */
    return ((SFU_SUCCESS == e_ret_status)? 1 /* match */ : 0 /* no match */);
  }
}

/**
  * @brief  Record Hash for a successfully verified header signature
  * @param  p_Hash     pointer to 32 byte hash of header to store
  * @retval void      This function does not return a value
  */
static void SB_HdrVerifiedSet(uint8_t *p_Hash)
{
  if (SB_HdrVerifiedRecord.SlotsUsed == HDR_VERIFICATION_RECORDS_NUM)
  {
    SB_HdrVerifiedInit();
  }
  memcpy(SB_HdrVerifiedRecord.Hash[SB_HdrVerifiedRecord.SlotsUsed], p_Hash, HDR_VERIFICATION_RECORDS_HASHSIZE);
#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Adding preverification tag to slot %d", SB_HdrVerifiedRecord.SlotsUsed);
#endif /* SFU_VERBOSE_DEBUG_MODE */
  SB_HdrVerifiedRecord.SlotsUsed++;
}

#if defined(SFU_VERBOSE_DEBUG_MODE)
/**
  * @brief  Print a DER encoded ECDSA signature
  * @param  p_SigDer     Pointer to DER encoded signature
  * @retval void        This function does not return a value
  */
static void EcdsaSigDerPrint(uint8_t *p_SigDer)
{
  int32_t i = 0;
  int32_t iLoop = 0;
  TRACE("\n\r%02x", p_SigDer[i++]);
  uint8_t DerLength = p_SigDer[i];
  TRACE("%02x", p_SigDer[i++]);
  if (0 != (DerLength & 0x80))
  {
    iLoop = DerLength & 0x7F;
    while (iLoop)
    {
      TRACE("%02x", p_SigDer[++i]);
      iLoop--;
    }
  }
  TRACE("\n\r  %02x", p_SigDer[i++]);
  iLoop = p_SigDer[i];
  TRACE("%02x ", p_SigDer[i++]);
  while (iLoop)
  {
    TRACE("%02x", p_SigDer[i++]);
    iLoop--;
  }
  TRACE("\n\r  %02x", p_SigDer[i++]);
  iLoop = p_SigDer[i];
  TRACE("%02x ", p_SigDer[i++]);
  while (iLoop)
  {
    TRACE("%02x", p_SigDer[i++]);
    iLoop--;
  }
  TRACE("\n\r");
}

#endif /* SFU_VERBOSE_DEBUG_MODE */

/**
  * @brief  Function to DER encode a raw ECDSA signature (r, s)
  * @note   This function currently only supports DER of <= 128 bytes
  * @param  p_r          pointer to the r coordinate of the ECDSA signature
  * @param  size_r     size in bytes of the r coordinate
  * @param  p_s          pointer to the s coordinate of the ECDSA signature
  * @param  size_s     size in bytes of the s coordinate
  * @param  p_der        pointer to the buffer where the der encoded signature
  *                    will be placed
  * @param  p_size_der   pointer to the size of the der buffer
  * @note              when calling the function, caller provids size of the
  *                    der buffer and function will update with actual size of
  *                    der
  * @note              if caller sets der to NULL, function returns the size
  *                    of the required buffer
  * @retval int32_t    -1 if there is an error
  *                    0  if no error but no der encoded data is produced (e.g.
  *                       if caller provides NULL der buffer)
  *                    > 0 size of der encoded data if successful and der data
  *                       is produced
  */
static int32_t EcdsaSigRawToDer(uint8_t *p_r, uint32_t size_r, uint8_t *p_s, uint32_t size_s, uint8_t *p_der,
                                uint32_t *p_size_der)
{
  uint32_t der_size_required = 0;
  der_size_required = size_r + size_s + 4; /* work out buffer size required */

  if (0 != (p_r[0] & 0x80)) /* Need to pad r? */
  {
    der_size_required++;
  }
  if (0 != (p_s[0] & 0x80)) /* Need to pad s? */
  {
    der_size_required++;
  }

  /* Only support <128 bytes - enough for P256 and P384 */
  if (der_size_required >= 128)
  {
    *p_size_der = 0;
    return -1;
  }

  der_size_required += 2; /* add initial 0x30 and length (<128bytes) */

  if ((*p_size_der < der_size_required) || (NULL == p_der))
  {
    *p_size_der = der_size_required;
    return 0; /* buffer is too small, return required size but no data */
  }

  *p_size_der = der_size_required;

  int32_t i = 0;
  p_der[i++] = 0x30; /* start byte */
  p_der[i++] = 0; /* total length to be updated later */
  int32_t tot_size_index = i - 1;
  p_der[i++] = 0x02; /* start of r */
  p_der[i++] = size_r; /* size of r */
  int32_t size_r_index = i - 1;
  if (0 != (p_r[0] & 0x80)) /* padding required for r? */
  {
    p_der[i] = 0x00; /* pad r */
    p_der[tot_size_index] += 1; /* increment total size */
    p_der[size_r_index] += 1; /* increment size of r */
    i++;
  }
  memcpy(&p_der[i], p_r, size_r); /* copy r */
  i += size_r; /* total size */

  p_der[i++] = 0x2; /* start of s */
  p_der[i++] = size_s; /* size of s */
  int32_t size_s_index = i - 1;
  if (p_s[0] & 0x80) /* padding required for s? */
  {
    p_der[i] = 0x00; /* pad s */
    p_der[tot_size_index] += 1; /* increment total size */
    p_der[size_s_index] += 1; /* increment size of s */
    i++;
  }
  memcpy(&p_der[i], p_s, size_s); /* copy s */
  i += size_s; /* total size */
  p_der[tot_size_index] = i - 2; /* update total  size */
  return i;
}

/**
  * @brief  X509 ECDSA Scheme initialization function
  * @retval SFU_ErrorStatus Returns SFU_ERROR if the function fails or
  *                         SFU_SUCCESS if successful
  */
SFU_ErrorStatus SFU_SCHEME_X509_CORE_Init(SE_FwRawHeaderTypeDef *pxFwRawHeader)
{
  SFU_ErrorStatus  e_ret_status = SFU_ERROR;

  SB_HdrVerifiedInit(); /* init preverified header structures */

#if (SBSFU_X509_LOWER_CERTS_SOURCE == SBSFU_X509_USE_PKCS11_CERTS)
  /* we need to load the FW Signing certs from the SE */
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  TRACE("\r\n= [SBOOT] LOADING CERTS FROM SECURE ENGINE");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */

  CK_SESSION_HANDLE xP11Session = (CK_SESSION_HANDLE) NULL;
  CK_FUNCTION_LIST_PTR pxP11FunctionList = (CK_FUNCTION_LIST_PTR) NULL;
  e_ret_status = SFU_SCHEME_X509_CRT_SEOpenSession(&xP11Session, &pxP11FunctionList);

  if ((SFU_SUCCESS != e_ret_status) || (xP11Session == (CK_SESSION_HANDLE) NULL))
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] FAILED TO OPEN PKCS11 SESSION (%d %d %d)", e_ret_status, (uint32_t) xP11Session,
          (uint32_t) pxP11FunctionList);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return e_ret_status; /* error */
  }

  /* FW Signing Root CA Cert */
  e_ret_status = SFU_SCHEME_X509_CRT_GetSECert(xP11Session, pxP11FunctionList, \
                                               (uint8_t *) KMS_SBSFU_ROOT_CA_CRT_LABEL, &p_CertChain_RootCA);
  if (SFU_SUCCESS != e_ret_status)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] FAILED TO LOAD FW SIGNING ROOT CA CERT");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* can't proceed without root cert */
  }

  /* FW Signing OEM CA Cert */
  /* Identify the slot number */
  if (memcmp(pxFwRawHeader->SFUMagic, SFUM_1, strlen(SFUM_1)) == 0)
  {
    e_ret_status = SFU_SCHEME_X509_CRT_GetSECert(xP11Session, pxP11FunctionList, \
                                               (uint8_t *) KMS_SBSFU_OEM_INTM_CA_CRT_1_LABEL, &p_CertChain_OEM);
  }
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  else if (memcmp(pxFwRawHeader->SFUMagic, SFUM_2, strlen(SFUM_2)) == 0)
  {
    e_ret_status = SFU_SCHEME_X509_CRT_GetSECert(xP11Session, pxP11FunctionList, \
                                               (uint8_t *) KMS_SBSFU_OEM_INTM_CA_CRT_2_LABEL, &p_CertChain_OEM);
  }
#endif  /* (NB_FW_IMAGES > 1) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  else if (memcmp(pxFwRawHeader->SFUMagic, SFUM_3, strlen(SFUM_3)) == 0)
  {
    e_ret_status = SFU_SCHEME_X509_CRT_GetSECert(xP11Session, pxP11FunctionList, \
                                               (uint8_t *) KMS_SBSFU_OEM_INTM_CA_CRT_3_LABEL, &p_CertChain_OEM);
  }
#endif  /* (NB_FW_IMAGES > 2) */
  else
  {
    e_ret_status = SFU_ERROR;
  }
  if (SFU_SUCCESS != e_ret_status)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] FAILED TO LOAD FW SIGNING OEM CA CERT");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* can't proceed without this intermediate cert */
  }
  /* Close the session */
  e_ret_status = SFU_SCHEME_X509_CRT_SECloseSession(&xP11Session, &pxP11FunctionList);
  if ((SFU_SUCCESS == e_ret_status) && (!xP11Session) && (!pxP11FunctionList))
  {
    /* success */
  }
  else
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\r\n= [SBOOT] FAILED TO CLOSE PKCS11 SESSION (%d %d %d)", e_ret_status, (uint32_t) xP11Session,
          (uint32_t) pxP11FunctionList);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return e_ret_status; /* error */
  }
  return SFU_SUCCESS;
#else
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  TRACE("\r\n= [SBOOT] Using Certs Embedded in the Firmware");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
  p_CertChain_OEM = a_OEMCACert; /* intermediate cert array */
  p_CertChain_RootCA = a_RootCACert; /* root cert array */
  return SFU_SUCCESS;
#endif /* (SBSFU_X509_LOWER_CERTS_SOURCE == SBSFU_X509_USE_PKCS11_CERTS) */
}


/**
  * @brief This function verifies a Secure Boot / Secure Firmware Update Header
  * @note  Part of the process of verification of a header requires the parsing
  *        and verification of an x509 certificate chain.
  * @note  In this scheme 2 certificates are stored somewhere on the device and
  *        two are delivered with the firmware package as part of the header.
  * @param p_FWHeader         pointer to the sbsfu header to be verified
  * @param p_CertChain_OEM     pointer to the OEM intermediate CA certificate
  * @param p_CertChain_RootCA  pointer to the Root CA certificate
  *
  * @retval SFU_ErrorStatus  Returns SFU_SUCCESS if certificate chain and header
  *                          verification was successful, SFU_ERROR if not
  */
SFU_ErrorStatus SFU_SCHEME_X509_CORE_VerifyFWHeader(SB_FWHeaderTypeDef *p_FWHeader, uint8_t *p_CertChain_OEM,
                                                    uint8_t *p_CertChain_RootCA)
{
  SFU_ErrorStatus ret = SFU_ERROR;
  int32_t ret_local;
  uint8_t HdrHashBuffer[SE_TAG_LEN];
  uint8_t PreVerifiedHash[SE_TAG_LEN];
  uint32_t FWHeader_Size = 0;
  mbedtls_x509_crt MbedCertChain;
  mbedtls_ecdsa_context *p_ecdsa_ctx;
  uint8_t *FW_Int_Certs = NULL;
  uint8_t SigDerBuffer[SE_HEADER_SIGN_LEN + 8] = {0};
  uint32_t SigDerBufferSize = 0;

#if defined(SBSFU_X509_USE_PKCS11DIGEST)
  /*
   * Use PKCS11 C_Digest */
  CK_RV rv;
  CK_SESSION_HANDLE session;
  CK_FLAGS session_flags = CKF_SERIAL_SESSION;  /* Read ONLY session */
  CK_MECHANISM smech;
  CK_ULONG MessageDigestLength = 0;
#else
  /*
   * Use mbedtls_sha256 digest
   */
  mbedtls_sha256_context sha256_ctx;
#endif /* SBSFU_X509_USE_PKCS11DIGEST */

  /*
   * check parameters passed are valid
   */
  if ((NULL == p_FWHeader) || (NULL == p_CertChain_OEM) || (NULL == p_CertChain_RootCA))
  {
    return SFU_ERROR;
  }

  /* Identify the slot number */
  if ((memcmp(p_FWHeader->SFUMagic, SFUM_1, strlen(SFUM_1)) != 0)
#if (SFU_NB_MAX_ACTIVE_IMAGE > 1U)
  &&  (memcmp(p_FWHeader->SFUMagic, SFUM_2, strlen(SFUM_2)) != 0)
#endif  /* (NB_FW_IMAGES > 1) */
#if (SFU_NB_MAX_ACTIVE_IMAGE > 2U)
  &&  (memcmp(p_FWHeader->SFUMagic, SFUM_3, strlen(SFUM_3)) != 0)
#endif  /* (NB_FW_IMAGES > 2) */
  )
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Not a valid header (no magic)");
#endif /* SFU_VERBOSE_DEBUG_MODE */
    return SFU_ERROR; /* no header magic - this is not a header */
  }


  /*
   * check if header and certs have already been verified
   * hash the FW header and certs
   */
#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Hashing header and certs: Addr: 0x%08x, Size: %d", p_FWHeader, sizeof(*p_FWHeader));
#endif /* SFU_VERBOSE_DEBUG_MODE */

#if !defined(SBSFU_X509_USE_PKCS11DIGEST)
  mbedtls_sha256_init(&sha256_ctx);
  mbedtls_sha256_starts_ret(&sha256_ctx, 0);
  mbedtls_sha256_update_ret(&sha256_ctx, (const unsigned char *) p_CertChain_RootCA, /* Root CA Cert */
                            SFU_SCHEME_X509_CRT_SizeOfDer(p_CertChain_RootCA));
  mbedtls_sha256_update_ret(&sha256_ctx, (const unsigned char *) p_CertChain_OEM, /* Intermediate Cert */
                            SFU_SCHEME_X509_CRT_SizeOfDer(p_CertChain_OEM));
  mbedtls_sha256_update_ret(&sha256_ctx, (const unsigned char *) p_FWHeader, sizeof(*p_FWHeader)); /* Header */
  mbedtls_sha256_finish_ret(&sha256_ctx, PreVerifiedHash);
#else

  C_Initialize(NULL);
  rv = C_OpenSession(0,  session_flags, NULL, 0, &session);
  if (rv == CKR_OK)
  {
    smech.pParameter = NULL;
    smech.ulParameterLen = 0;
    smech.mechanism = CKM_SHA256; /* mechanism is sha256 hash */
    rv = C_DigestInit(session, &smech);
  }

  if (rv == CKR_OK) /* Hash Root CA Cert  */
  {
    rv = C_DigestUpdate(session, (CK_BYTE_PTR) p_CertChain_RootCA, SFU_SCHEME_X509_CRT_SizeOfDer(p_CertChain_RootCA));
  }

  if (rv == CKR_OK) /* Hash Intermediate Cert */
  {
    rv = C_DigestUpdate(session, (CK_BYTE_PTR) p_CertChain_OEM, SFU_SCHEME_X509_CRT_SizeOfDer(p_CertChain_OEM));
  }

  if (rv == CKR_OK) /* Hash header */
  {
    rv = C_DigestUpdate(session, (CK_BYTE_PTR) p_FWHeader, sizeof(*p_FWHeader));
  }

  if (rv == CKR_OK)
  {
    MessageDigestLength = SE_TAG_LEN;
    rv = C_DigestFinal(session, (CK_BYTE_PTR)PreVerifiedHash, &MessageDigestLength);
  }

  if (rv != CKR_OK)
  {
    memset(PreVerifiedHash, 0, SE_TAG_LEN);
    MessageDigestLength = 0;
  }

  C_CloseSession(session);
  C_Finalize(NULL);

  if (rv != CKR_OK)
  {
    return SFU_ERROR; /* Error in hash calculation */
  }

  if (MessageDigestLength != SE_TAG_LEN)
  {
    return SFU_ERROR;
  }

#endif /* SBSFU_X509_USE_PKCS11DIGEST */


#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] PreVerified Hash (sha256):\n\r          ");
  for (int32_t i = 0; i < sizeof(PreVerifiedHash); i++)
  {
    TRACE("%02x", PreVerifiedHash[i]);
  }
  TRACE("\n\r");
#endif /* SFU_VERBOSE_DEBUG_MODE */

  if (0 != SB_HdrVerifiedCheck(PreVerifiedHash))
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] SB Header already verified");
#endif /* SFU_VERBOSE_DEBUG_MODE */
    return SFU_SUCCESS; /* Header already verified */
  }

  /* At this point we know we have not yet verified this header */
  FW_Int_Certs = (unsigned char *)(p_FWHeader->Certificates); /* Certs delivered with the header */

  /* Verify certificate chain before using FW signing certificate */
#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Verifying certificate chain...");
  TRACE("\n\r= [SBOOT] Size of RootCA %d", SFU_SCHEME_X509_CRT_SizeOfDer(p_CertChain_RootCA));
  TRACE("\n\r= [SBOOT] Size of OEM CA %d", SFU_SCHEME_X509_CRT_SizeOfDer(p_CertChain_OEM));
#endif /* SFU_VERBOSE_DEBUG_MODE */
  ret = SFU_SCHEME_X509_CRT_VerifyCert(FW_Int_Certs, p_CertChain_OEM, p_CertChain_RootCA, &MbedCertChain);
  if (SFU_SUCCESS == ret)
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Certificate chain verified OK");
#endif /* SFU_VERBOSE_DEBUG_MODE */
  }
  else
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] SFU_SCHEME_X509_CRT_VerifyCert FAILED with error: %d\n\r", ret);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return ret; /* Cert chain verification failed, can't proceed */
  }

  /*
   * Verify the header signature - hash fw header except the signature & other mutable data
   */
  FWHeader_Size = ((uint32_t) p_FWHeader->HeaderSignature) - ((uint32_t) p_FWHeader);

#if !defined(SBSFU_X509_USE_PKCS11DIGEST)
  mbedtls_sha256_init(&sha256_ctx);
  mbedtls_sha256_starts_ret(&sha256_ctx, 0);
  mbedtls_sha256_update_ret(&sha256_ctx, (const unsigned char *) p_FWHeader, FWHeader_Size);
  mbedtls_sha256_finish_ret(&sha256_ctx, HdrHashBuffer);
#else
  C_Initialize(NULL);

  rv = C_OpenSession(0,  session_flags, NULL, 0, &session);

  if (rv == CKR_OK)
  {
    smech.pParameter = NULL;
    smech.ulParameterLen = 0;
    smech.mechanism = CKM_SHA256;
    rv = C_DigestInit(session, &smech);
  }

  if (rv == CKR_OK)
  {
    rv = C_DigestUpdate(session, (CK_BYTE_PTR) p_FWHeader, FWHeader_Size);
  }

  if (rv == CKR_OK)
  {
    MessageDigestLength = SE_TAG_LEN;
    rv = C_DigestFinal(session, (CK_BYTE_PTR) HdrHashBuffer, &MessageDigestLength);
  }

  if (rv != CKR_OK)
  {
    memset(PreVerifiedHash, 0, SE_TAG_LEN);
    MessageDigestLength = 0;
  }

  C_CloseSession(session);
  C_Finalize(NULL);

  if (rv != CKR_OK)
  {
    return SFU_ERROR;
  }

  if (MessageDigestLength != SE_TAG_LEN)
  {
    return SFU_ERROR;
  }
#endif /* SBSFU_X509_USE_PKCS11DIGEST */

#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] FW Header Hash (sha256):\n\r          ");
  for (int32_t i = 0; i < sizeof(HdrHashBuffer); i++)
  {
    TRACE("%02x", HdrHashBuffer[i]);
  }
  TRACE("\n\r");
#endif /* SFU_VERBOSE_DEBUG_MODE */

  /* Header has a raw signature but mbedtls requires a DER encoded signature, so convert it */
  SigDerBufferSize = sizeof(SigDerBuffer);
  int32_t Asn1Size = EcdsaSigRawToDer(&p_FWHeader->HeaderSignature[0], sizeof(p_FWHeader->HeaderSignature) / 2, \
                                      &p_FWHeader->HeaderSignature[SE_HEADER_SIGN_LEN / 2], sizeof(p_FWHeader->HeaderSignature) / 2, \
                                      SigDerBuffer, &SigDerBufferSize);
  if (Asn1Size < 0)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] Error converting Raw ECDSA Sig to DER");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    return SFU_ERROR;
  }

#if defined(SFU_VERBOSE_DEBUG_MODE)
  TRACE("\n\rGenerated %d DER, required %d size buffer\n\r", Asn1Size, SigDerBufferSize);
  TRACE("\n\r= [SBOOT] Signature DER (%d bytes):", Asn1Size);
  EcdsaSigDerPrint(SigDerBuffer);
  TRACE("\n\r= [SBOOT] Firmware Header:\n\r");
  SFU_SCHEME_X509_CORE_PrintHeader(p_FWHeader);
#endif /* SFU_VERBOSE_DEBUG_MODE */


#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
  TRACE("\n\r= [SBOOT] Verify Header Signature... ");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
  p_ecdsa_ctx = mbedtls_pk_ec(MbedCertChain.pk);
  ret_local = mbedtls_ecdsa_read_signature(p_ecdsa_ctx, HdrHashBuffer, sizeof(HdrHashBuffer), \
                                           SigDerBuffer, SigDerBufferSize);
  if (0 != ret_local)
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("\n\r= [SBOOT] FAILED with error %d\n\r", ret);
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    mbedtls_x509_crt_free(&MbedCertChain);
    return SFU_ERROR; /* Signature Verification Failed */
  }
  else
  {
#if defined(SFU_X509_VERBOSE_DEBUG_MODE)
    TRACE("OK");
#endif /* SFU_X509_VERBOSE_DEBUG_MODE */
    SB_HdrVerifiedSet(PreVerifiedHash);
    mbedtls_x509_crt_free(&MbedCertChain);
    return SFU_SUCCESS; /* Signature Verification Passed */
  }
}

#endif /* SCHEME_X509_ECDSA_WITHOUT_ENCRYPT_SHA256 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
