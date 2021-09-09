/**
  ******************************************************************************
  * @file    se_key.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Engine KEY module
  *          functionalities.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SE_KEY_H
#define SE_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/** @addtogroup  SE Secure Engine
  * @{
  */

/** @addtogroup SE_KEY SE Key
  * @{
  */


/** @addtogroup SE_KEY_Exported_Functions
  * @brief Functions SE_CoreBin can use to retrieve the keys.
  *        These functions are implemented in se_key.s
  * @note Depending on the crypto scheme, @ref SE_ReadKey or @ref SE_ReadKey_Pub can be useless.
  *       Nevertheless, we do not use compiler switches as the linker will remove the unused function(s).
  * @{
  */
/**
  * @brief Function to retrieve the symmetric key.
  *        One specific key per FW image.
  * @param pKey: pointer to an array of uint8_t with the appropriate size
  * @retval void
  */
void SE_ReadKey_1(uint8_t *pKey);
void SE_ReadKey_2(uint8_t *pKey);
void SE_ReadKey_3(uint8_t *pKey);
/**
  * @brief Function to retrieve the public asymmetric key.
  * @param pPubKey: pointer to an array of uint8_t with the appropriate size
  * @retval void
  */
void SE_ReadKey_1_Pub(uint8_t *pPubKey);
void SE_ReadKey_2_Pub(uint8_t *pPubKey);
void SE_ReadKey_3_Pub(uint8_t *pPubKey);
/**
  * @brief Function to retrieve the external token pairing keys.
  * @param pPairingKey: pointer to an array of uint8_t with the appropriate size (32 bytes)
  * @retval void
  */
void SE_ReadKey_Pairing(uint8_t *pPairingKey);
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

#endif /* SE_KEY_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

