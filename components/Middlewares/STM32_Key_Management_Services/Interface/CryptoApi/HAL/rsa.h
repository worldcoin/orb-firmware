/**
  *******************************************************************************
  * @file    rsa.h
  * @author  ST
  * @version V1.0.0
  * @date    13-February-2020
  * @brief   Prototype for the RSA function
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  *******************************************************************************
  */

#ifndef RSA_H_
#define RSA_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
  * @brief   Macro defining maximum bits supported by RSA
  * @note    Could be 1024, 2048, 3072
  * @warning MUST be not greater than 3072
  */
#define RSA_SUPPORT_MAX_SIZE 3072u

/**
  * @brief   RSA errors definitions
  */
typedef enum
{
  RSA_SUCCESS,                /*!< RSA operation successfully performed */
  RSA_ERR_INVALID_SIGNATURE,  /*!< RSA invalid signature value */
  RSA_ERR_BAD_PARAMETER,      /*!< One of the expected parameter is invalid */
  RSA_ERR_PKA_INIT,           /*!< RSA PKA Init error */
  RSA_ERR_PKA_MODEXP,         /*!< RSA PKA ModExp internal error */
  RSA_ERR_PKA_DEINIT,         /*!< RSA PKA DeInit error */
  RSA_ERR_MODULUS_TOO_SHORT,  /*!< Input too long for the current modulus */
} rsa_error_t;

struct rsa_key_t;
typedef struct rsa_key_t rsa_key_t; /*!< Structure to hold public or private key parameters */

/**
  * @brief Function type for the Private Modular Exponentiation
  */
typedef rsa_error_t (*rsa_priv_func_t)(const rsa_key_t *key,
                                       const uint8_t   *input,
                                       uint8_t         *output);

/**
  * @brief Structure to hold public or private key parameters
  */
struct rsa_key_t
{
  rsa_priv_func_t   f;          /*!< Function executing CRT/standard modexp */
  size_t            mod_len;    /*!< Length (in bytes) of Modulus */

  union
  {
    /* Standard public/private fields */
    struct
    {
      const uint8_t *mod;       /*!< Modulus  */
      const uint8_t *exp;       /*!< Public/secret Exponent */
      size_t        exp_len;    /*!< Length (in bytes) of Exponent */
    } std;

    /* CRT fields */
    struct
    {
      const uint8_t *p;         /*!< Parameter P (in case of CRT) or normal modulus  */
      const uint8_t *q;         /*!< Parameter Q */
      const uint8_t *dp;        /*!< Secret exponent (mod P) (in case of CRT), or normal secret exponent */
      const uint8_t *dq;        /*!< Secret exponent (mod Q) */
      const uint8_t *iq;        /*!< Q^(-1) (mod P) */
    } crt;
  } fields;                     /*!< RSA key fields */
};

/**
  * @brief Structure to hold Hash information
  */
struct rsa_pkcs_hash_t;
typedef struct rsa_pkcs_hash_t rsa_pkcs_hash_t;   /*!< Structure to hold Hash information */

/**
  *  @brief Hash contexts to use with RSA PKCS#1 API
  */

extern const rsa_pkcs_hash_t RSA_Hash_SHA1;
extern const rsa_pkcs_hash_t RSA_Hash_SHA256;

/**
  * @brief       Set the key (public or private) in the key structure
  * @param[out]  P_pKey        Key to set
  * @param[in]   P_pModulus    Modulus
  * @param[in]   P_ModulusLen  Modulus Length (in Bytes)
  * @param[in]   P_pExp        Exponent (private or public)
  * @param[in]   P_ExpLen      Exponent Length (in Bytes)
  * @retval      RSA_SUCCESS             Ok
  * @retval      RSA_ERR_BAD_PARAMETER   NULL inputs or modulus/exponent (of the
  *                                      key) bigger than what supported
  */
rsa_error_t RSA_SetKey(rsa_key_t      *P_pKey,
                       const uint8_t  *P_pModulus,
                       size_t         P_ModulusLen,
                       const uint8_t  *P_pExp,
                       size_t         P_ExpLen);

/**
  * @brief       Set the private CRT key in the key structure
  * @param[out]  P_pPrivKey    Private Key to set
  * @param[in]   P_ModulusLen  Modulus Length (in bits)
  * @param[in]   P_pExpP       dP
  * @param[in]   P_pExpQ       dQ
  * @param[in]   P_pP          P
  * @param[in]   P_pQ          Q
  * @param[in]   P_pIq         invQ
  * @retval      RSA_SUCCESS             Ok
  * @retval      RSA_ERR_BAD_PARAMETER   NULL inputs or modulus/exponent (of the
  *                                      key) bigger than what supported
  */
rsa_error_t RSA_SetKeyCRT(rsa_key_t      *P_pPrivKey,
                          size_t         P_ModulusLen,
                          const uint8_t  *P_pExpP,
                          const uint8_t  *P_pExpQ,
                          const uint8_t  *P_pP,
                          const uint8_t  *P_pQ,
                          const uint8_t  *P_pIq);


/**
  * @brief       Sign a message using PKCS#1 v1.5
  * @param[in]   P_pPrivKey      Private Key (standard or CRT)
  * @param[in]   P_pInput        Message to sign
  * @param[in]   P_InputLen      Message Length (in Bytes)
  * @param[in]   P_pHashId       Hash to use
  * @param[out]  P_pSignature    Output signature
  * @retval      RSA_SUCCESS                 Ok
  * @retval      RSA_ERR_BAD_PARAMETER       NULL/empty inputs or key not set
  * @retval      RSA_ERR_MODULUS_TOO_SHORT   Input length is too long
  * @retval      RSA_ERR_PKA_INIT            Error during the HAL_PKA_Init
  * @retval      RSA_ERR_PKA_MODEXP          Error during the HAL_PKA_ModExp
  * @retval      RSA_ERR_PKA_DEINIT          Error during the HAL_PKA_DeInit
  */
rsa_error_t RSA_PKCS1v15_Sign(const rsa_key_t       *P_pPrivKey,
                              const uint8_t         *P_pInput,
                              size_t                P_InputLen,
                              const rsa_pkcs_hash_t *P_pHashId,
                              uint8_t               *P_pSignature);


/**
  * @brief       Verify a message signature using PKCS#1 v1.5
  * @param[in]   P_pPubKey       Public Key
  * @param[in]   P_pInput        Message to verify
  * @param[in]   P_InputLen      Message Length (in Bytes)
  * @param[in]   P_pHashId       Hash to use
  * @param[in]   P_pSignature    Signature
  * @param[in]   P_SignatureLen  Signature Length (in Bytes)
  * @retval      RSA_SUCCESS                 Ok
  * @retval      RSA_ERR_BAD_PARAMETER       NULL/empty inputs or key not set
  * @retval      RSA_ERR_INVALID_SIGNATURE   Signature is NULL/empty, with length
                                            different from moduls length, or wrong
  * @retval      RSA_ERR_MODULUS_TOO_SHORT   Input length is too long
  * @retval      RSA_ERR_PKA_INIT            Error during the HAL_PKA_Init
  * @retval      RSA_ERR_PKA_MODEXP          Error during the HAL_PKA_ModExp
  * @retval      RSA_ERR_PKA_DEINIT          Error during the HAL_PKA_DeInit
  */
rsa_error_t RSA_PKCS1v15_Verify(const rsa_key_t       *P_pPubKey,
                                const uint8_t         *P_pInput,
                                size_t                P_InputLen,
                                const rsa_pkcs_hash_t *P_pHashId,
                                const uint8_t         *P_pSignature,
                                size_t                P_SignatureLen);

#ifdef __cplusplus
}
#endif

#endif /* RSA_H_ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
