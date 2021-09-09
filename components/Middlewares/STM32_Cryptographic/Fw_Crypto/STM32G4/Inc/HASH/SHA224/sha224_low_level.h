/**
  ******************************************************************************
  * @file    sha224_low_level.h
  * @author  MCD Application Team
  * @brief   SHA-224 core functions
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
#ifndef __SHA224_LOW_LEVEL_H__
#define __SHA224_LOW_LEVEL_H__

#ifdef __cplusplus
extern "C"
{
#endif

  /* Need this include for shared Update */

  /* Exported functions ------------------------------------------------------- */
  /* SHA-224 Initialize new context */
  void SHA224Init(SHA224ctx_stt* P_pSHA224ctx);
  /* SHA-224 finalization function */
  void SHA224Final(SHA224ctx_stt* P_pSHA224ctx, \
                   uint8_t *P_pDigest);

#ifdef __cplusplus
}
#endif

#endif  /*__SHA224_LOW_LEVEL_H__*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
