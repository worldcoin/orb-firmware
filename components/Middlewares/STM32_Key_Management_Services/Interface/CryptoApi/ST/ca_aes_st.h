/**
  ******************************************************************************
  * @file    ca_aes_st.h
  * @author  MCD Application Team
  * @brief   This file contains the AES router includes and definitions of
  *          the Cryptographic API (CA) module to the ST Cryptographic library.
  * @note    This file shall never be included directly by application but
  *          through the main header ca_aes.h
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
#ifndef CA_AES_ST_H
#define CA_AES_ST_H

#if !defined(CA_AES_H)
#error "This file shall never be included directly by application but through the main header ca_aes.h"
#endif /* CA_AES_H */

/* Configuration defines -----------------------------------------------------*/
#if defined(CA_FEAT_AES_ENCRYPT)
#ifndef INCLUDE_ENCRYPTION
#define INCLUDE_ENCRYPTION
#endif /* INCLUDE_ENCRYPTION */
#endif /* CA_FEAT_AES_ENCRYPT */
#if defined(CA_FEAT_AES_DECRYPT)
#ifndef INCLUDE_DECRYPTION
#define INCLUDE_DECRYPTION
#endif /* INCLUDE_DECRYPTION */
#endif /* CA_FEAT_AES_DECRYPT */
#if defined(CA_FEAT_AES_128BITS)
#ifndef INCLUDE_AES128
#define INCLUDE_AES128
#endif /* INCLUDE_AES128 */
#endif /* CA_FEAT_AES_128BITS */
#if defined(CA_FEAT_AES_192BITS)
#ifndef INCLUDE_AES192
#define INCLUDE_AES192
#endif /* INCLUDE_AES192 */
#endif /* CA_FEAT_AES_192BITS */
#if defined(CA_FEAT_AES_256BITS)
#ifndef INCLUDE_AES256
#define INCLUDE_AES256
#endif /* INCLUDE_AES256 */
#endif /* CA_FEAT_AES_256BITS */

/* Includes ------------------------------------------------------------------*/
#include "AES/Common/aes_common.h"

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CBC */
#if defined(CA_ROUTE_AES_CBC) && ((CA_ROUTE_AES_CBC & CA_ROUTE_MASK) == CA_ROUTE_ST)
/* Defines Remapping ----------------------------------------------------------*/
#ifndef INCLUDE_CBC
#define INCLUDE_CBC
#endif /* INCLUDE_CBC */

#include "AES/CBC/aes_cbc.h"

/* Structures Remapping -------------------------------------------------------*/
typedef AESCBCctx_stt CA_AESCBCctx_stt;

/* API Remapping --------------------------------------------------------------*/
#if defined(INCLUDE_ENCRYPTION)
#define CA_AES_CBC_Encrypt_Init     AES_CBC_Encrypt_Init
#define CA_AES_CBC_Encrypt_Append   AES_CBC_Encrypt_Append
#define CA_AES_CBC_Encrypt_Finish   AES_CBC_Encrypt_Finish
#endif /* INCLUDE_ENCRYPTION */
#if defined(INCLUDE_DECRYPTION)
#define CA_AES_CBC_Decrypt_Init     AES_CBC_Decrypt_Init
#define CA_AES_CBC_Decrypt_Append   AES_CBC_Decrypt_Append
#define CA_AES_CBC_Decrypt_Finish   AES_CBC_Decrypt_Finish
#endif /* INCLUDE_DECRYPTION */

#endif /* CA_ROUTE_AES_CBC == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CBC */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CCM */
#if defined(CA_ROUTE_AES_CCM) && ((CA_ROUTE_AES_CCM & CA_ROUTE_MASK) == CA_ROUTE_ST)
/* Defines Remapping ----------------------------------------------------------*/
#ifndef INCLUDE_CCM
#define INCLUDE_CCM
#endif /* INCLUDE_CCM */

#include "AES/CCM/aes_ccm.h"

/* Structures Remapping -------------------------------------------------------*/
#define CA_AESCCMctx_stt    AESCCMctx_stt

/* API Remapping --------------------------------------------------------------*/
#if defined(INCLUDE_ENCRYPTION)
#define CA_AES_CCM_Encrypt_Init     AES_CCM_Encrypt_Init
#define CA_AES_CCM_Encrypt_Append   AES_CCM_Encrypt_Append
#define CA_AES_CCM_Encrypt_Finish   AES_CCM_Encrypt_Finish
#endif /* INCLUDE_ENCRYPTION */
#if defined(INCLUDE_DECRYPTION)
#define CA_AES_CCM_Decrypt_Init     AES_CCM_Decrypt_Init
#define CA_AES_CCM_Decrypt_Append   AES_CCM_Decrypt_Append
#define CA_AES_CCM_Decrypt_Finish   AES_CCM_Decrypt_Finish
#endif /* INCLUDE_DECRYPTION */
#define CA_AES_CCM_Header_Append    AES_CCM_Header_Append

#endif /* CA_ROUTE_AES_CCM == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CCM */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES CMAC */
#if defined(CA_ROUTE_AES_CMAC) && ((CA_ROUTE_AES_CMAC & CA_ROUTE_MASK) == CA_ROUTE_ST)
/* Defines Remapping ----------------------------------------------------------*/
#ifndef INCLUDE_CMAC
#define INCLUDE_CMAC
#endif /* INCLUDE_CMAC */

#include "AES/CMAC/aes_cmac.h"

/* Structures Remapping -------------------------------------------------------*/
#define CA_AESCMACctx_stt    AESCMACctx_stt

/* API Remapping --------------------------------------------------------------*/
#if defined(INCLUDE_ENCRYPTION)
#define CA_AES_CMAC_Encrypt_Init     AES_CMAC_Encrypt_Init
#define CA_AES_CMAC_Encrypt_Append   AES_CMAC_Encrypt_Append
#define CA_AES_CMAC_Encrypt_Finish   AES_CMAC_Encrypt_Finish
#endif /* INCLUDE_ENCRYPTION */
#if defined(INCLUDE_DECRYPTION)
#define CA_AES_CMAC_Decrypt_Init     AES_CMAC_Decrypt_Init
#define CA_AES_CMAC_Decrypt_Append   AES_CMAC_Decrypt_Append
#define CA_AES_CMAC_Decrypt_Finish   AES_CMAC_Decrypt_Finish
#endif /* INCLUDE_DECRYPTION */

#endif /* CA_ROUTE_AES_CMAC == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES CMAC */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES ECB */
#if defined(CA_ROUTE_AES_ECB) && ((CA_ROUTE_AES_ECB & CA_ROUTE_MASK) == CA_ROUTE_ST)
/* Defines Remapping ----------------------------------------------------------*/
#ifndef INCLUDE_ECB
#define INCLUDE_ECB
#endif /* INCLUDE_ECB */

#include "AES/ECB/aes_ecb.h"

/* Structures Remapping -------------------------------------------------------*/
#define CA_AESECBctx_stt    AESECBctx_stt

/* API Remapping --------------------------------------------------------------*/
#if defined(INCLUDE_ENCRYPTION)
#define CA_AES_ECB_Encrypt_Init     AES_ECB_Encrypt_Init
#define CA_AES_ECB_Encrypt_Append   AES_ECB_Encrypt_Append
#define CA_AES_ECB_Encrypt_Finish   AES_ECB_Encrypt_Finish
#endif /* INCLUDE_ENCRYPTION */
#if defined(INCLUDE_DECRYPTION)
#define CA_AES_ECB_Decrypt_Init     AES_ECB_Decrypt_Init
#define CA_AES_ECB_Decrypt_Append   AES_ECB_Decrypt_Append
#define CA_AES_ECB_Decrypt_Finish   AES_ECB_Decrypt_Finish
#endif /* INCLUDE_DECRYPTION */

#endif /* CA_ROUTE_AES_ECB == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES ECB */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> AES GCM */
#if defined(CA_ROUTE_AES_GCM) && ((CA_ROUTE_AES_GCM & CA_ROUTE_MASK) == CA_ROUTE_ST)
/* Defines Remapping ----------------------------------------------------------*/
#ifndef INCLUDE_GCM
#define INCLUDE_GCM
#endif /* INCLUDE_GCM */
#if defined(CRL_GFMUL)
#undef CRL_GFMUL
#endif /* CRL_GFMUL */
#define CRL_GFMUL   2     /* Define max value */
#include "AES/GCM/aes_gcm.h"

/* Structures Remapping -------------------------------------------------------*/
#define CA_AESGCMctx_stt    AESGCMctx_stt

/* API Remapping --------------------------------------------------------------*/
#if defined(INCLUDE_ENCRYPTION)
#define CA_AES_GCM_Encrypt_Init     AES_GCM_Encrypt_Init
#define CA_AES_GCM_Encrypt_Append   AES_GCM_Encrypt_Append
#define CA_AES_GCM_Encrypt_Finish   AES_GCM_Encrypt_Finish
#endif /* INCLUDE_ENCRYPTION */
#if defined(INCLUDE_DECRYPTION)
#define CA_AES_GCM_Decrypt_Init     AES_GCM_Decrypt_Init
#define CA_AES_GCM_Decrypt_Append   AES_GCM_Decrypt_Append
#define CA_AES_GCM_Decrypt_Finish   AES_GCM_Decrypt_Finish
#endif /* INCLUDE_DECRYPTION */
#define CA_AES_GCM_Header_Append    AES_GCM_Header_Append

#endif /* CA_ROUTE_AES_GCM == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< AES GCM */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_AES_ST_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

