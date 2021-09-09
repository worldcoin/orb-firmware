/**
  ******************************************************************************
  * @file    ca_defines.h
  * @author  MCD Application Team
  * @brief   This file contains the constants definitions of the Cryptographic API (CA) module.
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
#ifndef CA_DEFINES_H
#define CA_DEFINES_H

/* Includes ------------------------------------------------------------------*/

/* To avoid ST Cryptographic library header inclusion */
#if !defined(__CRL_SK_H__)
#define __CRL_SK_H__
#endif /* __CRL_SK_H__ */
#if !defined(__CRL_BN_H__)
#define __CRL_BN_H__
#endif /* __CRL_BN_H__ */


/* Exported constants --------------------------------------------------------*/
#define CA_NBIT_DIGIT        32UL          /*!< Bit Size of a BigNum digit (should equal to CPU word size, works for 32)  */
#define CA_MSBIT_MASK        0x80000000UL  /*!< Mask for the most significant bit in a digit */
#define CA_MSBYTE_MASK       0xFF000000UL  /*!< Mask for the most significant byte in a digit */
#define CA_LSBIT_MASK        0x00000001UL  /*!< Mask for the least significant bit in a digit */
#define CA_MAX_DIGIT_VALUE   0xFFFFFFFFUL  /*!< Mask for all bit to 1 in a word */
#define CA_SIGN_POSITIVE     0             /*!< Used to denote a positive Big Number  */
#define CA_SIGN_NEGATIVE     1             /*!< Used to denote a negative Big Number  */

#define CA_CRL_AES128_KEY   16UL /*!< Number of bytes (uint8_t) necessary to store an AES key of 128 bits  */
#define CA_CRL_AES192_KEY   24UL /*!< Number of bytes (uint8_t) necessary to store an AES key of 192 bits  */
#define CA_CRL_AES256_KEY   32UL /*!< Number of bytes (uint8_t) necessary to store an AES key of 256 bits  */
#define CA_CRL_AES_BLOCK    16UL /*!< Number of bytes (uint8_t) necessary to store an AES block  */

#define CA_CRL_SHA1_SIZE    20UL /*!<  Number of bytes (uint8_t) to store a SHA-1 digest  */
#define CA_CRL_SHA256_SIZE  32UL /*!<  Number of bytes (uint8_t) to store a SHA-256 digest  */

#define CA_CRL_RSA1024_MOD_SIZE   (1024UL/8UL)  /*!< Number of bytes (uint8_t) to store a RSA1024 Modulus  */
#define CA_CRL_RSA1024_PRIV_SIZE  (1024UL/8UL)  /*!< Number of bytes (uint8_t) to store a RSA1024 private exponent  */
#define CA_CRL_RSA1024_PUB_SIZE   (1024UL/8UL)  /*!< Number of bytes (uint8_t) to store a RSA1024 public exponent  */
#define CA_CRL_RSA2048_MOD_SIZE   (2048UL/8UL)  /*!< Number of bytes (uint8_t) to store a RSA2048 Modulus  */
#define CA_CRL_RSA2048_PRIV_SIZE  (2048UL/8UL)  /*!< Number of bytes (uint8_t) to store a RSA2048 private exponent  */
#define CA_CRL_RSA2048_PUB_SIZE   (2048UL/8UL)  /*!< Number of bytes (uint8_t) to store a RSA2048 public exponent  */

#define CA_CRL_ECC_P192_SIZE      (192UL/8UL)   /*!< Number of bytes (uint8_t) to store an ECC P192 curve element */
#define CA_CRL_ECC_P256_SIZE      (256UL/8UL)   /*!< Number of bytes (uint8_t) to store an ECC P256 curve element */
#define CA_CRL_ECC_P384_SIZE      (384UL/8UL)   /*!< Number of bytes (uint8_t) to store an ECC P384 curve element */
#define CA_CRL_ECC_BIGGEST_SIZE   (384UL/8UL)   /*!< Number of bytes (uint8_t) to store an ECC biggest curve element */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_DEFINES_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

