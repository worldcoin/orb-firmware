/**
  ******************************************************************************
  * @file    ca_if_utils.c
  * @author  MCD Application Team
  * @brief   This file contains the Cryptographic API (CA) interface utilities.
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

/* CA sources are built by building ca_core.c giving it the proper ca_config.h */
/* This file can not be build alone                                            */
#if defined(CA_CORE_C)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_IF_UTILS_C
#define CA_IF_UTILS_C

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/**
  * @brief      Function to reserve a memory area into the given memory buffer
  * @param[in]  size Size of area to reserved
  * @param[in]  P_pMemBuf Buffer in which area must be reserved
  * @retval     Area pointer if success, NULL otherwise
  */
uint32_t wrap_allocate_memory(uint32_t size, CA_membuf_stt *P_pMemBuf)
{
  uint32_t ptr;
  /* Check memory buffer exists and available size is enough */
  if ((P_pMemBuf == NULL) || ((P_pMemBuf->mSize - P_pMemBuf->mUsed) < (uint16_t)size))
  {
    return (uint32_t)NULL;
  }
  /* Return current buffer pointer */
  ptr = (((uint32_t)(P_pMemBuf->pmBuf)) + P_pMemBuf->mUsed);
  /* Remove the allocated bytes from the available ones */
  P_pMemBuf->mUsed += (uint16_t)size;
  return ptr;
}

/**
  * @brief      Function to pass an uint8_t type array to a Bignum type
  * @param[out] *P_pBigNum: Output data
  * @param[in]  *P_pArray: Data that will be transform
  * @param[in]  Psize: Size of the Array in bytes
  * @retval     WRAP_SUCCESS: On success
  * @reval      WRAP_FAILURE: An error occur
  */
uint8_t wrap_uint8_to_BigNum(CA_BigNum_stt *P_pBigNum, const uint8_t *P_pArray, int32_t Psize)
{
  uint8_t i;
  uint8_t j;
  uint32_t size_tp;
  uint32_t size_counter = 0;
  uint32_t psize = (uint32_t)Psize;

  /* Check parameters */
  if ((P_pBigNum == NULL)
      || (P_pArray == NULL))
  {
    return WRAP_FAILURE;
  }
  if (Psize <= 0)
  {
    return WRAP_FAILURE;
  }

  if (P_pBigNum->pmDigit == NULL)
  {
    return WRAP_FAILURE;
  }
  else
  {
    /* Compute size of u32 buffer to host u8 buffer.
     * Taking into account if bytes buffer length is or not a multiple of 4 */
    if ((psize % 4UL) > 0UL)
    {
      size_tp = (psize / 4UL) + 1UL;
    }
    else
    {
      size_tp = psize / 4UL;
    }
    /*Set to zero*/
    (void)memset(P_pBigNum->pmDigit, 0, size_tp * 4U);
    /* Fill the array */
    /* Loop on u32 buffer */
    for (i = 0U; i < size_tp; i++)
    {
      /* Loop on each u32 byte */
      for (j = 0U; j < 4U; j++)
      {
        size_counter ++;
        if (size_counter <= psize)
        {
          /* Add to u32 the u8 with correct shift */
          P_pBigNum->pmDigit[size_tp - i - 1U] += (uint32_t)(P_pArray[(4U * i) + j]) << (8U * (3U - j));
        }
        else
        {
          /* When there is not enough u8 to fill all the u32, pad with previous u8 */
          P_pBigNum->pmDigit[size_tp - i - 1U] = P_pBigNum->pmDigit[size_tp - i - 1U] >> 8U;
        }
      }
    }
    /*Fill the rest of the structure*/
    P_pBigNum->mNumDigits = (uint16_t) size_tp;
    P_pBigNum->mSignFlag = CA_SIGN_POSITIVE;
  }
  return WRAP_SUCCESS;
}

/**
  * @brief      Function to pass an BigNum type to a uint8_t type array
  * @param[out]  *P_pArray: Output data
  * @param[in] *P_pBigNum: Data that will be transform
  * @param[out]  *Psize: Size of the Array in bytes
  * @retval     WRAP_SUCCESS: On success
  * @reval      WRAP_FAILURE: An error occurs
  */
uint8_t wrap_BigNum_to_uint8(uint8_t *P_pArray,
                             const CA_BigNum_stt *P_pBigNum,
                             int32_t *P_psize)
{
  uint16_t i;
  uint16_t j;
  /* Check parameters */
  if ((P_pBigNum == NULL)
      || (P_pArray == NULL))
  {
    return WRAP_FAILURE;
  }

  /* Fill the array */
  /* Loop on u32 buffer */
  for (i = 0U; i < P_pBigNum->mNumDigits; i++)
  {
    /* Loop on each u32 byte */
    for (j = 4U; j > 0U; j--)
    {
      P_pArray[(P_pBigNum->mNumDigits * 4U) - (i * 4U) - j] = (uint8_t)(P_pBigNum->pmDigit[i] >> ((j - 1U) * 8U));
    }
  }
  if (P_psize != NULL)
  {
    *P_psize = ((int32_t)(P_pBigNum->mNumDigits) * 4); /*uint32 -> uint8*/
  }

  return WRAP_SUCCESS;
}
#endif /* CA_IF_UTILS_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
