/**
  ******************************************************************************
  * @file    ca_low_level_template.c
  * @author  MCD Application Team
  * @brief   This file contains the low level implementations of the Cryptographic API (CA) module.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
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
#ifndef CA_LOW_LEVEL_C
#define CA_LOW_LEVEL_C

/* Includes ------------------------------------------------------------------*/
#include "CryptoApi/ca.h"
#if defined(CA_MBED_CRYPTOLIB_SUPP)
#if defined(CA_USES_PSA_CRYPTO) && defined(HAL_RNG_MODULE_ENABLED)
#include "mbedtls/entropy_poll.h"
#endif /* CA_USES_PSA_CRYPTO && HAL_RNG_MODULE_ENABLED */
#endif /* CA_MBED_CRYPTOLIB_SUPP */

/* Private defines -----------------------------------------------------------*/
#if defined(CA_MBED_CRYPTOLIB_SUPP)
#if defined(CA_USES_PSA_CRYPTO) && defined(HAL_RNG_MODULE_ENABLED)
#define COMPILER_BARRIER() __DMB();__DSB();
#endif /* CA_USES_PSA_CRYPTO && HAL_RNG_MODULE_ENABLED */
#endif /* CA_MBED_CRYPTOLIB_SUPP */
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#if defined(CA_MBED_CRYPTOLIB_SUPP)
#if defined(CA_USES_PSA_CRYPTO) && defined(HAL_RNG_MODULE_ENABLED)
static RNG_HandleTypeDef handle;
static uint8_t users = 0;
#endif /* CA_USES_PSA_CRYPTO && HAL_RNG_MODULE_ENABLED */
#endif /* CA_MBED_CRYPTOLIB_SUPP */
/* Private function prototypes -----------------------------------------------*/
#if defined(CA_MBED_CRYPTOLIB_SUPP)
#if defined(CA_USES_PSA_CRYPTO) && defined(HAL_RNG_MODULE_ENABLED)
static uint8_t atomic_incr_u8(__IO uint8_t *valuePtr, uint8_t delta);
static void RNG_Init(void);
static void RNG_GetBytes(uint8_t *output, size_t length, size_t *output_length);
static void RNG_DeInit(void);
#endif /* CA_USES_PSA_CRYPTO && HAL_RNG_MODULE_ENABLED */
#endif /* CA_MBED_CRYPTOLIB_SUPP */

/* Exported functions implementation -----------------------------------------*/

#if defined(CA_HAL_CRYPTOLIB_SUPP)

#if ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_HAL) || ((CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_HAL) || ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_HAL) || ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_HAL) || ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_HAL)

/**
  * @brief CRYP MSP Initialization
  * This function configures the hardware resources
  * @param hcryp: CRYP handle pointer
  * @retval None
  */
void HAL_CRYP_MspInit(CRYP_HandleTypeDef *hcryp)
{
  if (hcryp->Instance == CA_AES_INSTANCE)
  {
    /* Release AES/CRYP from reset state */
    __HAL_RCC_AESx_RELEASE_RESET();
    /* Peripheral clock enable */
    __HAL_RCC_AESx_CLK_ENABLE();
  }
}

/**
  * @brief CRYP MSP De-Initialization
  * This function freeze the hardware resources
  * @param hcryp: CRYP handle pointer
  * @retval None
  */
void HAL_CRYP_MspDeInit(CRYP_HandleTypeDef *hcryp)
{
  if (hcryp->Instance == CA_AES_INSTANCE)
  {
    /* Peripheral clock disable */
    __HAL_RCC_AESx_CLK_DISABLE();
    /* Force AES/CRYP into reset state */
    __HAL_RCC_AESx_FORCE_RESET();
  }
}
#endif /* CA_ROUTE_AES_CBC || CA_ROUTE_AES_CCM || CA_ROUTE_AES_CMAC || CA_ROUTE_AES_ECB || CA_ROUTE_AES_GCM => CA_ROUTE_HAL */


#if ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_HAL) || ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_HAL)
/**
  * @brief      PKA MSP Initialization
  *             This function configures the hardware resources used in this example
  * @param[in] hpka: PKA handle pointer
  * @retval None
  */
void HAL_PKA_MspInit(PKA_HandleTypeDef *hpka)
{
  if (hpka->Instance == CA_PKA_INSTANCE)
  {
    /* Release PKA from reset state */
    __HAL_RCC_PKAx_RELEASE_RESET();
    /* Peripheral clock enable */
    __HAL_RCC_PKAx_CLK_ENABLE();
  }
}

/**
  * @brief      PKA MSP De-Initialization
  *             This function freeze the hardware resources used in this example
  * @param[in] hpka: PKA handle pointer
  * @retval None
  */
void HAL_PKA_MspDeInit(PKA_HandleTypeDef *hpka)
{
  if (hpka->Instance == CA_PKA_INSTANCE)
  {
    /* Peripheral clock disable */
    __HAL_RCC_PKAx_CLK_DISABLE();
    /* Force PKA into reset state */
    __HAL_RCC_PKAx_FORCE_RESET();
  }
}
#endif /* CA_ROUTE_ECC_ECDSA || CA_ROUTE_RSA => CA_ROUTE_HAL */

#endif /* CA_HAL_CRYPTOLIB_SUPP */

#if defined(CA_MBED_CRYPTOLIB_SUPP)
#if defined(CA_USES_PSA_CRYPTO) && defined(HAL_RNG_MODULE_ENABLED)
static uint8_t atomic_incr_u8(__IO uint8_t *valuePtr, uint8_t delta)
{
  COMPILER_BARRIER();
  uint8_t newValue;
  do
  {
    newValue = __LDREXB(valuePtr) + delta;
  } while (__STREXB(newValue, valuePtr));
  COMPILER_BARRIER();
  return newValue;
}

static void RNG_Init(void)
{
  uint32_t dummy;

  /*  We're only supporting a single user of RNG */
  if (atomic_incr_u8(&users, 1) > 1)
  {
    while (1);
  }

  /* Select MSI as RNG clock source */
  __HAL_RCC_RNG_CONFIG(RCC_PERIPHCLK_RNG);

  /* RNG Peripheral clock enable */
  __HAL_RCC_RNG_CLK_ENABLE();

  /* Initialize RNG instance */
  handle.Instance = RNG;
  handle.State = HAL_RNG_STATE_RESET;
  handle.Lock = HAL_UNLOCKED;

  HAL_RNG_Init(&handle);

  /* first random number generated after setting the RNGEN bit should not be used */
  HAL_RNG_GenerateRandomNumber(&handle, &dummy);
}

static void RNG_GetBytes(uint8_t *output, size_t length, size_t *output_length)
{
  int32_t ret = 0;
  __IO uint8_t random[4];
  *output_length = 0;

  /* Get Random byte */
  while ((*output_length < length) && (ret == 0))
  {
    if (HAL_RNG_GenerateRandomNumber(&handle, (uint32_t *)random) != HAL_OK)
    {
      ret = -1;
    }
    else
    {
      for (uint8_t i = 0; (i < 4) && (*output_length < length) ; i++)
      {
        *output++ = random[i];
        *output_length += 1;
        random[i] = 0;
      }
    }
  }
  /* Just be extra sure that we didn't do it wrong */
  if ((__HAL_RNG_GET_FLAG(&handle, (RNG_FLAG_CECS | RNG_FLAG_SECS))) != 0)
  {
    *output_length = 0;
  }
}

static void RNG_DeInit(void)
{
  /*Disable the RNG peripheral */
  HAL_RNG_DeInit(&handle);
  /* RNG Peripheral clock disable - assume we're the only users of RNG  */
  __HAL_RCC_RNG_CLK_DISABLE();

  users = 0;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
  RNG_Init();
  RNG_GetBytes(output, len, olen);
  RNG_DeInit();
  if (*olen != len)
  {
    return -1;
  }
  return 0;
}
#endif /* CA_USES_PSA_CRYPTO && HAL_RNG_MODULE_ENABLED */
#endif /* CA_MBED_CRYPTOLIB_SUPP */

#if defined(CA_ST_CRYPTOLIB_SUPP)
/**
  * @brief      CRC initialization
  *             This function release reset and clock CRC IP peripheral
  * @retval None
  */
void CA_LL_CRC_Init(void)
{
  /* Force CRC into reset state */
  __HAL_RCC_CRC_FORCE_RESET();
  /* Release CRC from reset state */
  __HAL_RCC_CRC_RELEASE_RESET();
  /* Peripheral clock enable */
  __HAL_RCC_CRC_CLK_ENABLE();
}

#endif /* CA_ST_CRYPTOLIB_SUPP */

#endif /* CA_LOW_LEVEL_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
