/**
  ******************************************************************************
  * @file    ca_err_codes.h
  * @author  MCD Application Team
  * @brief   This file contains the error codes definitions of the Cryptographic API (CA) module.
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
#ifndef CA_ERR_CODES_H
#define CA_ERR_CODES_H

/* Configuration defines -----------------------------------------------------*/



/* Includes ------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Cryptographic_API Cryptographic API (CA)
  * @{
  */


/** @addtogroup ErrCodes Error Codes Definitions
  * @{
  */

/** @defgroup GenericError Generic Error Codes
  * @{
  */

#define CA_SUCCESS                   (int32_t) (0)             /*!< Operation successful */
#define CA_ERROR                     (int32_t) (1)             /*!< Operation unsuccessful */

#define CA_AUTHENTICATION_SUCCESSFUL (int32_t) (1003)          /*!< Authentication successful */
#define CA_AUTHENTICATION_FAILED     (int32_t) (1004)          /*!< Authentication failed */
#define CA_SIGNATURE_VALID           CA_AUTHENTICATION_SUCCESSFUL /*!< Signature is valid */
#define CA_SIGNATURE_INVALID         CA_AUTHENTICATION_FAILED     /*!< Signature is NOT valid */
#define CA_ERR_MEMORY_FAIL (int32_t) (1005)                    /*!< Problems with dynamic allocation
                                                                    (there's no more available memory) */
/**
  * @}
  */

/** @defgroup AESError AES Error Codes
  * @{
  */
#define CA_AES_SUCCESS             (int32_t) (0)    /*!< AES of PRIVKEY Success */
#define CA_AES_ERR_BAD_INPUT_SIZE  (int32_t) (3101) /*!< AES of PRIVKEY Invalid input size */
#define CA_AES_ERR_BAD_OPERATION   (int32_t) (3102) /*!< AES of PRIVKEY Invalid operation */
#define CA_AES_ERR_BAD_CONTEXT     (int32_t) (3103) /*!< AES of PRIVKEY The AES context contains some invalid
                                                         or uninitialized values */
#define CA_AES_ERR_BAD_PARAMETER   (int32_t) (3104) /*!< AES of PRIVKEY One of the expected function parameters
                                                         is invalid */
/**
  * @}
  */

/** @defgroup HASHError HASH Error Codes
  * @{
  */
#define CA_HASH_SUCCESS             (int32_t) (0)    /*!< hash Success */
#define CA_HASH_ERR_BAD_OPERATION   (int32_t) (4001) /*!< hash Invalid operation */
#define CA_HASH_ERR_BAD_CONTEXT     (int32_t) (4002) /*!< hash The HASH context contains some invalid
                                                          or uninitialized values */
#define CA_HASH_ERR_BAD_PARAMETER   (int32_t) (4003) /*!< hash One of the expected function parameters is invalid */
#define CA_HASH_ERR_INTERNAL        (int32_t) (4011) /*!< hash Generic internal error */
/**
  * @}
  */

/** @defgroup RSAError RSA Error Codes
  * @{
  */
#define CA_RSA_SUCCESS               (int32_t) (0)     /*!< RSA Success */
#define CA_RSA_ERR_BAD_OPERATION     (int32_t) (5102)  /*!< RSA Invalid operation */
#define CA_RSA_ERR_BAD_KEY           (int32_t) (5103)  /*!< RSA Invalid Key */
#define CA_RSA_ERR_BAD_PARAMETER     (int32_t) (5104)  /*!< RSA One of the expected function parameters is invalid */
#define CA_RSA_ERR_UNSUPPORTED_HASH  (int32_t) (5105)  /*!< RSA The hash function is not supported */
#define CA_RSA_ERR_MESSAGE_TOO_LONG  (int32_t) (5106)  /*!< RSA Message too long */
#define CA_RSA_ERR_MODULUS_TOO_SHORT (int32_t) (5107)  /*!< RSA modulus too short */
#define CA_RSA_ERR_GENERIC           (int32_t) (5108)  /*!< RSA Generic Error */
/**
  * @}
  */

/** @defgroup ECCError ECC Error Codes
  * @{
  */
#define CA_ECC_SUCCESS              (int32_t) (0)        /*!< ecc Success */
#define CA_ECC_ERR_BAD_OPERATION    (int32_t) (5202)     /*!< ecc Invalid operation */
#define CA_ECC_ERR_BAD_CONTEXT      (int32_t) (5203)     /*!< ecc The ECC context contains some invalid
                                                              or initialized parameters */
#define CA_ECC_ERR_BAD_PARAMETER    (int32_t) (5204)     /*!< ecc One of the expected function parameters is invalid */
#define CA_ECC_ERR_BAD_PUBLIC_KEY   (int32_t) (5205)     /*!< ecc Invalid Public Key */
#define CA_ECC_ERR_BAD_PRIVATE_KEY  (int32_t) (5206)     /*!< ecc Invalid Private Key */
#define CA_ECC_ERR_MISSING_EC_PARAMETER (int32_t) (5207) /*!< ecc The EC parameters structure miss some parameter
                                                              required by the function */
#define CA_ECC_WARN_POINT_AT_INFINITY   (int32_t) (5208) /*!< ecc Returned Point is the point at infinity */
/**
  * @}
  */

/** @defgroup RNGError Random Number Error Codes
  * @{
  */
#define CA_RNG_SUCCESS                  (int32_t) (0)    /*!< RNG Success */
#define CA_RNG_ERR_UNINIT_STATE         (int32_t) (6001) /*!< RNG has not been correctly initialized */
#define CA_RNG_ERR_BAD_OPERATION        (int32_t) (6002) /*!< RNG Invalid operation */
#define CA_RNG_ERR_RESEED_NEEDED        (int32_t) (6003) /*!< RNG Reseed is needed */
#define CA_RNG_ERR_BAD_PARAMETER        (int32_t) (6004) /*!< RNG One of the expected function parameters is invalid */
#define CA_RNG_ERR_BAD_ENTROPY_SIZE     (int32_t) (6006) /*!< RNG Check the size of the entropy string */
#define CA_RNG_ERR_BAD_PERS_STRING_SIZE (int32_t) (6007) /*!< RNG Check the size of the personalization string */
#define CA_RNG_ERR_BAD_ADD_INPUT_SIZE   (int32_t) (6008) /*!< RNG Check the size of the additional input string */
#define CA_RNG_ERR_BAD_REQUEST          (int32_t) (6009) /*!< RNG Check the size of the random request */
#define CA_RNG_ERR_BAD_NONCE_SIZE       (int32_t) (6010) /*!< RNG Check the size of the nocne */
#define CA_RNG_ERR_INTERNAL             (int32_t) (6011) /*!< RNG Generic internal RNG error */
/**
  * @}
  */

/** @defgroup MathError Mathemtical Error Codes
  * @{
  */
#define CA_MATH_SUCCESS                (int32_t) (0)    /*!< Math Success */
#define CA_MATH_ERR_BIGNUM_OVERFLOW    (int32_t) (5301) /*!< Math Overflow, the returned BigNum would be greater than
                                                             its maximum size */
#define CA_MATH_ERR_EVEN_MODULUS       (int32_t) (5302) /*!< Math This function can be used only with odd moduli */
#define CA_MATH_ERR_BAD_PARAMETER      (int32_t) (5304) /*!< Math One of the expected function parameters is invalid */
#define CA_MATH_ERR_INTERNAL           (int32_t) (5311) /*!< Math Generic internal error */
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

#endif /* CA_ERR_CODES_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

