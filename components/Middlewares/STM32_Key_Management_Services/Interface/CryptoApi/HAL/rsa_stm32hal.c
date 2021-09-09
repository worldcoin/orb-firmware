/**
  *******************************************************************************
  * @file    rsa_stm32hal.c
  * @author  ST
  * @version V1.0.0
  * @date    12-February-2020
  * @brief   Computes RSA PKCS#1 v1.5 using the STM32 PKA
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

/* CA sources are built by building ca_core.c giving it the proper ca_config.h */
/* This file can not be build alone                                            */
#if defined(CA_CORE_C)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_RSA_STM32HAL_C
#define CA_RSA_STM32HAL_C

#include "HAL/rsa.h"

#include <stdint.h>
#include <string.h>

#define BITS_TO_BYTES(n)  (((n) + ((size_t)7u)) >> ((size_t)3u))  /* Number of bits to number of bytes conversion */
#define EXP_TIMEOUT_VALUE (20000U)                                /* Generic PKA processing timeout */

/**
  * @brief       Call the PKA modular exponentiation
  * @param[in]   key     Exponent for the ModExp
  * @param[in]   input   Input of the modexp
  * @param[out]  output  Output of the ModExp (length of the modulus)
  * @retval      RSA_SUCCESS         Ok
  * @retval      RSA_ERR_PKA_INIT    Error during the HAL_PKA_Init
  * @retval      RSA_ERR_PKA_MODEXP  Error during the HAL_PKA_ModExp
  * @retval      RSA_ERR_PKA_DEINIT  Error during the HAL_PKA_DeInit
  */
static rsa_error_t RSA_PKA_modexp(const rsa_key_t *key,
                                  const uint8_t   *input,
                                  uint8_t         *output)
{
  PKA_HandleTypeDef hpka;
  PKA_ModExpInTypeDef in;
  rsa_error_t rv = RSA_ERR_PKA_MODEXP;

  /* Global initialization of PKA */
  (void)memset(&hpka, 0, sizeof(PKA_HandleTypeDef));
  (void)memset(&in, 0, sizeof(PKA_ModExpInTypeDef));
  hpka.Instance = PKA;
  if (HAL_PKA_Init(&hpka) != HAL_OK)
  {
    rv = RSA_ERR_PKA_INIT;
  }
  else
  {
    /* Set input parameters */
    in.expSize = key->fields.std.exp_len;   /* Exponent size */
    in.OpSize  = key->mod_len;              /* Modulus size */
    in.pOp1    = input;                     /* Input data for the modular exponentiation */
    in.pExp    = key->fields.std.exp;       /* Exponent */
    in.pMod    = key->fields.std.mod;       /* Modulus */

    /* Start the modular exponentiation */
    if (HAL_PKA_ModExp(&hpka, &in, EXP_TIMEOUT_VALUE) == HAL_OK)
    {
      rv = RSA_SUCCESS;
      /* Copy the results to user specified space */
      HAL_PKA_ModExp_GetResult(&hpka, output);
    }

    /* De initialization of PKA */
    if (HAL_PKA_DeInit(&hpka) != HAL_OK)
    {
      /* do not overwrite other previous errors */
      if (rv == RSA_SUCCESS)
      {
        rv = RSA_ERR_PKA_DEINIT;
      }
    }
  }

  return rv;
}

/**
  * @brief       Call the PKA CRT routine
  * @param[in]   key     Exponent for the ModExp
  * @param[in]   input   Input of the modexp
  * @param[out]  output  Output of the ModExp (length of the modulus)
  * @retval      RSA_SUCCESS         Ok
  * @retval      RSA_ERR_PKA_INIT    Error during the HAL_PKA_Init
  * @retval      RSA_ERR_PKA_MODEXP  Error during the HAL_PKA_ModExp
  * @retval      RSA_ERR_PKA_DEINIT  Error during the HAL_PKA_DeInit
  */
static rsa_error_t RSA_CRT_PKA_modexp(const rsa_key_t *key,
                                      const uint8_t   *input,
                                      uint8_t         *output)
{
  PKA_HandleTypeDef hpka;
  PKA_RSACRTExpInTypeDef in;
  rsa_error_t rv = RSA_ERR_PKA_MODEXP;

  /* Global initialization of PKA */
  (void)memset(&hpka, 0, sizeof(PKA_HandleTypeDef));
  (void)memset(&in, 0, sizeof(PKA_RSACRTExpInTypeDef));
  hpka.Instance = PKA;
  if (HAL_PKA_Init(&hpka) != HAL_OK)
  {
    rv = RSA_ERR_PKA_INIT;
  }
  else
  {
    /* Set input parameters */
    in.size    = key->mod_len;          /* Modulus size */
    in.pOpDp   = key->fields.crt.dp;    /* dp parameter */
    in.pOpDq   = key->fields.crt.dq;    /* dq parameter */
    in.pOpQinv = key->fields.crt.iq;    /* iq parameter */
    in.pPrimeP = key->fields.crt.p;     /* p parameter */
    in.pPrimeQ = key->fields.crt.q;     /* q parameter */
    in.popA    = input;                 /* Input data for the modular exponentiation */

    /* Start the modular exponentiation */
    if (HAL_PKA_RSACRTExp(&hpka, &in, EXP_TIMEOUT_VALUE) == HAL_OK)
    {
      rv = RSA_SUCCESS;
      /* Copy the results to user specified space */
      HAL_PKA_ModExp_GetResult(&hpka, output);
    }

    /* De initialization of PKA */
    if (HAL_PKA_DeInit(&hpka) != HAL_OK)
    {
      /* do not overwrite other previous errors */
      if (rv == RSA_SUCCESS)
      {
        rv = RSA_ERR_PKA_DEINIT;
      }
    }
  }

  return rv;
}

static const uint8_t rsa_pkcs_sha1_id[] = /* Buffer with SHA1 Identifier for PKCS#1 v1.5 */
{
  0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
  0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
};

static const uint8_t rsa_pkcs_sha256_id[] = /* Buffer with SHA256 Identifier for PKCS#1 v1.5 */
{
  0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
  0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
  0x00, 0x04, 0x20
};

struct rsa_pkcs_hash_t /* Structure definition for PKCS Hash */
{
  const uint8_t  *id;
  const size_t   id_len;
};

const rsa_pkcs_hash_t RSA_Hash_SHA1 = /* PKCS Hash structure for SHA1 */
{
  .id = rsa_pkcs_sha1_id,
  .id_len = sizeof(rsa_pkcs_sha1_id),
};

const rsa_pkcs_hash_t RSA_Hash_SHA256 = /* PKCS Hash structure for SHA256 */
{
  .id = rsa_pkcs_sha256_id,
  .id_len = sizeof(rsa_pkcs_sha256_id),
};

/* Set the key (public or private) in the key structure */
rsa_error_t RSA_SetKey(rsa_key_t      *P_pKey,
                       const uint8_t  *P_pModulus,
                       size_t         P_ModulusLen,
                       const uint8_t  *P_pExp,
                       size_t         P_ExpLen)
{
  rsa_error_t retval;

  /* Check input parameters */
  if ((P_pKey == NULL)
      || (P_pModulus == NULL) || (P_ModulusLen == 0u) || (P_ModulusLen > BITS_TO_BYTES(RSA_SUPPORT_MAX_SIZE))
      || (P_pExp == NULL) || (P_ExpLen == 0u) || (P_ExpLen > BITS_TO_BYTES(RSA_SUPPORT_MAX_SIZE)))
  {
    retval = RSA_ERR_BAD_PARAMETER;
  }
  else
  {
    /* Set input values inside the key context */
    P_pKey->f = RSA_PKA_modexp;                   /* Choose mudlar exponentiation primitive */
    P_pKey->fields.std.mod = P_pModulus;          /* Modulus */
    P_pKey->mod_len = P_ModulusLen;               /* Modulus size */
    P_pKey->fields.std.exp = P_pExp;              /* Exponent */
    P_pKey->fields.std.exp_len = P_ExpLen;        /* Exponent size */

    retval = RSA_SUCCESS;
  }

  return retval;
}

/* Set the private CRT key in the key structure */
rsa_error_t RSA_SetKeyCRT(rsa_key_t     *P_pPrivKey,
                          size_t        P_ModulusLen,
                          const uint8_t *P_pExpP,
                          const uint8_t *P_pExpQ,
                          const uint8_t *P_pP,
                          const uint8_t *P_pQ,
                          const uint8_t *P_pIq)
{
  rsa_error_t retval;

  /* Check input parameters */
  if ((P_pPrivKey == NULL) || (P_pExpP == NULL) || (P_pExpQ == NULL)
      || (P_pP == NULL) || (P_pQ == NULL) || (P_pIq == NULL)
      || (P_ModulusLen == 0u) || (P_ModulusLen > BITS_TO_BYTES(RSA_SUPPORT_MAX_SIZE)))
  {
    retval = RSA_ERR_BAD_PARAMETER;
  }
  else
  {
    /* Set input values inside the key context */
    P_pPrivKey->f = RSA_CRT_PKA_modexp;       /* Choose mudlar exponentiation primitive */
    P_pPrivKey->mod_len = P_ModulusLen;       /* Modulus size */
    P_pPrivKey->fields.crt.dp = P_pExpP;      /* dp parameter */
    P_pPrivKey->fields.crt.dq = P_pExpQ;      /* dq parameter */
    P_pPrivKey->fields.crt.p = P_pP;          /* p parameter */
    P_pPrivKey->fields.crt.q = P_pQ;          /* q parameter */
    P_pPrivKey->fields.crt.iq = P_pIq;        /* iq parameter */

    retval = RSA_SUCCESS;
  }

  return retval;
}

/* Verify a message signature using PKCS#1 v1.5 */
rsa_error_t RSA_PKCS1v15_Verify(const rsa_key_t       *P_pPubKey,
                                const uint8_t         *P_pInput,
                                size_t                P_InputLen,
                                const rsa_pkcs_hash_t *P_pHashId,
                                const uint8_t         *P_pSignature,
                                size_t                P_SignatureLen)
{
  size_t tLen, psLen, i;
  rsa_error_t retval;
  uint8_t pEM[BITS_TO_BYTES(RSA_SUPPORT_MAX_SIZE)], pEMfromSign[BITS_TO_BYTES(RSA_SUPPORT_MAX_SIZE)];
  __IO size_t modlen;  /* Use __IO mode variable to ensure reading this variable value each time it is read */

  /* Input Parameters Checking */
  if ((P_pPubKey == NULL) || (P_pPubKey->f == NULL) || (P_pInput == NULL) || (P_InputLen == 0u) || (P_pHashId == NULL))
  {
    return RSA_ERR_BAD_PARAMETER;
  }

  modlen = P_pPubKey->mod_len;   /* Modulus size, determine processing size */

  /* Check input signature */
  if ((P_pSignature == NULL) || (P_SignatureLen == 0u))
  {
    return RSA_ERR_INVALID_SIGNATURE;
  }
  if (P_SignatureLen != modlen)
  {
    return RSA_ERR_INVALID_SIGNATURE;
  }

  /* Call modular exponentiation routine */
  retval = P_pPubKey->f(P_pPubKey, P_pSignature, pEMfromSign);
  if (retval != RSA_SUCCESS)
  {
    return retval;
  }

  /******************** Start PKCS#1 v1.5 Encoding ********************/

  /* Evaluate Lengths */
  tLen = P_pHashId->id_len + P_InputLen;
  psLen = modlen - tLen - 3u;

  /* If P_pPubKey->mModulusSize < tLen + 11 then return RSA modulus too short */
  if (modlen < (tLen + 11u))
  {
    return RSA_ERR_MODULUS_TOO_SHORT;
  }
  /* Construct pEM = (0x) 00 01 FF [FF]* 00 HASH_AlgorithmIdentifier aHash */
  pEM[0] = 0x00u;
  pEM[1] = 0x01u;
  for (i = 0; i < psLen; i++)
  {
    pEM[2u + i] = 0xFFu;
  }
  pEM[2u + psLen] = 0x00u;

  for (i = 0; i < P_pHashId->id_len; i++)
  {
    pEM[3u + psLen + i] = P_pHashId->id[i];
  }
  for (i = 0u; i < P_InputLen; i++)
  {
    pEM[3u + psLen + P_pHashId->id_len + i] = P_pInput[i];
  }

  /******************** End of PKCS#1 v1.5 Encoding ********************/

  /* Do comparison */
  for (i = 0; i < modlen; i++)
  {
    /* byte-by-byte comparison */
    if (pEM[i] != pEMfromSign[i])
    {
      /* exit on first failed byte */
      return RSA_ERR_INVALID_SIGNATURE;
    }
  }
  /* check that modlen has not been faulted */
  if (modlen == P_pPubKey->mod_len)
  {
    /* only if i == modlen (that is, all the bytes are equal), return values is RSA_SUCCESS */
    return (rsa_error_t)((i + (size_t)RSA_SUCCESS) - modlen);
  }
  else
  {
    return RSA_ERR_INVALID_SIGNATURE;
  }
}

/* Sign a message using PKCS#1 v1.5 */
rsa_error_t RSA_PKCS1v15_Sign(const rsa_key_t       *P_pPrivKey,
                              const uint8_t         *P_pInput,
                              size_t                P_InputLen,
                              const rsa_pkcs_hash_t *P_pHashId,
                              uint8_t               *P_pSignature)
{
  size_t emLen, tLen, psLen, i;
  rsa_error_t retval;
  uint8_t pEM[BITS_TO_BYTES(RSA_SUPPORT_MAX_SIZE)];

  /* Check input parameters */
  if ((P_pPrivKey == NULL) || (P_pPrivKey->f == NULL) || (P_pInput == NULL) || (P_InputLen == 0u)
      || (P_pHashId == NULL) || (P_pSignature == NULL))
  {
    return RSA_ERR_BAD_PARAMETER;
  }

  /******************** Start PKCS#1 v1.5 Encoding ********************/

  /* Evaluate Lengths */
  tLen = P_pHashId->id_len + P_InputLen;
  emLen = P_pPrivKey->mod_len;
  psLen = emLen - tLen - 3u;

  /* If emLen < tLen + 11 then return RSA modulus too short */
  if (emLen < (tLen + 11u))
  {
    return RSA_ERR_MODULUS_TOO_SHORT;
  }

  /* Construct pEM = (0x) 00 01 FF [FF]* 00 Hash_AlgorithmIdentifier aHash */
  pEM[0] = 0x00u;
  pEM[1] = 0x01u;
  for (i = 0; i < psLen; i++)
  {
    pEM[2u + i] = 0xFFu;
  }
  pEM[2u + i] = 0x00u;

  for (i = 0u; i < P_pHashId->id_len; i++)
  {
    pEM[3u + psLen + i] = P_pHashId->id[i];
  }
  for (i = 0u; i < P_InputLen; i++)
  {
    pEM[3u + psLen + P_pHashId->id_len + i] = P_pInput[i];
  }

  /******************** End of PKCS#1 v1.5 Encoding ********************/

  /* Call modular exponentiation routine */
  retval = P_pPrivKey->f(P_pPrivKey, pEM, P_pSignature);

  return retval;
}

#endif /* CA_RSA_STM32HAL_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
