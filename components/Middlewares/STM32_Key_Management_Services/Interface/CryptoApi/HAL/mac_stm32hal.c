/**
  *******************************************************************************
  * @file    mac_stm32hal.c
  * @author  ST
  * @version V1.0.0
  * @date    12-February-2020
  * @brief   Computes AES-CMAC using the STM32 Crypto Core
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
  *******************************************************************************
  */

/* CA sources are built by building ca_core.c giving it the proper ca_config.h */
/* This file can not be build alone                                            */
#if defined(CA_CORE_C)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_MAC_STM32HAL_C
#define CA_MAC_STM32HAL_C

#include "HAL/mac.h"

#include <stdint.h>


/**
  * @brief  This macro returns the least significant byte from an uint32_t
  * @param  P_word The 32-bit from which the least significant byte is taken
  * @retval Least significant byte of P_word
  */
#define BYTE_3(P_word) (uint8_t) ((P_word) & 0xFFu)

/**
  * @brief  This macro returns the second least significant byte from an uint32_t
  * @param  P_word The 32-bit from which the second least significant byte is taken
  * @retval Second least significant byte of P_word
  */
#define BYTE_2(P_word) (uint8_t) (((P_word) >> 8) & 0xFFu)

/**
  * @brief  This macro returns the second most significant byte from an uint32_t
  * @param  P_word The 32-bit from which the second most significant byte is taken
  * @retval Second most significant byte of P_word
  */
#define BYTE_1(P_word) (uint8_t) (((P_word) >> 16) & 0xFFu)

/**
  * @brief  This macro returns the most significant byte from an uint32_t
  * @param  P_word The 32-bit from which the most significant byte is taken
  * @retval Most significant byte of x
  */
#define BYTE_0(P_word) (uint8_t) (((P_word) >> 24) & 0xFFu)

/**
  * @brief  This macro returns a byte within a word
  * @param   P_word The 32-bit from which the byte
  * @param   P_n The INDEX of the byte to be taken, 0 = MSB, 3 = LSB
  * @retval  Selected P_n byte from P_word
  */
#define BYTE_X(P_word, P_n) (uint8_t) (((P_word) >> (24u - (8u * (P_n)))) & 0xFFU)


/**
  * @brief   This macro outputs the 4 octects that form the input 32bit integer
  * @param   [in]    P_x The input 32bit integer
  * @param   [out]   P_a The least significant byte of P_x
  * @param   [out]   P_b The second least significant byte of P_x
  * @param   [out]   P_c The second most significant byte of P_x
  * @param   P_d The most significant byte of P_x
  * @retval  none
  */
#define WORD_TO_BYTES(P_x,P_a,P_b,P_c,P_d)                   \
  do                                                         \
  {                                                          \
    (P_a) = (uint8_t)BYTE_3(P_x);                            \
    (P_b) = (uint8_t)BYTE_2(P_x);                            \
    (P_c) = (uint8_t)BYTE_1(P_x);                            \
    (P_d) = (uint8_t)BYTE_0(P_x);                            \
  } while(0)

/**
  * @brief   This macro returns an integer from 4 octects
  * @param   P_b0 The most significant byte of the resulting integer
  * @param   P_b1 The second most byte of the resulting integer
  * @param   P_b2 The second least byte of the resulting integer
  * @param   P_b3 The least byte of the resulting integer
  * @retval  The resulting 32bit integer formed by P_b0 || P_b1 || P_b2 || P_b3
  */
#define BYTES_TO_WORD(P_b0, i)                             \
  (uint32_t) (((uint32_t)((P_b0)[i]) << 24)                \
              | ((uint32_t)((P_b0)[(i) + 1u]) << 16)       \
              | ((uint32_t)((P_b0)[(i) + 2u]) << 8)        \
              | (((P_b0)[(i) + 3u])))

#define AES_BLOCK_SIZE  16u /*!< Size in bytes of an AES block */

/**
  * @brief   Perform an AES encryption with the indicated mode
  *
  * @param   hcryp The handler of the AES peripheral
  * @param   input Buffer containing the input to cipher
  * @param   inputSize Size in bytes of the input to cipher
  * @param   key Buffer containing the key
  * @param   iv Buffer containing the initialization vector
  * @param   output Buffer where the ciphertext will be stored
  *
  * @retval  None
  */
static mac_error_t AES_Encrypt(CRYP_HandleTypeDef *hcryp,
                               uint32_t *input,
                               uint32_t inputSize,
                               uint32_t *key,
                               uint32_t *iv,
                               uint32_t *output)
{
  mac_error_t retval = MAC_SUCCESS;

  HAL_StatusTypeDef periph_retval;

  hcryp->Init.pKey = key;
  hcryp->Init.pInitVect = iv;

  periph_retval = HAL_CRYP_Encrypt(hcryp, input, (uint16_t)inputSize,
                                   output, TIMEOUT_VALUE);
  if (periph_retval != HAL_OK)
  {
    retval = MAC_ERROR_HW_FAILURE;
  }

  return retval;
}

/**
  * @brief   Convert a block of bytes in a block of words
  *
  * @param   input_start Buffer of bytes containing the input
  * @param   num_els Number of bytes in the buffer
  * @param   load_buffer Buffer of words where the converted input
  *          will be stored
  *
  * @retval  None
  */
static void load_block(const uint8_t *input_start,
                       uint32_t num_els,
                       uint32_t *load_buffer)
{
  uint32_t i;
  uint32_t j;
  uint32_t full_words = num_els / 4u;

  uint32_t remaining_bytes = num_els % 4u;
  uint32_t temp_word;

  /* Convert any 4 bytes in one 32 bits word */
  for (i = 0; i < full_words; i++)
  {
    load_buffer[i] = BYTES_TO_WORD(input_start, 4u * i);
  }

  /* If the block is not multiple of 16 bytes, pad the buffer */
  if ((num_els == 0u) || (full_words < 4u))
  {
    /* last block: 0x [remaining data] 80 00 00 .. 00 */
    temp_word = 0x80u;

    /* Insert the 0x80 byte in the correct position */
    load_buffer[full_words] = temp_word << (24u - (8u * remaining_bytes));

    /* Include the remaining bytes of data */
    for (j = 0u; j < remaining_bytes; j++)
    {
      load_buffer[full_words]
      |= (uint32_t)(input_start[(4u * full_words) + j]) << (24u - (8u * j));
    }

    /* Fill the rest of the block with zeros */
    for (i = full_words + 1u; i < 4u; i++)
    {
      load_buffer[i] = 0u;
    }
  }
}

/**
  * @brief   Derive the K2 subkey
  *
  * @param   sub_key1 Buffer containing the K1 subkey computed before
  * @param   output_buffer Buffer that will contain the computed K2 subkey
  *
  * @retval  None
  */
static void derive_subkey2(const uint32_t *sub_key1, uint32_t *output_buffer)
{
  uint32_t carry = ((sub_key1[0] >> 31) & 1u) * 0x00000087u;

  output_buffer[0] = ((sub_key1[0] << 1) | (sub_key1[1] >> 31));
  output_buffer[1] = ((sub_key1[1] << 1) | (sub_key1[2] >> 31));
  output_buffer[2] = ((sub_key1[2] << 1) | (sub_key1[3] >> 31));
  output_buffer[3] = (sub_key1[3] << 1) ^ carry;

}

/**
  * @brief   Derive the K1 subkey
  *
  * @param   hcryp Handler of the AES peripheral
  * @param   encryption_key Buffer containing the AES key
  * @param   output_buffer Buffer that will contain the computed K1 subkey
  *
  * @retval  None
  */
static mac_error_t derive_subkey1(CRYP_HandleTypeDef *hcryp,
                                  uint32_t *encryption_key,
                                  uint32_t *output_buffer)
{
  mac_error_t retval;

  retval = AES_Encrypt(hcryp, output_buffer, 4u,
                       encryption_key, output_buffer, output_buffer);

  if (retval == MAC_SUCCESS)
  {
    derive_subkey2(output_buffer, output_buffer);
  }

  return retval;
}

/**
  * @brief   Xor a key into a buffer
  *
  * @param   block Buffer containing the block that will be updated with
  *                the xored key
  * @param   key Buffer containing the key
  *
  * @retval  None
  */
static void xor_key(uint32_t *block, const uint32_t *key)
{
  uint32_t i;

  for (i = 0u; i < 4u; i++)
  {
    block[i] ^= key[i];
  }
}

/**
  * @brief Compute the CMAC.
  *
  * @param[in]  inputData       Buffer containing the data.
  * @param[in]  inputDataLength Length in bytes of the data.
  * @param[in]  key             Buffer containing the MAC key.
  * @param[in]  keySize         Size of the MAC key in bytes. The only supported
  *                             sizes are 16 bytes (AES-128 CMAC) and 32 bytes
  *                             (AES-256 CMAC)
  * @param[in]  macSize         Desired size in bytes of the MAC.
  *                             Must be greater than 0 and less or equal to 16.
  * @param[out] macBuff         Buffer where the computed MAC will be stored.
  *
  * @return     mac_error_t
  */
mac_error_t CMAC_compute(const uint8_t *inputData,
                         uint32_t inputDataLength,
                         const uint8_t *key,
                         uint32_t keySize,
                         uint32_t macSize,
                         uint8_t *macBuff)
{
  mac_error_t retval;

  /* Internal variables, buffers */
  uint32_t temp_iv[4] = {0,};
  uint32_t temp_key[8] = {0,};
  uint32_t temp_buffer[4u * MAX_PROCESSED_BLOCKS];

  uint32_t k1[4] = {0,};
  uint32_t k2[4] = {0,};

  uint32_t num_blocks;
  uint32_t remaining_bytes;

  uint32_t i;
  uint32_t j;
  uint32_t block_size;
  uint32_t processed_block = 0u;

  uint32_t encrypt = 0u;

  CRYP_HandleTypeDef hcryp;
  HAL_StatusTypeDef periph_retval;

  /* Check that pointers to buffers are not null */
  if ((key == NULL) || (macBuff == NULL))
  {
    return MAC_ERROR_BAD_PARAMETER;
  }

  /* Plaintext buffer can be null only when there is no data to authenticate */
  if ((inputData == NULL) && (inputDataLength > 0u))
  {
    return MAC_ERROR_BAD_PARAMETER;
  }

  /* check that the requested mac size is less or equal than the block size */
  if (macSize > AES_BLOCK_SIZE)
  {
    return MAC_ERROR_WRONG_MAC_SIZE;
  }
  /* check that the requested mac size is greater than 0 */
  if (macSize == 0u)
  {
    return MAC_ERROR_WRONG_MAC_SIZE;
  }

  /* Initialize the AES peripheral */
  (void)memset((uint8_t *)&hcryp, 0, sizeof(CRYP_HandleTypeDef));

  /* check that the provided key size is acceptable */
  switch (keySize)
  {
    case AES128_KEY:
      hcryp.Init.KeySize = CRYP_KEYSIZE_128B;
      break;
    case AES256_KEY:
      hcryp.Init.KeySize = CRYP_KEYSIZE_256B;
      break;
    default:
      return MAC_ERROR_UNSUPPORTED_KEY_SIZE;
      break;
  }

  /* Complete the configuration of the AES peripheral */
  hcryp.Instance = CA_AES_INSTANCE;
  hcryp.Init.DataType = CRYP_DATATYPE_32B;
  hcryp.Init.Algorithm = CRYP_AES_CBC;
  hcryp.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_WORD;

  /* Initialize the CRYP peripheral */
  periph_retval = HAL_CRYP_Init(&hcryp);
  if (periph_retval != HAL_OK)
  {
    return MAC_ERROR_HW_FAILURE;
  }

  /* Generate the subkeys K1 and K2 */
  load_block(key, keySize, temp_key);
  retval = derive_subkey1(&hcryp, temp_key, k1);
  if (retval != MAC_SUCCESS)
  {
    return retval;
  }
  derive_subkey2(k1, k2);

  /* get the number of AES blocks included into the input */
  num_blocks = inputDataLength / AES_BLOCK_SIZE;
  remaining_bytes = inputDataLength % AES_BLOCK_SIZE;
  if (remaining_bytes > 0u)
  {
    num_blocks++;
  }

  /* Transform the number of blocks in number of bytes */
  num_blocks *= AES_BLOCK_SIZE;

  i = 0u;

  do
  {
    if ((inputDataLength - i) < AES_BLOCK_SIZE)
    {
      /* If the remaining data size is less than the AES block size, it means that
       * it is the last block to load and it is possible to do the encryption */
      block_size = inputDataLength - i;
      encrypt = 1u;
    }
    else
    {
      block_size = AES_BLOCK_SIZE;
    }

    load_block(&inputData[i], block_size, &temp_buffer[processed_block * 4u]);

    /* If needed, include the subkey into the block of data to process */
    if (block_size < AES_BLOCK_SIZE)
    {
      xor_key(&temp_buffer[processed_block * 4u], k2);
    }
    else if ((inputDataLength - i) == AES_BLOCK_SIZE)
    {
      xor_key(&temp_buffer[processed_block * 4u], k1);
      encrypt = 1u;
    }
    else
    {
      /* do nothing */
    }

    /* If the buffer is full, let's encrypt it */
    if (processed_block == (MAX_PROCESSED_BLOCKS - 1u))
    {
      encrypt = 1u;
    }
    else
    {
      /* do nothing */
    }

    if (encrypt == 0u)
    {
      processed_block++;
    }
    else
    {
      /* Encrypt the loaded blocks and overwrite it with the ciphertext */
      retval = AES_Encrypt(&hcryp, temp_buffer, 4u * (processed_block + 1u),
                           temp_key, temp_iv, temp_buffer);
      if (retval != MAC_SUCCESS)
      {
        return retval;
      }

      /* Copy last processed block into the IV for the next iteration */
      for (j = 0; j < 4u; j++)
      {
        temp_iv[j] = temp_buffer[(processed_block * 4u) + j];
      }

      encrypt = 0u;
      processed_block = 0u;
    }

    /* Ready for the next iteration */
    i += AES_BLOCK_SIZE;

  } while (i < num_blocks);

  /* The CRYP peripheral isno more needed. Let's deinit it */
  periph_retval = HAL_CRYP_DeInit(&hcryp);
  if (periph_retval != HAL_OK)
  {
    return MAC_ERROR_HW_FAILURE;
  }

  /* Extract the MAC */
  for (i = 0u; i < (macSize / 4u); i++)
  {
    WORD_TO_BYTES(temp_iv[i], macBuff[(4u * i) + 3u], macBuff[(4u * i) + 2u],
                  macBuff[(4u * i) + 1u], macBuff[(4u * i) + 0u]);
  }

  if (macSize < 16u)
  {
    /* copy remaining bytes */
    for (i = 0; i < (macSize % 4u); i++)
    {
      macBuff[((macSize / 4u) * 4u) + i] = BYTE_X(temp_iv[macSize / 4u], i);
    }
  }

  return retval;
}

#endif /* CA_MAC_STM32HAL_C */

#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
