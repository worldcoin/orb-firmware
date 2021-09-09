/**
  ******************************************************************************
  * @file    ca_if_utils.h
  * @author  MCD Application Team
  * @brief   This file contains the definitions of the Cryptographic API (CA)
  *          interface utilities.
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
#ifndef CA_IF_UTILS_H
#define CA_IF_UTILS_H

/* Includes ------------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
#define WRAP_SUCCESS 0U                 /*!< Return's Type on Success*/
#define WRAP_FAILURE 1U                 /*!< Return's Type on Failure*/
#define WRAP_BAD_KEY 2U                 /*!< Return's Type with a Bad key*/

/* Exported prototypes -------------------------------------------------------*/

uint32_t wrap_allocate_memory(uint32_t size, CA_membuf_stt *P_pMemBuf);
uint8_t wrap_uint8_to_BigNum(BigNum_stt *P_pBigNum, const uint8_t *P_pArray, int32_t Psize);
uint8_t wrap_BigNum_to_uint8(uint8_t *P_pArray, const BigNum_stt *P_pBigNum, int32_t *P_psize);


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_IF_UTILS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

