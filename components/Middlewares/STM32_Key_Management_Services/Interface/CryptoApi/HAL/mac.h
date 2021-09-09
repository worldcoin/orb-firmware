/**
  *******************************************************************************
  * @file    mac.h
  * @author  ST
  * @version V1.0.0
  * @date    13-February-2020
  * @brief   Prototype for the AES CMAC function
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

#ifndef CMAC_H_
#define CMAC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_PROCESSED_BLOCKS 16u /*!< Maximum AES blocks that the peripheral
                                      will process in a single call */

#define AES128_KEY CA_CRL_AES128_KEY /*!< Size in bytes of an AES128 key */
#define AES256_KEY CA_CRL_AES256_KEY /*!< Size in bytes of an AES256 key */

typedef enum
{
  MAC_SUCCESS = 0,                      /*!< The operation has been successfully
                                             performed */
  MAC_ERROR_BAD_PARAMETER,              /*!< One or more parameter have been
                                             wrongly passed to the function
                                             (e.g. NULL pointers) */
  MAC_ERROR_WRONG_MAC_SIZE,             /*!< The requested size for the MAC is
                                             not supported (e.g. zero or more
                                             than 16 bytes) */
  MAC_ERROR_UNSUPPORTED_KEY_SIZE,       /*!< AES key size not supported (only
                                             AES128 and AES256 are supported) */
  MAC_ERROR_HW_FAILURE,                 /*!< Generic failure on HW peripheral */
} mac_error_t;

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
                         uint8_t *macBuff);

#ifdef __cplusplus
}
#endif

#endif /* CMAC_H_ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
