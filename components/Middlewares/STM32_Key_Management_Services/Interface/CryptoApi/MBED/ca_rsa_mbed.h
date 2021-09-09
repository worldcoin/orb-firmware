/**
  ******************************************************************************
  * @file    ca_rsa_mbed.h
  * @author  MCD Application Team
  * @brief   This file contains the RSA router includes and definitions of
  *          the Cryptographic API (CA) module to the MBED Cryptographic library.
  * @note    This file shall never be included directly by application but
  *          through the main header ca_hash.h
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_RSA_MBED_H
#define CA_RSA_MBED_H

#if !defined(CA_RSA_H)
#error "This file shall never be included directly by application but through the main header ca_rsa.h"
#endif /* CA_RSA_H */

/* Configuration defines -----------------------------------------------------*/

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> RSA */
#if defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED)
/* Includes ------------------------------------------------------------------*/

/* Defines ------------------------------------------------------------------*/
#define CA_RSA_REQUIRED_WORKING_BUFFER    4  /* Reduce size to minimum but aligned on 4 bytes */
#define CA_RSA_ADD_PUBEXP_IN_PRIVATEKEY

/* Typedef ------------------------------------------------------------------*/
/**
  * @brief  Structure type for RSA public key
  */
typedef struct
{
  uint8_t  *pmModulus;    /*!< RSA Modulus */
  int32_t  mModulusSize;  /*!< Size of RSA Modulus */
  uint8_t  *pmExponent;   /*!< RSA Public Exponent */
  int32_t  mExponentSize; /*!< Size of RSA Public Exponent */
}
CA_RSApubKey_stt;

/**
  * @brief  Structure type for RSA private key
  */
typedef struct
{
  uint8_t  *pmModulus; /*!< RSA Modulus */
  int32_t  mModulusSize; /*!< Size of RSA Modulus */
  uint8_t  *pmExponent; /*!< RSA Private Exponent */
  int32_t  mExponentSize; /*!< Size of RSA Private Exponent */
  /* Required by MBED to compute p & q from d,n & e */
  uint8_t  *pmPubExponent;   /*!< RSA Public Exponent */
  int32_t  mPubExponentSize; /*!< Size of RSA Public Exponent */
}
CA_RSAprivKey_stt;

/* Exported functions -------------------------------------------------------*/
#if (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_SIGN_ENABLE)
int32_t CA_RSA_PKCS1v15_Sign(const CA_RSAprivKey_stt *P_pPrivKey, const uint8_t *P_pDigest, CA_hashType_et P_hashType,
                             uint8_t *P_pSignature, CA_membuf_stt *P_pMemBuf);
#endif /* (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_SIGN_ENABLE) */

#if (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_VERIFY_ENABLE)
int32_t CA_RSA_PKCS1v15_Verify(const CA_RSApubKey_stt *P_pPubKey, const uint8_t *P_pDigest, CA_hashType_et P_hashType,
                               const uint8_t *P_pSignature, CA_membuf_stt *P_pMemBuf);
#endif /* (CA_ROUTE_RSA & CA_ROUTE_RSA_CFG_VERIFY_ENABLE) */
#endif /* (CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< RSA */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_RSA_MBED_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

