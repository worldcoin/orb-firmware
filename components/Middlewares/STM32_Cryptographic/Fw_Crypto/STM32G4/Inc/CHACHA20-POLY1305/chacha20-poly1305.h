/**
  ******************************************************************************
  * @file    chacha20-poly1305.h
  * @author  MCD Application Team
  * @brief   Container for chacha20-poly1305 functionalities
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
#ifndef __CHACHA20_POLY1305_H__
#define __CHACHA20_POLY1305_H__

#ifdef __cplusplus
extern "C"
{
#endif

  /** @addtogroup CHACHA20POLY1305
  * @{
  */
  /* Includes ------------------------------------------------------------------*/
#include <stdint.h>
  /* Exported types ------------------------------------------------------------*/

  /**
  * @brief  Structure for ChaCha20-Poly1305 context
  */

  typedef struct
  {
    uint32_t   mContextId;   /*!< Unique ID of this context. \b Not \b used in current implementation. */
    SKflags_et mFlags;       /*!< 32 bit mFlags, used to perform keyschedule and future use */
    const uint8_t *pmKey;    /*!< Pointer to original 32 bytes Key buffer */
    const uint8_t *pmNonce;  /*!< Pointer to original 12 bytes Nonce buffer */
    const uint8_t *pmTag;    /*!< Pointer to Authentication TAG. This value must be set in decryption, and this TAG will be verified */
    uint32_t mAadSize;       /*!< Size of the processed AAD */
    uint32_t mCipherSize;    /*!< Size of the processed CipherText */
    uint32_t r[5];           /*!< Internal: value of r */
    uint32_t h[5];           /*!< Internal: value of h */
    uint32_t pad[4];         /*!< Internal: value of Poly nonce */
    uint32_t amState[16];   /*!< Internal: ChaCha Internal State */
  }
  ChaCha20Poly1305ctx_stt;

  /* Exported constants --------------------------------------------------------*/
  /* Exported macro ------------------------------------------------------------*/
  /* Exported functions ------------------------------------------------------- */

  int32_t ChaCha20Poly1305_Encrypt_Init(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx, const uint8_t *P_pKey, const uint8_t *P_pNonce);

  int32_t ChaCha20Poly1305_Encrypt_Append(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx,
                                          const uint8_t *P_pInputBuffer,
                                          int32_t P_inputSize,
                                          uint8_t *P_pOutputBuffer,
                                          int32_t *P_pOutputSize);

  int32_t ChaCha20Poly1305_Header_Append(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx, const uint8_t *P_pInputBuffer, int32_t P_inputSize);

  int32_t ChaCha20Poly1305_Encrypt_Finish(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx, uint8_t *P_pOutputBuffer, int32_t *P_pOutputSize);

  int32_t ChaCha20Poly1305_Decrypt_Init(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx, const uint8_t *P_pKey, const uint8_t *P_pNonce);

  int32_t ChaCha20Poly1305_Decrypt_Append(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx,
                                          const uint8_t *P_pInputBuffer,
                                          int32_t P_inputSize,
                                          uint8_t *P_pOutputBuffer,
                                          int32_t *P_pOutputSize);

  int32_t ChaCha20Poly1305_Decrypt_Finish(ChaCha20Poly1305ctx_stt *P_pChaCha20Poly1305ctx, uint8_t *P_pOutputBuffer, int32_t *P_pOutputSize);

  /**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __CHACHA20_POLY1305_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
