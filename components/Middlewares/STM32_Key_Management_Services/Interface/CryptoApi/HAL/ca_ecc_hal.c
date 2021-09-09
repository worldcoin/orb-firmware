/**
  ******************************************************************************
  * @file    ca_ecc_hal.c
  * @author  MCD Application Team
  * @brief   This file contains the ECC router implementation of
  *          the Cryptographic API (CA) module to the HAL Cryptographic drivers.
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

/* CA sources are built by building ca_core.c giving it the proper ca_config.h */
/* This file can not be build alone                                            */
#if defined(CA_CORE_C)

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CA_ECC_HAL_C
#define CA_ECC_HAL_C

/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/
#define PKA_ECDSA_VERIF_OUT_SIGNATURE_R ((0x055CUL - PKA_RAM_OFFSET)>>2)   /*!< Output result */
#define IMAGE_VALID   (uint8_t)(0x55)
#define IMAGE_INVALID (uint8_t)(0x82)

/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static uint8_t wrap_AbsA(uint8_t *P_pR,
                         uint8_t const *P_pA,
                         uint8_t const *P_pB,
                         uint32_t P_sizeA,
                         uint32_t P_sizeB,
                         uint32_t P_pSizeR,
                         uint8_t *P_pSign);

/* Functions Definition ------------------------------------------------------*/

/**
  * @brief      Compute absolute value of A-B
  * @param[out] P_pR R result of the operation
  * @param[in]  P_pA A value
  * @param[in]  P_pB B value
  * @param[in]  P_sizeA A size
  * @param[in]  P_sizeB B size
  * @param[in]  P_sizeR R size
  * @param[in]  P_pSign Sign of R
  * @retval     WRAP_FAILURE, WRAP_SUCCESS
  */
static uint8_t wrap_AbsA(uint8_t *P_pR,
                         uint8_t const *P_pA,
                         uint8_t const *P_pB,
                         uint32_t P_sizeA,
                         uint32_t P_sizeB,
                         uint32_t P_pSizeR,
                         uint8_t *P_pSign)
{
  uint32_t wrap_maxsize;
  uint32_t i;
  int32_t wrap_upper;
  uint8_t wrap_hold = 0U;

  /* Check parameters */
  if ((P_pR == NULL)
      || (P_pA == NULL)
      || (P_pB == NULL))
  {
    return WRAP_FAILURE;
  }

  /* Compute biggest size between A & B */
  if (P_sizeA > P_sizeB)
  {
    wrap_maxsize = P_sizeA;
  }
  else
  {
    wrap_maxsize = P_sizeB;
  }

  /* Check that we have the memory space necessary to compute the operation */
  if (wrap_maxsize > P_pSizeR)
  {
    return WRAP_FAILURE;
  }

  /* Compare the two buffer */
  wrap_upper = memcmp(P_pA, P_pB, wrap_maxsize);
  if (wrap_upper > 0)
  {
    /* Positive value */
    /* Do the subtraction from the right most byte to the leftmost one */
    for (i = wrap_maxsize; i > 0U; i--)
    {
      /* A >= B, simply subtract and reset hold */
      /* Take into account potential previous computation hold */
      if (P_pA[i - 1U] >= P_pB[i - 1U])
      {
        /* R  = A - B*/
        P_pR[i - 1U] = P_pA[i - 1U] - P_pB[i - 1U] - wrap_hold;
        wrap_hold = 0U;
      }
      /* A < B, do (A + 0x100) - B and keep an hold for the next computation */
      /* Take into account potential previous computation hold */
      else
      {
        /* R  = A - B + 0x100 - hold */
        P_pR[i - 1U] = (uint8_t)((uint32_t)P_pA[i - 1U] - (uint32_t)P_pB[i - 1U] + 256U - (uint32_t)wrap_hold);
        wrap_hold = 1U;
      }
    }
    *P_pSign = 0U; /* Positive */
  }
  else if (wrap_upper < 0)
  {
    /* Negative value, inversion of A and B */
    /* Do the subtraction from the right most byte to the leftmost one */
    for (i = wrap_maxsize; i > 0U; i--)
    {
      /* B >= A, simply subtract and reset hold*/
      /* Take into account potential previous computation hold */
      if (P_pB[i - 1U] >= P_pA[i - 1U])
      {
        /* R  = B - A */
        P_pR[i - 1U] = P_pB[i - 1U] - P_pA[i - 1U] - wrap_hold;
        wrap_hold = 0U;
      }
      /* B < A, do (B + 0x100) - A and keep an hold for the next computation */
      /* Take into account potential previous computation hold */
      else
      {
        /* R  = B - A + 0x100 - hold */
        P_pR[i - 1U] = (uint8_t)((uint32_t)P_pB[i - 1U] - (uint32_t)P_pA[i - 1U] + 256U - (uint32_t)wrap_hold);
        wrap_hold = 1U;
      }
    }

    *P_pSign = 1U; /* Negative */
  }
  else
  {
    /* Same number */
    /* Just set 0 into R as the result of the subtraction */
    (void)memset(P_pR, 0, P_pSizeR);
    *P_pSign = 0U; /* Positive */
  }
  return WRAP_SUCCESS;
}

#if defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_HAL)
/**
  * @brief      Initialize the elliptic curve parameters into a EC_stt structure
  * @param[in, out]
  *             *P_pECctx: EC_stt context with parameters of ellliptic curve used
  *             NOT USED
  * @param[in, out]
  *             *P_pMemBuf: Pointer to membuf_stt structure that will be used
  *             to store the Ellitpic Curve internal values. NOT USED
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: otherwise
  */
int32_t CA_ECCinitEC(CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf)
{
  (void)P_pECctx;
  (void)P_pMemBuf;

  /* Nothing special to do, simply return success */
  return CA_ECC_SUCCESS;
}

/**
  * @brief      Do nothing
  * @retval     ECC_SUCCESS
  */
int32_t CA_ECCfreeEC(CA_EC_stt *P_pECctx, membuf_stt *P_pMemBuf)
{
  (void)P_pECctx;
  (void)P_pMemBuf;

  /* Nothing special to do, simply return success */
  return CA_ECC_SUCCESS;
}

/**
  * @brief      Initialize an ECC point
  * @param[out] **P_ppECPnt: The point that will be initialized
  * @param[in]  *P_pECctx: The EC_stt containing the Elliptic Curve Parameters
  *             NOT USED
  * @param[in, out]
  *             *P_pMemBuf: Pointer to the membuf_stt structure that will
  *             be used to store the Ellitpic Curve Point internal values NOT USED
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: P_ppECPnt == NULL
  * @retval     CA_ERR_MEMORY_FAIL:: Not enough memory
  */
int32_t CA_ECCinitPoint(CA_ECpoint_stt **P_ppECPnt, const CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf)
{
  int32_t ecc_ret_status;

  /* Check parameters */
  if ((P_ppECPnt == NULL) || (P_pECctx == NULL) || (P_pMemBuf == NULL))
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }
  else
  {
    /* Allocate main structure */
    *P_ppECPnt = (CA_ECpoint_stt *)wrap_allocate_memory(sizeof(CA_ECpoint_stt), P_pMemBuf);
    if (*P_ppECPnt  == NULL)
    {
      ecc_ret_status = CA_ERR_MEMORY_FAIL;
    }
    else
    {
      /* Allocate pmX element */
      (**P_ppECPnt).pmX = (BigNum_stt *)wrap_allocate_memory(sizeof(BigNum_stt), P_pMemBuf);
      if ((**P_ppECPnt).pmX == NULL)
      {
        ecc_ret_status =  CA_ERR_MEMORY_FAIL;
      }
      else
      {
        /* Allocate pmDigit element */
        (**P_ppECPnt).pmX->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize), P_pMemBuf);
        if ((**P_ppECPnt).pmX->pmDigit == NULL)
        {
          ecc_ret_status =  CA_ERR_MEMORY_FAIL;
        }
        else
        {
          /* Set element size */
          (**P_ppECPnt).pmX->mSize = (uint8_t)((uint32_t)(P_pECctx->mAsize) / sizeof(uint32_t));
          /* Allocate pmY */
          (**P_ppECPnt).pmY = (BigNum_stt *)wrap_allocate_memory(sizeof(BigNum_stt), P_pMemBuf);
          if ((**P_ppECPnt).pmY == NULL)
          {
            ecc_ret_status =  CA_ERR_MEMORY_FAIL;
          }
          else
          {
            /* Allocate pmDigit element */
            (**P_ppECPnt).pmY->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize), P_pMemBuf);
            if ((**P_ppECPnt).pmY->pmDigit == NULL)
            {
              ecc_ret_status =  CA_ERR_MEMORY_FAIL;
            }
            else
            {
              /* Set element size */
              (**P_ppECPnt).pmY->mSize = (uint8_t)((uint32_t)(P_pECctx->mAsize) / sizeof(uint32_t));
              /* Allocate pmZ */
              (**P_ppECPnt).pmZ = (BigNum_stt *)wrap_allocate_memory(sizeof(BigNum_stt), P_pMemBuf);
              if ((**P_ppECPnt).pmZ == NULL)
              {
                ecc_ret_status =  CA_ERR_MEMORY_FAIL;
              }
              else
              {
                /* Allocate pmDigit element */
                (**P_ppECPnt).pmZ->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize), P_pMemBuf);
                if ((**P_ppECPnt).pmZ->pmDigit == NULL)
                {
                  ecc_ret_status =  CA_ERR_MEMORY_FAIL;
                }
                else
                {
                  /* Set element size */
                  (**P_ppECPnt).pmZ->mSize = (uint8_t)((uint32_t)(P_pECctx->mAsize) / sizeof(uint32_t));
                  /* All allocation succeeded, return success */
                  ecc_ret_status =  CA_ECC_SUCCESS;
                }
              }
            }
          }
        }
      }
    }
  }
  return ecc_ret_status;
}

/**
  * @brief      Free Elliptic curve point
  * @param[in]  *P_pECPnt The point that will be free
  * @param[in, out]
  *             *P_pMemBuf Pointer to membuf_stt structure that stores Ellitpic
  *             Curve Point internal values. NOT USED
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER P_pECPnt == NULL
  * @retval     CA_ECC_ERR_BAD_CONTEXT  *P_pECPnt == NULL
  */
int32_t CA_ECCfreePoint(CA_ECpoint_stt **P_pECPnt, CA_membuf_stt *P_pMemBuf)
{
  (void)P_pECPnt;
  (void)P_pMemBuf;

  /* Nothing to free as all allocation are made into a specific memory buffer hosted by the context */
  return CA_ECC_SUCCESS;
}

/**
  * @brief      Set the value of one of coordinate of an ECC point
  * @param[in, out]
  *             *P_pECPnt: The ECC point that will have a coordinate set
  * @param[in]  P_Coordinate: Flag used to select which coordinate must be set
  *             (see ECcoordinate_et )
  * @param[in]  *P_pCoordinateValue: Pointer to an uint8_t array that
  *             contains the value to be set
  * @param[in]  P_coordinateSize: The size in bytes of P_pCoordinateValue
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: One of the input parameters is invalid
  */
int32_t CA_ECCsetPointCoordinate(CA_ECpoint_stt *P_pECPnt,
                                 CA_ECcoordinate_et P_Coordinate,
                                 const uint8_t *P_pCoordinateValue,
                                 int32_t P_coordinateSize)
{
  uint32_t wrap_ret_status;
  int32_t ecc_ret_status;

  /* Check parameters */
  if ((P_pECPnt == NULL) || (P_pCoordinateValue == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Are we setting the X coordinate ?*/
  if (P_Coordinate == CA_E_ECC_POINT_COORDINATE_X)
  {
    /* Set X */
    wrap_ret_status = wrap_uint8_to_BigNum(P_pECPnt->pmX,
                                           P_pCoordinateValue,
                                           P_coordinateSize);
    if (wrap_ret_status == WRAP_SUCCESS)
    {
      ecc_ret_status = CA_ECC_SUCCESS;
    }
    else
    {
      ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
    }
  }

  /* Are we setting the Y coordinate ? */
  else if (P_Coordinate == CA_E_ECC_POINT_COORDINATE_Y)
  {
    /* Set Y */
    wrap_ret_status = wrap_uint8_to_BigNum(P_pECPnt->pmY,
                                           P_pCoordinateValue,
                                           P_coordinateSize);
    if (wrap_ret_status == WRAP_SUCCESS)
    {
      ecc_ret_status = CA_ECC_SUCCESS;
    }
    else
    {
      ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }

  return ecc_ret_status;
}

/**
  * @brief  Init a Signature
  * @param[in]  **P_ppSignature: The initialized Signature
  * @param[in]  *P_pECctx: The ecc context NOT USED
  * @param[in]  *P_pMemBuf: NOT USED
  * @retval CA_ECC_SUCCESS: on Success
  * @retval CA_ECC_ERR_BAD_PARAMETER: P_ppSignature == NULL
  */
int32_t CA_ECDSAinitSign(CA_ECDSAsignature_stt **P_ppSignature, const CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf)
{
  int32_t ecc_ret_status;
  HAL_StatusTypeDef hal_ret_status;

  /* Check parameters */
  if (P_ppSignature == NULL)
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }
  else
  {
    ((CA_EC_stt *)(uint32_t)P_pECctx)->hpka.Instance = CA_PKA_INSTANCE;
    ((CA_EC_stt *)(uint32_t)P_pECctx)->hpka.State = HAL_PKA_STATE_RESET;
    ((CA_EC_stt *)(uint32_t)P_pECctx)->hpka.ErrorCode = HAL_PKA_ERROR_NONE;
    /* HAL initialisation */
    hal_ret_status = HAL_PKA_Init(&((CA_EC_stt *)(uint32_t)P_pECctx)->hpka);
    if (hal_ret_status == HAL_OK)
    {
      /* Allocate main structure */
      *P_ppSignature = (CA_ECDSAsignature_stt *)wrap_allocate_memory(sizeof(CA_ECDSAsignature_stt), P_pMemBuf);
      if (*P_ppSignature  == NULL)
      {
        ecc_ret_status = CA_ERR_MEMORY_FAIL;
      }
      else
      {
        /* Allocate pmR */
        (**P_ppSignature).pmR = (BigNum_stt *)wrap_allocate_memory(sizeof(BigNum_stt), P_pMemBuf);
        if ((**P_ppSignature).pmR == NULL)
        {
          ecc_ret_status =  CA_ERR_MEMORY_FAIL;
        }
        else
        {
          /* Allocate pmDigit */
          (**P_ppSignature).pmR->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize), P_pMemBuf);
          if ((**P_ppSignature).pmR->pmDigit == NULL)
          {
            ecc_ret_status =  CA_ERR_MEMORY_FAIL;
          }
          else
          {
            /* Set element size */
            (**P_ppSignature).pmR->mSize = (uint8_t)((uint32_t)(P_pECctx->mAsize) / sizeof(uint32_t));
            /* Allocate pmS */
            (**P_ppSignature).pmS = (BigNum_stt *)wrap_allocate_memory(sizeof(BigNum_stt), P_pMemBuf);
            if ((**P_ppSignature).pmS == NULL)
            {
              ecc_ret_status =  CA_ERR_MEMORY_FAIL;
            }
            else
            {
              /* Allocate pmDigit */
              (**P_ppSignature).pmS->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize),
                                                                                P_pMemBuf);
              if ((**P_ppSignature).pmS->pmDigit == NULL)
              {
                ecc_ret_status =  CA_ERR_MEMORY_FAIL;
              }
              else
              {
                /* Set element size */
                (**P_ppSignature).pmS->mSize = (uint8_t)((uint32_t)(P_pECctx->mAsize) / sizeof(uint32_t));
                /* All allocation succeeded, return success */
                ecc_ret_status =  CA_ECC_SUCCESS;
              }
            }
          }
        }
      }
    }
    else
    {
      ecc_ret_status = CA_ECC_ERR_BAD_CONTEXT;
    }
  }
  return ecc_ret_status;
}

/**
  * @brief      Free an ECDSA signature structure
  * @param[in, out]
  *             **P_pSignature: The ECDSA signature that will be free
  * @param[in, out]
  *             *P_pMemBuf: Pointer to the membuf_stt structure that currently
  *             stores the ECDSA signature internal values NOT USED
  * @retval     CA_ECC_SUCCESS Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: P_pSignature == NULL
  */
int32_t CA_ECDSAfreeSign(CA_ECDSAsignature_stt **P_pSignature, CA_membuf_stt *P_pMemBuf)
{
  (void)P_pSignature;
  (void)P_pMemBuf;

  /* Nothing to free as all allocation are made into a specific memory buffer hosted by the context */
  return CA_ECC_SUCCESS;
}

/**
  * @brief      Set the value of the parameters (one at a time) of an ECDSAsignature_stt
  * @param[out] *P_pSignature: The ECDSA signature whose one of the value
  *             will be set.
  * @param[in]  P_RorS: Flag selects if the parameter R or the parameter S
  *             must be set.
  * @param[in]  *P_pValue: Pointer to an uint8_t array containing the signature value
  * @param[in]  P_valueSize: Size of the signature value
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: otherwise
  */
int32_t CA_ECDSAsetSignature(CA_ECDSAsignature_stt *P_pSignature,
                             CA_ECDSAsignValues_et P_RorS,
                             const uint8_t *P_pValue,
                             int32_t P_valueSize)
{
  uint8_t wrap_ret_status = WRAP_SUCCESS;
  int32_t ecc_ret_status = CA_ECC_SUCCESS;

  /* Check parameters */
  if ((P_pValue == NULL) || (P_pSignature == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  /* Are we setting R ? */
  if (P_RorS == CA_E_ECDSA_SIGNATURE_R_VALUE)
  {
    /* Set R */
    wrap_ret_status = wrap_uint8_to_BigNum(P_pSignature->pmR, P_pValue, P_valueSize);
  }
  /* Or S ? */
  else if (P_RorS == CA_E_ECDSA_SIGNATURE_S_VALUE)
  {
    /* Set S */
    wrap_ret_status = wrap_uint8_to_BigNum(P_pSignature->pmS, P_pValue, P_valueSize);
  }
  else
  {
    /* Did not find what to set, return error */
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }
  if (wrap_ret_status == WRAP_FAILURE)
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }
  return ecc_ret_status;
}

#if (CA_ROUTE_ECC_ECDSA & CA_ROUTE_ECC_CFG_VERIFY_ENABLE)
/**
  * @brief      ECDSA signature verification with a digest input
  * @param[in]  *P_pDigest: The digest of the signed message
  * @param[in]  P_digestSize: The mSize in bytes of the digest
  * @param[in]  *P_pSignature: The public key that will verify the signature.
  * @param[in]  *P_pVerifyCtx: The ECDSA signature that will be verified
  * @param[in, out]
  *             *P_pMemBuf: Pointer to the membuf_stt structure that will be used
  *             to store the internal values required by computation. NOT USED
  * @note       This function requires that:
  *             - P_pVerifyCtx.pmEC points to a valid and initialized EC_stt structure
  *             - P_pVerifyCtx.pmPubKey points to a valid and initialized public key ECpoint_stt structure
  * @retval     CA_ERR_MEMORY_FAIL: There's not enough memory
  * @retval     CA_ECC_ERR_BAD_PARAMETER: An error occur
  * @retval     CA_ECC_ERR_BAD_CONTEXT: One parameter is not valid
  * @retval     CA_SIGNATURE_INVALID: The signature is NOT valid
  * @retval     CA_SIGNATURE_VALID: The signature is valid
  */
int32_t CA_ECDSAverify(const uint8_t      *P_pDigest,
                       int32_t             P_digestSize,
                       const CA_ECDSAsignature_stt   *P_pSignature,
                       const CA_ECDSAverifyCtx_stt *P_pVerifyCtx,
                       CA_membuf_stt *P_pMemBuf)
{
  int32_t ecdsa_ret_status = CA_SIGNATURE_INVALID;
  uint8_t wrap_ret_status;
  HAL_StatusTypeDef hal_ret_status;
  PKA_ECDSAVerifInTypeDef PkaVerify = {0};
  uint8_t wrap_R[384] = {0};
  uint8_t wrap_S[384] = {0};
  uint8_t wrap_X[384] = {0};
  uint8_t wrap_Y[384] = {0};
  uint8_t wrap_absA[384] = {0};
  uint8_t wrap_sign;
  __IO uint8_t sign_check_status = 0x00U;
  uint32_t i;
  uint32_t j;
  uint8_t *p_sign_PKA;

  (void)P_digestSize;
  (void)P_pMemBuf;

  /* Check parameters */
  if ((P_pDigest == NULL)
      || (P_pSignature == NULL)
      || (P_pVerifyCtx == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  if (P_pVerifyCtx->pmPubKey == NULL)
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  if (((uint32_t)(P_pVerifyCtx->pmPubKey->pmX->mNumDigits) + (uint32_t)(P_pVerifyCtx->pmPubKey->pmY->mNumDigits))
      > CA_ECDSA_PUBKEY_MAXSIZE)
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Compute |a| to transmit it to PKA */
  wrap_ret_status = wrap_AbsA(wrap_absA,
                              P_pVerifyCtx->pmEC->pmA,
                              P_pVerifyCtx->pmEC->pmP,
                              (uint32_t)(P_pVerifyCtx->pmEC->mAsize),
                              (uint32_t)(P_pVerifyCtx->pmEC->mBsize),
                              sizeof(wrap_absA),
                              &wrap_sign);
  if (wrap_ret_status == WRAP_SUCCESS)
  {

    /* Fill PkaVerify structure */
    PkaVerify.primeOrderSize = (uint32_t)(P_pVerifyCtx->pmEC->mNsize);
    PkaVerify.modulusSize = (uint32_t)(P_pVerifyCtx->pmEC->mPsize);
    PkaVerify.coefSign =        wrap_sign;
    PkaVerify.coef =            wrap_absA;
    PkaVerify.modulus =         P_pVerifyCtx->pmEC->pmP;
    PkaVerify.basePointX =      P_pVerifyCtx->pmEC->pmGx;
    PkaVerify.basePointY =      P_pVerifyCtx->pmEC->pmGy;
    PkaVerify.primeOrder =      P_pVerifyCtx->pmEC->pmN;
    PkaVerify.hash =            P_pDigest;

    /* Convert R to PKA format */
    wrap_ret_status = wrap_BigNum_to_uint8(wrap_R, P_pSignature->pmR, NULL);
    if (wrap_ret_status == WRAP_SUCCESS)
    {
      PkaVerify.RSign = (const uint8_t *)wrap_R;
    }
    /* Convert S to PKA format */
    wrap_ret_status = wrap_BigNum_to_uint8(wrap_S, P_pSignature->pmS, NULL);
    if (wrap_ret_status == WRAP_SUCCESS)
    {
      PkaVerify.SSign = (const uint8_t *)wrap_S;
    }
    /* Convert X to PKA format */
    wrap_ret_status = wrap_BigNum_to_uint8(wrap_X, P_pVerifyCtx->pmPubKey->pmX, NULL);
    if (wrap_ret_status == WRAP_SUCCESS)
    {
      PkaVerify.pPubKeyCurvePtX = (const uint8_t *)wrap_X;
    }
    /* Convert Y to PKA format */
    wrap_ret_status = wrap_BigNum_to_uint8(wrap_Y, P_pVerifyCtx->pmPubKey->pmY, NULL);
    if (wrap_ret_status == WRAP_SUCCESS)
    {
      PkaVerify.pPubKeyCurvePtY = (const uint8_t *)wrap_Y;
    }

    /* Launch the verification */
    hal_ret_status = HAL_PKA_ECDSAVerif(&P_pVerifyCtx->pmEC->hpka, &PkaVerify, 5000);
    if (hal_ret_status == HAL_OK)
    {
      /* Check the signature */
      if (HAL_PKA_ECDSAVerif_IsValidSignature(&P_pVerifyCtx->pmEC->hpka) == 1UL)
      {
        /* Double check ECDSA signature to avoid basic HW attack */
        p_sign_PKA = (uint8_t *) & (P_pVerifyCtx->pmEC->hpka).Instance->RAM[PKA_ECDSA_VERIF_OUT_SIGNATURE_R];

        /* Signature comparison LSB vs MSB */
        j = PkaVerify.primeOrderSize - 1U;
        for (i = 0U; i < PkaVerify.primeOrderSize; i++)
        {
          sign_check_status |= wrap_R[i] ^ IMAGE_VALID ^ p_sign_PKA[j];
          j--;
        }

        /* Loop fully executed ==> no basic HW attack */
        if ((sign_check_status == IMAGE_VALID) && (i == PkaVerify.primeOrderSize))
        {
          ecdsa_ret_status = CA_SIGNATURE_VALID;
        }
        else
        {
          ecdsa_ret_status = CA_SIGNATURE_INVALID;
        }
      }
    }
    else
    {
      /* PKA operation returns error */
      ecdsa_ret_status = CA_ECC_ERR_BAD_CONTEXT;
    }

    /* Deinitialize the CA_PKA_INSTANCE */
    hal_ret_status = HAL_PKA_DeInit(&P_pVerifyCtx->pmEC->hpka);
    if ((hal_ret_status == HAL_OK) && (ecdsa_ret_status == CA_SIGNATURE_VALID))
    {
      ecdsa_ret_status = CA_SIGNATURE_VALID;
    }
  }
  else
  {
    ecdsa_ret_status =  CA_ECC_ERR_BAD_CONTEXT;
  }

  return ecdsa_ret_status;
}
#endif /* CA_ROUTE_ECC_ECDSA & CA_ROUTE_ECC_CFG_VERIFY_ENABLE */

/**
  * @brief  Checks the validity of a public key
  * @param[in]  *pECCpubKey: The public key to be checked
  * @param[in]  *P_pECctx: Structure describing the curve parameters
  * @param[in, out]
  *             *P_pMemBuf: Pointer to the membuf_stt structure that will
  *             be used to store the internal values required by computation NOT USED
  * @retval CA_ECC_SUCCESS pECCpubKey is a valid point of the curve
  * @retval CA_ECC_ERR_BAD_PUBLIC_KEY pECCpubKey is not a valid point of the curve
  * @retval CA_ECC_ERR_BAD_PARAMETER One of the input parameter is NULL
  * @retval CA_ECC_ERR_BAD_CONTEXT One of the values inside P_pECctx is invalid
  */
int32_t CA_ECCvalidatePubKey(const CA_ECpoint_stt *pECCpubKey, const CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf)
{
  (void)pECCpubKey;
  (void)P_pECctx;
  (void)P_pMemBuf;

  /* Nothing special to do, simply return success */
  return CA_ECC_SUCCESS;
}

#endif /* (CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_HAL */




#endif /* CA_ECC_HAL_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

