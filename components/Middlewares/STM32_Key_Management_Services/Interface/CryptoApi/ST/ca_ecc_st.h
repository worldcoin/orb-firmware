/**
  ******************************************************************************
  * @file    ca_ecc_st.h
  * @author  MCD Application Team
  * @brief   This file contains the ECC router includes and definitions of
  *          the Cryptographic API (CA) module to the ST Cryptographic library.
  * @note    This file shall never be included directly by application but
  *          through the main header ca_ecc.h
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
#ifndef CA_ECC_ST_H
#define CA_ECC_ST_H

#if !defined(CA_ECC_H)
#error "This file shall never be included directly by application but through the main header ca_ecc.h"
#endif /* CA_ECC_H */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ECC ECDSA */
#if defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_ST)

/* Configuration defines -----------------------------------------------------*/
#ifndef INCLUDE_ECC
#define INCLUDE_ECC
#endif /* INCLUDE_ECC */

/* Includes ------------------------------------------------------------------*/
#include "ECC/ecc.h"

/* Defines ------------------------------------------------------------------*/
#define CA_ECDSA_REQUIRED_WORKING_BUFFER    2048

/* Structures Remapping -------------------------------------------------------*/
typedef EC_stt CA_EC_stt;
typedef intEC_stt CA_intEC_stt;
typedef ECPntFlags_et CA_ECPntFlags_et;
typedef ECpoint_stt CA_ECpoint_stt;
typedef ECcoordinate_et CA_ECcoordinate_et;
typedef ECCprivKey_stt CA_ECCprivKey_stt;

/* Defines Remapping ----------------------------------------------------------*/
/* ECPntFlags_et values */
#define CA_E_POINT_GENERAL      E_POINT_GENERAL
#define CA_E_POINT_NORMALIZED   E_POINT_NORMALIZED
#define CA_E_POINT_INFINITY     E_POINT_INFINITY
#define CA_E_POINT_MONTY        E_POINT_MONTY

/* ECcoordinate_et values */
#define CA_E_ECC_POINT_COORDINATE_X   E_ECC_POINT_COORDINATE_X
#define CA_E_ECC_POINT_COORDINATE_Y   E_ECC_POINT_COORDINATE_Y
#define CA_E_ECC_POINT_COORDINATE_Z   E_ECC_POINT_COORDINATE_Z

/* API Remapping --------------------------------------------------------------*/
#define CA_ECCinitEC ECCinitEC
#define CA_ECCfreeEC ECCfreeEC
#define CA_ECCinitPoint ECCinitPoint
#define CA_ECCfreePoint ECCfreePoint
#define CA_ECCsetPointCoordinate ECCsetPointCoordinate
#define CA_ECCgetPointCoordinate ECCgetPointCoordinate
#define CA_ECCinitPrivKey ECCinitPrivKey
#define CA_ECCfreePrivKey ECCfreePrivKey
#define CA_ECCsetPrivKeyValue ECCsetPrivKeyValue
#define CA_ECCgetPrivKeyValue ECCgetPrivKeyValue
#define CA_ECCscalarMul ECCscalarMul
#define CA_ECCvalidatePubKey ECCvalidatePubKey
#define CA_ECCkeyGen ECCkeyGen

/* Defines Remapping ----------------------------------------------------------*/
/* ECDSAsignValues_et values */
#define CA_E_ECDSA_SIGNATURE_R_VALUE   E_ECDSA_SIGNATURE_R_VALUE
#define CA_E_ECDSA_SIGNATURE_S_VALUE   E_ECDSA_SIGNATURE_S_VALUE

/* Structures Remapping -------------------------------------------------------*/
typedef ECDSAsignValues_et CA_ECDSAsignValues_et;
typedef ECDSAsignature_stt CA_ECDSAsignature_stt;
typedef ECDSAverifyCtx_stt CA_ECDSAverifyCtx_stt;

/* API Remapping --------------------------------------------------------------*/
#define CA_ECDSAinitSign ECDSAinitSign
#define CA_ECDSAfreeSign ECDSAfreeSign
#define CA_ECDSAsetSignature ECDSAsetSignature
#define CA_ECDSAverify ECDSAverify

#endif /* CA_ROUTE_ECC_ECDSA == CA_ROUTE_ST */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ECC ECDSA */



#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_ECC_ST_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

