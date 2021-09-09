/**
  ******************************************************************************
  * @file    sha512_sha384_transform.h
  * @author  MCD Application Team
  * @brief   SHA-512 Update function
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
#ifndef __SHA512_TRANSFORM_H__
#define __SHA512_TRANSFORM_H__

#ifdef __cplusplus
extern "C"
{
#endif

  /* Exported functions ------------------------------------------------------- */
  /* SHA512 Update */
  void SHA512Update(HASHLctx_stt* P_pSHA512ctx, const uint8_t* P_pInput, uint32_t P_inputSize);

#ifdef __cplusplus
}
#endif

#endif  /*__SHA256_TRANSFORM_H__*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
