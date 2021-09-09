/**
  ******************************************************************************
  * @file    ca_core.c
  * @author  MCD Application Team
  * @brief   This file constitutes the Cryptographic API (CA) module sources
  *          as its inclusion allows based on the configuration to build every
  *          needed other c files contents
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

/* CA sources are built by building ca_core.c giving it the proper ca_config.h    */
/* This define allows to indicates included files that ca_core.c build is ongoing */
#define CA_CORE_C

#include "kms.h"
#if defined(KMS_ENABLED)

#include "ca.h"

#include "ca_if_utils.c"

#if defined(CA_FEAT_RNG)
#include "ca_rng.c"
#endif /* CA_FEAT_RNG */

#if defined(CA_FEAT_AES)
#include "ca_aes.c"
#endif /* CA_FEAT_AES */

#if defined(CA_FEAT_HASH)
#include "ca_hash.c"
#endif /* CA_FEAT_HASH */

#if defined(CA_FEAT_ECC)
#include "ca_ecc.c"
#endif /* CA_FEAT_ECC */

#if defined(CA_FEAT_RSA)
#include "ca_rsa.c"
#endif /* CA_FEAT_RSA */

/**
  * @brief  Initialize the Crypto API core
  * @retval CA_SUCCESS, CA_ERROR
  */
int32_t CA_Init(void)
{
#if !defined(CA_MBED_CRYPTOLIB_SUPP) || !defined(CA_USES_PSA_CRYPTO)
  int32_t ecc_ret_status = CA_SUCCESS;
#else /* !CA_MBED_CRYPTOLIB_SUPP || !CA_USES_PSA_CRYPTO */
  int32_t ecc_ret_status;
#endif /* !CA_MBED_CRYPTOLIB_SUPP || !CA_USES_PSA_CRYPTO */
#if defined(CA_ST_CRYPTOLIB_SUPP)
  CA_LL_CRC_Init();
#endif /* CA_ST_CRYPTOLIB_SUPP */
#if defined(CA_MBED_CRYPTOLIB_SUPP) && defined(CA_USES_PSA_CRYPTO)
  /* Initialize MBED crypto library */
  if (psa_crypto_init() != PSA_SUCCESS)
  {
    ecc_ret_status = CA_ERROR;
  }
  else
  {
    ecc_ret_status = CA_SUCCESS;
  }
#endif /* CA_MBED_CRYPTOLIB_SUPP && CA_USES_PSA_CRYPTO */
  return ecc_ret_status;
}

/**
  * @brief  DeInitialize the Crypto API core
  * @retval CA_SUCCESS, CA_ERROR
  */
int32_t CA_DeInit(void)
{
  int32_t ecc_ret_status = CA_SUCCESS;
#if defined(CA_MBED_CRYPTOLIB_SUPP) && defined(CA_USES_PSA_CRYPTO)
  mbedtls_psa_crypto_free();
#endif /* CA_MBED_CRYPTOLIB_SUPP && CA_USES_PSA_CRYPTO */
  return ecc_ret_status;
}


#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
