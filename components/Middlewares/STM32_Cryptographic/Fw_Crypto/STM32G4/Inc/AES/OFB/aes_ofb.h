/**
  ******************************************************************************
  * @file    aes_ofb.h
  * @author  MCD Application Team
  * @brief   AES in OFB Mode
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

#ifndef __CRL_AES_OFB_H__
#define __CRL_AES_OFB_H__

#ifdef __cplusplus
extern "C"
{
#endif

  /** @ingroup AESOFB
    * @{
    */

  /** OFB context = CBC context. AESOFBctx_stt is equal to AESCBCctx_stt */
  typedef AESCBCctx_stt AESOFBctx_stt;

#ifdef INCLUDE_ENCRYPTION
  /* load the key and ivec, eventually performs key schedule, etc. */
  int32_t AES_OFB_Encrypt_Init (AESOFBctx_stt *P_pAESOFBctx, \
                                const uint8_t *P_pKey,      \
                                const uint8_t *P_pIv);

  /* launch crypto operation , can be called several times */
  int32_t AES_OFB_Encrypt_Append (AESOFBctx_stt *P_pAESOFBctx,   \
                                  const uint8_t *P_pInputBuffer, \
                                  int32_t        P_inputSize,    \
                                  uint8_t       *P_pOutputBuffer, \
                                  int32_t       *P_pOutputSize);

  /* Possible final output */
  int32_t AES_OFB_Encrypt_Finish (AESOFBctx_stt *P_pAESOFBctx,   \
                                  uint8_t       *P_pOutputBuffer, \
                                  int32_t       *P_pOutputSize);
#endif
#ifdef INCLUDE_DECRYPTION
  /* load the key and ivec, eventually performs key schedule, etc. */
  int32_t AES_OFB_Decrypt_Init (AESOFBctx_stt *P_pAESOFBctx, \
                                const uint8_t *P_pKey,      \
                                const uint8_t *P_pIv);

  /* launch crypto operation , can be called several times */
  int32_t AES_OFB_Decrypt_Append (AESOFBctx_stt *P_pAESOFBctx,   \
                                  const uint8_t *P_pInputBuffer, \
                                  int32_t        P_inputSize,    \
                                  uint8_t       *P_pOutputBuffer, \
                                  int32_t       *P_pOutputSize);

  /* Possible final output */
  int32_t AES_OFB_Decrypt_Finish (AESOFBctx_stt *P_pAESOFBctx,   \
                                  uint8_t       *P_pOutputBuffer, \
                                  int32_t       *P_pOutputSize);
#endif
  /**
    * @}
    */

#ifdef __cplusplus
}
#endif

#endif /* __CRL_AES_OFB_H__*/


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
