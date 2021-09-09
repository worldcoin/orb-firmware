/**
  ******************************************************************************
  * @file    ca_ecc_hal.h
  * @author  MCD Application Team
  * @brief   This file contains the ECC router includes and definitions of
  *          the Cryptographic API (CA) module to the HAL Cryptographic drivers.
  * @note    This file shall never be included directly by application but
  *          through the main header ca_hash.h
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
#ifndef CA_ECC_HAL_H
#define CA_ECC_HAL_H

#if !defined(CA_ECC_H)
#error "This file shall never be included directly by application but through the main header ca_ecc.h"
#endif /* CA_ECC_H */

/* Configuration defines -----------------------------------------------------*/


/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ECC ECDSA */
#if defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_HAL)
/* Includes ------------------------------------------------------------------*/


/* Defines ------------------------------------------------------------------*/
#define CA_ECDSA_REQUIRED_WORKING_BUFFER    512

#define CA_ECDSA_PUBKEY_MAXSIZE  ((2UL * (CA_CRL_ECC_BIGGEST_SIZE)) + 1UL)      /*!< Maximum Size of ECDSA's Public key */
#define CA_ECDSA_PRIVKEY_MAXSIZE (CA_CRL_ECC_BIGGEST_SIZE)                      /*!< Maximum Size of ECDSA's Private key */
#define CA_ECDSA_SIGN_MAXSIZE    ((CA_CRL_ECC_BIGGEST_SIZE)*2UL)                /*!< Maximum Size of Buffer containing R and S */

/* Typedef ------------------------------------------------------------------*/

/**
  * @brief  Enumeration to specify the possible flags for an Elliptic Curve Point
  */
typedef enum CA_ECPntFlags_e
{

  CA_E_POINT_GENERAL = 0,    /*!< The point is not normalized (Coordinate Z != 1) */

  CA_E_POINT_NORMALIZED = 1, /*!< The point is normalized (Coordinate Z == 1)*/

  CA_E_POINT_INFINITY = 2,   /*!< The point is the O point */

  CA_E_POINT_MONTY = 4       /*!< The point's coordinates are expressed in Montgomery domain */
} CA_ECPntFlags_et;

/**
  * @brief   Enumeration for the coordinates of an elliptic curve point
  */
typedef enum CA_ECcoordinate_e
{
  CA_E_ECC_POINT_COORDINATE_X = 0,  /*!< Coordinate X */

  CA_E_ECC_POINT_COORDINATE_Y = 1,   /*!< Coordinate Y */

  CA_E_ECC_POINT_COORDINATE_Z = 2,   /*!< Coordinate Z */
} CA_ECcoordinate_et;

/**
  * @brief  Enumeration for the values inside the ECDSA signature
  */
typedef enum CA_ECDSAsignValues_e
{
  CA_E_ECDSA_SIGNATURE_R_VALUE = 0,  /*!<  Value R  */
  CA_E_ECDSA_SIGNATURE_S_VALUE = 1,  /*!<  Value S */
} CA_ECDSAsignValues_et;

/**
  * @brief  Object used to store an elliptic curve point.
  */
typedef struct
{

  CA_BigNum_stt *pmX ;     /*!< pmX coordinate  */

  CA_BigNum_stt *pmY ;     /*!< pmY coordinate  */

  CA_BigNum_stt *pmZ ;     /*!< pmZ coordinate, used in projective representations  */

  CA_ECPntFlags_et mFlag;  /*!< Point Flag, allowed values are: \n
                           * - flag=CRL_EPOINT_GENERAL for a point which may have pmZ different from 1
                           * - flag=CRL_EPOINT_NORMALIZED for a point which has pmZ equal to 1
                           * - flag=CRL_EPOINT_INFINITY to denote the infinity point
                          */
} CA_ECpoint_stt;

/**
  * @brief   Object used to store an ECC private key
  */
typedef struct
{
  CA_BigNum_stt *pmD;   /*!<  BigNum Representing the Private Key */
} CA_ECCprivKey_stt;

/**
  * @brief   Object used to store an ECDSA signature
  */
typedef struct
{
  /** R */
  CA_BigNum_stt *pmR ;  /*!< pointer to parameter R*/
  /** S */
  CA_BigNum_stt *pmS ; /*!< pointer to parameter S*/
} CA_ECDSAsignature_stt;

/**
  * @brief  Structure that keeps the Elliptic Curve Parameter
  */
typedef struct
{
  const uint8_t *pmA;  /*!< pointer to parameter "a" */
  int32_t mAsize;      /*!< size of parameter "a" */
  const uint8_t *pmB;  /*!< pointer to parameter "b" */
  int32_t mBsize;      /*!< size of parameter "b" */
  const uint8_t *pmP;  /*!<pointer to parameter "p" */
  int32_t mPsize;      /*!<size of parameter "p" */
  const uint8_t *pmN;  /*!< pointer to parameter "n" */
  int32_t mNsize;      /*!< size of parameter "n" */
  const uint8_t *pmGx; /*!< pointer to x coordinate of generator point */
  int32_t mGxsize;     /*!< size of x coordinate of generator point */
  const uint8_t *pmGy; /*!< pointer to y coordinate of generator point */
  int32_t mGysize;     /*!< size of y coordinate of generator point */
  /*intEC_stt *pmInternalEC;*/  /*!< Pointer to internal structure for handling the parameters - not in use for HAL router */
  /* Additional information to maintain context to communicate with HAL */
  PKA_HandleTypeDef hpka;
} CA_EC_stt;

#if (CA_ROUTE_ECC_ECDSA & CA_ROUTE_ECC_CFG_VERIFY_ENABLE)
/**
  * @brief  Structure used in ECDSA signature verification function
  */
typedef struct
{

  CA_ECpoint_stt *pmPubKey;  /*!<  Pointer to the ECC Public Key used in the verification */

  CA_EC_stt      *pmEC;      /*!<  Pointer to Elliptic Curve parameters */
} CA_ECDSAverifyCtx_stt;
#endif /* CA_ROUTE_ECC_ECDSA & CA_ROUTE_ECC_CFG_VERIFY_ENABLE */

/* Exported functions -------------------------------------------------------*/
int32_t CA_ECCinitEC(CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf);
int32_t CA_ECCfreeEC(CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf);
int32_t CA_ECCinitPoint(CA_ECpoint_stt **P_ppECPnt, const CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf);
int32_t CA_ECCfreePoint(CA_ECpoint_stt **P_pECPnt, CA_membuf_stt *P_pMemBuf);
int32_t CA_ECCsetPointCoordinate(CA_ECpoint_stt *P_pECPnt,
                                 CA_ECcoordinate_et P_Coordinate,
                                 const uint8_t *P_pCoordinateValue,
                                 int32_t P_coordinateSize);
int32_t CA_ECDSAinitSign(CA_ECDSAsignature_stt **P_ppSignature, const CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf);
int32_t CA_ECDSAfreeSign(CA_ECDSAsignature_stt **P_pSignature, CA_membuf_stt *P_pMemBuf);
int32_t CA_ECDSAsetSignature(CA_ECDSAsignature_stt *P_pSignature,
                             CA_ECDSAsignValues_et P_RorS,
                             const uint8_t *P_pValue,
                             int32_t P_valueSize);
#if (CA_ROUTE_ECC_ECDSA & CA_ROUTE_ECC_CFG_VERIFY_ENABLE)
int32_t CA_ECDSAverify(const uint8_t      *P_pDigest,
                       int32_t             P_digestSize,
                       const CA_ECDSAsignature_stt   *P_pSignature,
                       const CA_ECDSAverifyCtx_stt *P_pVerifyCtx,
                       CA_membuf_stt *P_pMemBuf);
#endif /* CA_ROUTE_ECC_ECDSA & CA_ROUTE_ECC_CFG_VERIFY_ENABLE */
int32_t CA_ECCvalidatePubKey(const CA_ECpoint_stt *pECCpubKey, const CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf);
#endif /* (CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_HAL */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ECC ECDSA */


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CA_ECC_HAL_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

