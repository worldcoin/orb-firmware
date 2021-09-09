/**
  ******************************************************************************
  * @file    hmac_sha256.h
  * @author  MCD Application Team
  * @brief   Provides HMAC-SHA256 functions
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
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
#ifndef __CRL_HMAC_SHA256_H__
#define __CRL_HMAC_SHA256_H__

#ifdef __cplusplus
extern "C"
{
#endif

  /* Includes ------------------------------------------------------------------*/

  /** @ingroup HMAC_SHA256
    * @{
    */

  /**
    * @brief  HMAC-SHA-256 Context Structure
    */
  /* Exported types ------------------------------------------------------------*/
  typedef HMACctx_stt HMAC_SHA256ctx_stt;

  /* Exported constants --------------------------------------------------------*/
  /* Exported macro ------------------------------------------------------------*/
  /* Exported functions ------------------------------------------------------- */

  int32_t HMAC_SHA256_Init   (HMAC_SHA256ctx_stt *P_pHMAC_SHA256ctx);

  int32_t HMAC_SHA256_Append (HMAC_SHA256ctx_stt *P_pHMAC_SHA256ctx, \
                              const uint8_t *P_pInputBuffer,        \
                              int32_t P_inputSize);

  int32_t HMAC_SHA256_Finish (HMAC_SHA256ctx_stt *P_pHMAC_SHA256ctx, \
                              uint8_t *P_pOutputBuffer,             \
                              int32_t *P_pOutputSize);

  /**
    * @}
    */


#ifdef __cplusplus
}
#endif

#endif   /* __CRL_HMAC_SHA256_H__ */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
