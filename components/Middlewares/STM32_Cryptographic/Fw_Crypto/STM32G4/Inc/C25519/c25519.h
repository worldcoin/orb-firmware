/**
  ******************************************************************************
  * @file    c5519.h
  * @author  MCD Application Team
  * @brief   Container for ed25519 functionalities
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
#ifndef __C25519_H__
#define __C25519_H__

#ifdef __cplusplus
extern "C"
{
#endif

  /** @addtogroup C25519
  * @{
  */
  /* Includes ------------------------------------------------------------------*/
#include <stdint.h>
  /* Exported types ------------------------------------------------------------*/
  /* Exported constants --------------------------------------------------------*/
  /* Exported macro ------------------------------------------------------------*/
  /* Exported functions ------------------------------------------------------- */
  int32_t C25519keyGen      (uint8_t *P_pPrivateKey, \
                             uint8_t *P_pPublicKey);
  \

  int32_t C25519keyExchange (uint8_t *P_pSharedSecret,    \
                             const uint8_t *P_pPrivateKey, \
                             const uint8_t *P_pPublicKey);
  /**
  * @}
  */
  /* Lower level functionality. Useful for testing, might be used in real world if there is small NVM */


#ifdef __cplusplus
}
#endif

#endif /* __C25519_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
