/**
  ******************************************************************************
  * @file    ca_ecc_mbed.c
  * @author  MCD Application Team
  * @brief   This file contains the ECC router implementation of
  *          the Cryptographic API (CA) module to the MBED Cryptographic library.
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
#ifndef CA_ECC_MBED_C
#define CA_ECC_MBED_C

/* Includes ------------------------------------------------------------------*/
#include "ca_ecc_mbed.h"

#include "mbedtls/bignum.h"
#include "mbedtls/pk.h"
#include "mbedtls/pk_internal.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
#if (defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED)) \
    || (defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED))
static psa_status_t wrap_import_ecc_pubKey_into_psa(psa_key_handle_t *P_Key_Handle,
                                                    psa_key_usage_t P_Psa_Usage,
                                                    psa_algorithm_t  P_Psa_Algorithm,
                                                    psa_ecc_curve_t ecc_curve,
                                                    const uint8_t *P_pEcc_pubKey,
                                                    uint32_t P_KeySize);
static uint8_t bignum_to_mpi(mbedtls_mpi *P_pMpi, BigNum_stt *P_pBigNum);
static uint8_t mpi_to_BigNum(mbedtls_mpi *P_pMpi, BigNum_stt *P_pBigNum);
static uint8_t uint8_t_to_mpi(mbedtls_mpi *P_pMpi, const uint8_t *P_pArray, int32_t Psize);
#endif /* ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED) || (CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED */

/* Functions Definition ------------------------------------------------------*/
#if (defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED)) \
    || (defined(CA_ROUTE_RSA) && ((CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED))
/**
  * @brief      Import a ECC public key to a psa struct
  * @param[in,out]
                *P_Key_Handle: Handle of the key
  * @param[in]  P_Psa_Usage: Key usage
  * @param[in]  P_Psa_Algorithm: Algorithm that will be used with the key
  * @param[in]  ecc_curve: The ECC curve
  * @param[in]  *P_pEcc_pubKey: The key
  * @param[in]  P_KeySize: The size of the key in bytes
  * @note       The only supported ecc curves for now are PSA_ECC_CURVE_SECP192R1,
  *             PSA_ECC_CURVE_SECP256R1 and PSA_ECC_CURVE_SECP384R1
  * @retval     CA_ECC_ERR_BAD_PARAMETER: Could not import the key
  * @retval     PSA_SUCCESS: Operation Successful
  */
static psa_status_t wrap_import_ecc_pubKey_into_psa(psa_key_handle_t *P_Key_Handle,
                                                    psa_key_usage_t P_Psa_Usage,
                                                    psa_algorithm_t  P_Psa_Algorithm,
                                                    psa_ecc_curve_t ecc_curve,
                                                    const uint8_t *P_pEcc_pubKey,
                                                    uint32_t P_KeySize)
{
  psa_status_t psa_ret_status;
  psa_key_policy_t key_policy = {0};

  /* Check parameters */
  if (P_Key_Handle == NULL)
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  if (P_pEcc_pubKey == NULL)
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  /* Import key into psa struct*/
  /* Allocate a Key storage area */
  psa_ret_status = psa_allocate_key(P_Key_Handle);
  if (psa_ret_status == PSA_SUCCESS)
  {
    /* Set algorithm for the allocated key */
    psa_key_policy_set_usage(&key_policy, P_Psa_Usage, P_Psa_Algorithm);
    psa_ret_status = psa_set_key_policy(*P_Key_Handle, &key_policy);
    if (psa_ret_status == PSA_SUCCESS)
    {
      /* Import the key */
      psa_ret_status = psa_import_key(*P_Key_Handle,
                                      PSA_KEY_TYPE_ECC_PUBLIC_KEY(ecc_curve),
                                      P_pEcc_pubKey,
                                      P_KeySize);
      if (psa_ret_status != PSA_SUCCESS)
      {
        psa_ret_status = CA_ECC_ERR_BAD_OPERATION;
      }
    }
    else
    {
      psa_ret_status = CA_ECC_ERR_BAD_CONTEXT;
    }
  }
  else
  {
    psa_ret_status = CA_ECC_ERR_BAD_CONTEXT;
  }
  return psa_ret_status;
}

/**
  * @brief Transform a BigNum into a mbedtls_mpi
  * @param[out] P_pmpi: The mpi equivalent of the BigNum
  * @param[in]  P_pbignum: A BigNum that will be transform
  * @retval WRAP_SUCCESS: on Success
  * @retval WRAP_FAILURE: if an input is NULL
  */
static uint8_t bignum_to_mpi(mbedtls_mpi *P_pMpi, BigNum_stt *P_pBigNum)
{
  int32_t mbed_return;
  /* Check parameters */
  if ((P_pMpi == NULL)
      || (P_pBigNum == NULL))
  {
    return WRAP_FAILURE;
  }
  /* mSignFlag is 0 for positive or 1 for negative,
  and s is 1 for positive and -1 for negative */
  P_pMpi->s = (P_pBigNum->mSignFlag == 0) ? 1 : -1;
  /* Increase mpi size if needed */
  mbed_return = mbedtls_mpi_grow(P_pMpi, P_pBigNum->mNumDigits);
  if (mbed_return == 0)
  {
    P_pMpi->n = P_pBigNum->mNumDigits;
    /* Copy data from bignum to mpi */
    for (uint32_t i = 0; i < P_pMpi->n; i++)
    {
      P_pMpi->p[i] = P_pBigNum->pmDigit[i];
    }
    return WRAP_SUCCESS;
  }
  else
  {
    return WRAP_FAILURE;
  }
}

/**
  * @brief      Function to pass an mpi type to a BigNum type
  * @param[in]  P_pmpi: The mpi data
  * @param[out] *P_pBigNum: Output data
  * @retval     WRAP_SUCCESS: On success
  * @reval      WRAP_FAILURE: An error occurs
  */
static uint8_t mpi_to_BigNum(mbedtls_mpi *P_pMpi, BigNum_stt *P_pBigNum)
{
  int32_t mbed_return = 0;
  /* Check parameters */
  if (P_pBigNum == NULL)
  {
    return WRAP_FAILURE;
  }
  /* If mpi is too big to fit in bignum, reduce it */
  if (P_pMpi->n > P_pBigNum->mSize)
  {
    mbed_return = mbedtls_mpi_shrink(P_pMpi, P_pBigNum->mSize);
  }
  /* If shrink failed, return on error */
  if ((P_pMpi->n > P_pBigNum->mSize) || (mbed_return != 0))
  {
    return WRAP_FAILURE;
  }
  /* mSignFlag is 0 for positive or 1 for negative,
  and s is 1 for positive and -1 for negative */
  P_pBigNum->mSignFlag = (P_pMpi->s == 1) ? 0 : 1;
  P_pBigNum->mNumDigits = (uint16_t)(P_pMpi->n);
  /* Copy data from mpi to bignum */
  for (uint32_t i = 0; i < P_pMpi->n; i++)
  {
    P_pBigNum->pmDigit[i] = P_pMpi->p[i];
  }

  return WRAP_SUCCESS;
}

/**
  * @brief Transform a uint8_t into a mbedtls_mpi
  * @param[out] P_pmpi: The mpi equivalent of the Array
  * @param[in]  P_pbignum: A array that will be transform
  * @retval WRAP_SUCCESS: on Success
  * @retval WRAP_FAILURE: otherwise
  */
static uint8_t uint8_t_to_mpi(mbedtls_mpi *P_pMpi, const uint8_t *P_pArray, int32_t Psize)
{
  int32_t mbed_return;
  uint8_t i;
  uint8_t j;
  uint32_t size_tp;
  int32_t size_counter = 0;

  /* Check parameters */
  if ((P_pMpi == NULL)
      || (P_pArray == NULL))
  {
    return WRAP_FAILURE;
  }
  if (Psize <= 0)
  {
    return WRAP_FAILURE;
  }

  /* Compute required mpi size, taking into account if the uint8 buffer length is or not a multiple of 4 */
  if ((Psize % 4) > 0)
  {
    size_tp = (((uint32_t)Psize / 4UL) + 1UL);
  }
  else
  {
    size_tp = ((uint32_t)Psize / 4UL);
  }
  /* Resize mpi to fit computed size */
  mbed_return = mbedtls_mpi_grow(P_pMpi, size_tp);
  if (mbed_return == 0)
  {
    /* Loop on mpi elements */
    for (i = 0U; i < size_tp; i++)
    {
      /* Loop on bytes within a mpi element */
      for (j = 0U; j < 4U; j++)
      {
        size_counter ++;
        if (size_counter <= Psize)
        {
          /* Copy each byte to the mpi element with corresponding shift */
          P_pMpi->p[size_tp - i - 1U] += (uint32_t)(P_pArray[(4U * i) + j]) << (8U * (3U - j));
        }
        else
        {
          /* If there is not enough uint8 data to fill alll the mpi elements, pad with previous uint8 element */
          P_pMpi->p[size_tp - i - 1U] = P_pMpi->p[size_tp - i - 1U] >> 8U;
        }
      }
    }
    /*Fill the rest of the structure*/
    P_pMpi->n = size_tp;
    P_pMpi->s = 1;
  }
  else
  {
    return WRAP_FAILURE;
  }

  return WRAP_SUCCESS;
}
#endif /* ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED) || (CA_ROUTE_RSA & CA_ROUTE_MASK) == CA_ROUTE_MBED */

#if defined(CA_ROUTE_ECC_ECDSA) && ((CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED)
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

  /* Are we setting the X coordinate ? */
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

  /* Are we setting the Y coordinate ?*/
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
  * @brief      Get the value of one of coordinate of an ECC point
  * @param[in]  *P_pECPnt: The ECC point from which extract the coordinate
  * @param[in]  P_Coordinate: Flag used to select which coordinate must
  *             be retrieved (see ECcoordinate_et )
  * @param[out] *P_pCoordinateValue: Pointer to an uint8_t array that will
  *             contain the returned coordinate
  * @param[out] *P_pCoordinateSize: Pointer to an integer that will
  *             contain the size of the returned coordinate
  * @note       The Coordinate size depends only on the size of the Prime (P)
  *             of the elliptic curve. Specifically if P_pECctx->mPsize is not
  *             a multiple of 4, then the size will be expanded to be
  *             a multiple of 4. In this case P_pCoordinateValue will contain
  *             one or more leading zeros.
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: One of the input parameters is invalid
  * @retval     CA_MATH_ERR_INTERNAL: internal computing error
  */
int32_t CA_ECCgetPointCoordinate(const CA_ECpoint_stt *P_pECPnt,
                                 CA_ECcoordinate_et P_Coordinate,
                                 uint8_t *P_pCoordinateValue,
                                 int32_t *P_pCoordinateSize)
{
  int32_t ecc_ret_status = CA_ECC_SUCCESS;

  /* Check parameters */
  if ((P_pECPnt == NULL)
      || (P_pCoordinateValue == NULL)
      || (P_pCoordinateSize == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Do we want to retrieve X value ? */
  if (P_Coordinate == CA_E_ECC_POINT_COORDINATE_X)
  {
    if (P_pECPnt->pmX->pmDigit == NULL)
    {
      return CA_ECC_ERR_BAD_PARAMETER;
    }
    /* Copy the data to the output */
    if (wrap_BigNum_to_uint8(P_pCoordinateValue, P_pECPnt->pmX, P_pCoordinateSize) != WRAP_SUCCESS)
    {
      return CA_MATH_ERR_INTERNAL;
    }
  }

  /* Do we want to retrieve Y value ? */
  else if (P_Coordinate == CA_E_ECC_POINT_COORDINATE_Y)
  {
    if (P_pECPnt->pmY->pmDigit == NULL)
    {
      return CA_ECC_ERR_BAD_PARAMETER;
    }
    /* Copy the data to the output */
    if (wrap_BigNum_to_uint8(P_pCoordinateValue, P_pECPnt->pmY, P_pCoordinateSize) != WRAP_SUCCESS)
    {
      return CA_MATH_ERR_INTERNAL;
    }
  }
  else
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }

  return ecc_ret_status;
}

/**
  * @brief  Init the ECC private key structure with a static array
  * @param[in,out]
  *             **P_ppECCprivKey: The ECC private key that will be init
  * @param[in]  *P_pECctx: NOT USED
  * @param[in]  *P_pMemBuf: NOT USED
  * @retval CA_ECC_ERR_BAD_PARAMETER: When **P_ppECCprivKey is NULL
  * @retval CA_ECC_SUCCESS: Otherwise
  */
int32_t CA_ECCinitPrivKey(CA_ECCprivKey_stt **P_ppECCprivKey, CA_EC_stt *P_pECctx, CA_membuf_stt *P_pMemBuf)
{
  int32_t ecc_ret_status;

  /* Check parameters */
  if ((P_ppECCprivKey == NULL) || (P_pMemBuf == NULL))
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }
  else
  {
    /* Allocate main structure */
    *P_ppECCprivKey = (CA_ECCprivKey_stt *)wrap_allocate_memory(sizeof(CA_ECCprivKey_stt), P_pMemBuf);
    if (*P_ppECCprivKey  == NULL)
    {
      ecc_ret_status = CA_ERR_MEMORY_FAIL;
    }
    else
    {
      /* Allocate pmD */
      (**P_ppECCprivKey).pmD = (BigNum_stt *)wrap_allocate_memory(sizeof(BigNum_stt), P_pMemBuf);
      if ((**P_ppECCprivKey).pmD == NULL)
      {
        ecc_ret_status =  CA_ERR_MEMORY_FAIL;
      }
      else
      {
        /* Allocate pmDigit element */
        (**P_ppECCprivKey).pmD->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize), P_pMemBuf);
        if ((**P_ppECCprivKey).pmD->pmDigit == NULL)
        {
          ecc_ret_status =  CA_ERR_MEMORY_FAIL;
        }
        else
        {
          /* Set element size */
          (**P_ppECCprivKey).pmD->mSize = (uint8_t)((uint32_t)(P_pECctx->mAsize) / sizeof(uint32_t));
          /* All allocation succeeded, return success */
          ecc_ret_status =  CA_ECC_SUCCESS;
        }
      }
    }
  }
  return ecc_ret_status;
}

/**
  * @brief  Free the Private key
  * @param[in,out]
  *          **P_ppECCprivKey: The ECC private key that will be free
  * @param[in]  *P_pMemBuf: NOT USED
  * @retval CA_ECC_ERR_BAD_OPERATION: When **P_ppECCprivKey is NULL
  * @retval CA_ECC_SUCCESS: Otherwise
  */
int32_t CA_ECCfreePrivKey(CA_ECCprivKey_stt **P_ppECCprivKey, CA_membuf_stt *P_pMemBuf)
{
  (void)P_ppECCprivKey;
  (void)P_pMemBuf;
  /* Nothing to free as all allocation are made into a specific memory buffer hosted by the context */
  return CA_ECC_SUCCESS;
}

/**
  * @brief  Set the priv key value
  * @param[in, out]
                *P_pECCprivKey: The ECC private key object to set
  * @param[in]  *P_pPrivateKey: Pointer to an uint8_t array that contains
  *             the value of the private key
  * @param[in]  P_privateKeySize: The size in bytes of P_pPrivateKey
  * @retval CA_ECC_SUCCESS: On success
  * @retval CA_ECC_ERR_BAD_OPERATION: Invalid parameters
  */
int32_t CA_ECCsetPrivKeyValue(CA_ECCprivKey_stt *P_pECCprivKey,
                              const uint8_t *P_pPrivateKey,
                              int32_t P_privateKeySize)
{
  uint8_t wrap_ret_status;
  int32_t ecc_ret_status = CA_ECC_SUCCESS;

  /* Check parameters */
  if ((P_pECCprivKey == NULL) || (P_pPrivateKey == NULL))
  {
    return CA_ECC_ERR_BAD_OPERATION;
  }
  if (P_privateKeySize < 0)
  {
    return CA_ECC_ERR_BAD_OPERATION;
  }

  /* Set the key */
  wrap_ret_status = wrap_uint8_to_BigNum(P_pECCprivKey->pmD, P_pPrivateKey, P_privateKeySize);
  if (wrap_ret_status == WRAP_FAILURE)
  {
    ecc_ret_status = CA_ECC_ERR_BAD_OPERATION;
  }
  return ecc_ret_status;
}

/**
  * @brief  Get the private key value
  * @param[in]   *P_pECCprivKey: The ECC private key object to get
  * @param[out]  *P_pPrivateKey: Pointer to an uint8_t array that contains
  *             the value of the private key
  * @param[out]  *P_pPrivateKeySize: The size in bytes of P_pPrivateKey
  * @retval CA_ECC_SUCCESS: On success
  * @retval CA_ECC_ERR_BAD_OPERATION: Invalid parameters
  */
int32_t CA_ECCgetPrivKeyValue(const CA_ECCprivKey_stt *P_pECCprivKey,
                              uint8_t *P_pPrivateKey,
                              int32_t *P_pPrivateKeySize)
{
  uint8_t wrap_ret_status;
  int32_t ecc_ret_status = CA_ECC_SUCCESS;

  /* Check parameters */
  if ((P_pECCprivKey == NULL)
      || (P_pPrivateKey == NULL)
      || (P_pPrivateKeySize == NULL))
  {
    return CA_ECC_ERR_BAD_OPERATION;
  }
  /* Get the key value and put in the output */
  wrap_ret_status = wrap_BigNum_to_uint8(P_pPrivateKey,
                                         P_pECCprivKey->pmD,
                                         P_pPrivateKeySize);
  if (wrap_ret_status == WRAP_FAILURE)
  {
    ecc_ret_status = CA_ECC_ERR_BAD_OPERATION;
  }
  return ecc_ret_status;
}

/**
  * @brief      Computes the point scalar multiplication kP = k*P
  * @param[in]  *P_pECbasePnt: The point that will be multiplied
  * @param[in]  *P_pECCprivKey: Structure containing the scalar value of the multiplication
  * @param[out] *P_pECresultPnt: The output point, result of the multiplication
  * @param[in]  *P_pECctx: Structure describing the curve parameters
  * @param[in, out]
                *P_pMemBuf: Pointer to the membuf_stt structure that currently stores the EC Priv Key internal value
  * @retval     CA_ECC_SUCCESS Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: One of the input is null
  * @retval     CA_ECC_ERR_BAD_CONTEXT: Public key is incorrect
  * @retval     CA_MATH_ERR_INTERNAL: Generic MATH error
  * @retval     CA_ECC_ERR_MISSING_EC_PARAMETER: Some required parameters are missing from the P_pECctx structure
  * @retval     CA_ECC_ERR_BAD_PRIVATE_KEY: Private key is not correct
  * @retval     CA_ERR_MEMORY_FAIL: There is not enough memory
  */
int32_t CA_ECCscalarMul(const CA_ECpoint_stt *P_pECbasePnt,
                        const CA_ECCprivKey_stt *P_pECCprivKey,
                        CA_ECpoint_stt *P_pECresultPnt,
                        const CA_EC_stt *P_pECctx,
                        CA_membuf_stt *P_pMemBuf)
{
  mbedtls_ecdsa_context peer_ctx; /* mbed context */
  mbedtls_ecp_point dev_eph_pub_key_ec_point; /* public key */
  mbedtls_ecp_point peer_shared_secret_x_ec_point; /* result of the multiplication */
  int32_t pub_key_size;                     /* pub key size */
  int32_t PeerPrvKeyEcPointLen;   /* private key size */
  int32_t mbed_return;            /* return value for mbed function */
  uint8_t wrap_status;            /* return value for wrap function */
  uint8_t *pTmp;

  /* Check parameters */
  if ((P_pECbasePnt == NULL) || (P_pECCprivKey == NULL) || (P_pECctx == NULL) || (P_pECresultPnt == NULL)
      || (P_pMemBuf == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Initialize cryptographic underlying layer context for ECDSA computing */
  mbedtls_ecdsa_init(&peer_ctx);

  /* Load the correct curve into the context */
  switch (P_pECctx->mAsize)
  {
    case (CA_CRL_ECC_P192_SIZE) :
      mbed_return = mbedtls_ecp_group_load(&peer_ctx.grp, MBEDTLS_ECP_DP_SECP192R1);
      break;

    case (CA_CRL_ECC_P256_SIZE) :
      mbed_return = mbedtls_ecp_group_load(&peer_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
      break;

    case (CA_CRL_ECC_P384_SIZE) :
      mbed_return = mbedtls_ecp_group_load(&peer_ctx.grp, MBEDTLS_ECP_DP_SECP384R1);
      break;

    default :
      mbedtls_ecdsa_free(&peer_ctx);
      return CA_ECC_ERR_MISSING_EC_PARAMETER;
  }

  if (0 != mbed_return)
  {
    /* Upon error, release context */
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }

  /* Convert the public key on mbed format */
  pTmp = (uint8_t *)(uint32_t) &(P_pECctx->tmp_pubKey[0]);
  pTmp[0] = 0x04; /* Init pub key for X9.62 format*/
  /* Init point structure */
  mbedtls_ecp_point_init(&dev_eph_pub_key_ec_point);
  /* Get X & Y coordinates, store them into pTmp */
  if (CA_ECCgetPointCoordinate(P_pECbasePnt, CA_E_ECC_POINT_COORDINATE_X,
                               &(pTmp[1]), &pub_key_size) != CA_ECC_SUCCESS)
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }
  if (CA_ECCgetPointCoordinate(P_pECbasePnt, CA_E_ECC_POINT_COORDINATE_Y,
                               &(pTmp[pub_key_size + 1]), &pub_key_size) != CA_ECC_SUCCESS)
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }
  /* Inject public key to MBED context */
  if (0 != mbedtls_ecp_point_read_binary(&peer_ctx.grp, &dev_eph_pub_key_ec_point,
                                         &(pTmp[0]), ((uint32_t)pub_key_size * 2UL) + 1UL))
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }

  /* Check public key validity */
  if (0 != mbedtls_ecp_check_pubkey(&peer_ctx.grp, &dev_eph_pub_key_ec_point))
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_ECC_ERR_BAD_CONTEXT;
  }

  /* Convert the private key on mbed format */
  pTmp = (uint8_t *)(uint32_t) &(P_pECctx->tmp_privKey[0]);
  if (CA_ECCgetPrivKeyValue(P_pECCprivKey, pTmp, &PeerPrvKeyEcPointLen) != CA_ECC_SUCCESS)
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }
  /* Inject private key into mbed context */
  if (0 != mbedtls_mpi_read_binary(&peer_ctx.d, pTmp, (uint32_t)PeerPrvKeyEcPointLen))
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }
  if (0 != mbedtls_ecp_check_privkey(&peer_ctx.grp, &peer_ctx.d))
  {
    /* Upon error, release context and point */
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_ECC_ERR_BAD_PRIVATE_KEY;
  }

  /* Do the multiplication */
  mbedtls_ecp_point_init(&peer_shared_secret_x_ec_point);

  /* Do the scalar multiplication of injected public and private keys */
  mbed_return = mbedtls_ecp_mul(&peer_ctx.grp, &peer_shared_secret_x_ec_point, &peer_ctx.d,
                                &dev_eph_pub_key_ec_point, NULL, NULL);
  if (MBEDTLS_ERR_MPI_ALLOC_FAILED  == mbed_return)
  {
    /* Upon error, release context and points */
    mbedtls_ecp_point_free(&peer_shared_secret_x_ec_point);
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_ERR_MEMORY_FAIL;
  }
  else if (0 != mbed_return)
  {
    /* Upon error, release context and points */
    mbedtls_ecp_point_free(&peer_shared_secret_x_ec_point);
    mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
    mbedtls_ecdsa_free(&peer_ctx);
    /* Private key and public already check, so it's a unknown error*/
    return CA_MATH_ERR_INTERNAL;
  }
  else
  {
    /* Write peer_shared_secret_x_ec_point in P_pECresultPnt */
    wrap_status = mpi_to_BigNum(&(peer_shared_secret_x_ec_point.X), P_pECresultPnt->pmX);
    wrap_status |= mpi_to_BigNum(&(peer_shared_secret_x_ec_point.Y), P_pECresultPnt->pmY);
    wrap_status |= mpi_to_BigNum(&(peer_shared_secret_x_ec_point.Z), P_pECresultPnt->pmZ);
  }

  /* Cleanup */
  mbedtls_ecp_point_free(&peer_shared_secret_x_ec_point);
  mbedtls_ecp_point_free(&dev_eph_pub_key_ec_point);
  mbedtls_ecdsa_free(&peer_ctx);

  if (WRAP_SUCCESS != wrap_status)
  {
    return CA_MATH_ERR_INTERNAL;
  }
  return CA_ECC_SUCCESS;
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
  /* Check parameters */
  if ((P_ppSignature == NULL) || (P_pECctx == NULL) || (P_pMemBuf == NULL))
  {
    ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
  }
  else
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
            (**P_ppSignature).pmS->pmDigit = (uint32_t *)wrap_allocate_memory((uint32_t)(P_pECctx->mAsize), P_pMemBuf);
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
  uint8_t wrap_ret_status = WRAP_FAILURE;
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
  if (wrap_ret_status == WRAP_SUCCESS)
  {
    ecc_ret_status = CA_ECC_SUCCESS;
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
  int32_t ecc_ret_status;
  psa_status_t psa_ret_status;
  psa_status_t psa_ret_status_tp;
  const psa_algorithm_t psa_algorithm = PSA_ALG_ECDSA_BASE;
  int32_t wrap_sign_R_size = 0;
  int32_t wrap_sign_S_size = 0;
  uint32_t wrap_pubKey_x = 0;
  uint32_t wrap_pubKey_y = 0;
  uint32_t wrap_pubKey;
  psa_ecc_curve_t wrap_ecc_curve;
  psa_key_handle_t ECDSAkeyHandle = {0};

  (void)P_pMemBuf;

  /* Check parameters */
  if ((P_pDigest == NULL) || (P_pVerifyCtx == NULL) || (P_pSignature == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  if ((P_pVerifyCtx->pmPubKey == NULL) || (P_pVerifyCtx->pmEC == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  /* Save R */
  if (wrap_BigNum_to_uint8(P_pVerifyCtx->pmEC->tmp_Sign,
                           P_pSignature->pmR,
                           &wrap_sign_R_size) == WRAP_SUCCESS)
  {
    /* Save S right after R */
    if (wrap_BigNum_to_uint8(&P_pVerifyCtx->pmEC->tmp_Sign[wrap_sign_R_size],
                             P_pSignature->pmS,
                             &wrap_sign_S_size) != WRAP_SUCCESS)
    {
      return CA_ECC_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }
  /* Choose which curves to use */
  if ((uint32_t)wrap_sign_R_size == CA_CRL_ECC_P192_SIZE)
  {
    wrap_ecc_curve = PSA_ECC_CURVE_SECP192R1;
  }
  else if ((uint32_t)wrap_sign_R_size == CA_CRL_ECC_P256_SIZE)
  {
    wrap_ecc_curve = PSA_ECC_CURVE_SECP256R1;
  }
  else if ((uint32_t)wrap_sign_R_size == CA_CRL_ECC_P384_SIZE)
  {
    wrap_ecc_curve = PSA_ECC_CURVE_SECP384R1;
  }
  else
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /*Pubkey is in the P_pVerifyCtx,
  so we need to get the data and put those into the right format*/
  /*It as to be in this format: 0x04 + x_P + y_P.
  0x04 mean uncompress key in x9.62 format */
  (P_pVerifyCtx->pmEC->tmp_pubKey)[0] = 0x04;
  /* Copy X coordinate */
  if (wrap_BigNum_to_uint8(&(P_pVerifyCtx->pmEC->tmp_pubKey)[1],
                           P_pVerifyCtx->pmPubKey->pmX,
                           (int32_t *)(uint32_t)&wrap_pubKey_x) == WRAP_SUCCESS)
  {
    /* Copy Y coordinate */
    if (wrap_BigNum_to_uint8(&(P_pVerifyCtx->pmEC->tmp_pubKey)[1UL + wrap_pubKey_x],
                             P_pVerifyCtx->pmPubKey->pmY,
                             (int32_t *)(uint32_t)&wrap_pubKey_y) == WRAP_SUCCESS)
    {
      wrap_pubKey = wrap_pubKey_x + wrap_pubKey_y + 1UL;
    }
    else
    {
      return CA_ECC_ERR_BAD_PARAMETER;
    }
  }
  else
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  if (wrap_pubKey > CA_ECDSA_PUBKEY_MAXSIZE)
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Import the key */
  psa_ret_status = wrap_import_ecc_pubKey_into_psa(&ECDSAkeyHandle,
                                                   PSA_KEY_USAGE_VERIFY,
                                                   psa_algorithm,
                                                   wrap_ecc_curve,
                                                   (P_pVerifyCtx->pmEC->tmp_pubKey),
                                                   wrap_pubKey);
  if (psa_ret_status == PSA_SUCCESS)
  {
    /* Verify */
    psa_ret_status = psa_asymmetric_verify(ECDSAkeyHandle,
                                           psa_algorithm,
                                           P_pDigest,
                                           (uint32_t) P_digestSize,
                                           P_pVerifyCtx->pmEC->tmp_Sign,
                                           ((uint32_t)wrap_sign_R_size + (uint32_t)wrap_sign_S_size));
    /* We won't be able to reuse the key, so we destroy it */
    psa_ret_status_tp = psa_destroy_key(ECDSAkeyHandle);
    if (psa_ret_status_tp == PSA_SUCCESS)
    {
      if (psa_ret_status == PSA_ERROR_INVALID_SIGNATURE)
      {
        ecc_ret_status = CA_SIGNATURE_INVALID;
      }
      else if (psa_ret_status == PSA_ERROR_INSUFFICIENT_MEMORY)
      {
        ecc_ret_status = CA_ERR_MEMORY_FAIL;
      }
      else if (psa_ret_status == PSA_ERROR_INVALID_ARGUMENT)
      {
        ecc_ret_status = CA_ECC_ERR_BAD_PARAMETER;
      }
      /*Success*/
      else if (psa_ret_status == PSA_SUCCESS)
      {
        ecc_ret_status = CA_SIGNATURE_VALID;
      }
      /* In case of other return status*/
      else
      {
        ecc_ret_status = CA_ECC_ERR_BAD_CONTEXT;
      }
    }
    else
    {
      ecc_ret_status = CA_ECC_ERR_BAD_OPERATION;
    }
  }
  else
  {
    ecc_ret_status = CA_ECC_ERR_BAD_OPERATION;
  }

  return ecc_ret_status;
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
  int32_t ecc_ret_status = CA_ECC_ERR_BAD_CONTEXT;
  uint8_t wrap_ret_status;
  int32_t ret_status;
  mbedtls_mpi x;
  mbedtls_mpi y;
  mbedtls_mpi a;
  mbedtls_mpi b;
  mbedtls_mpi p;
  mbedtls_mpi aX;
  mbedtls_mpi X2;
  mbedtls_mpi X3;
  mbedtls_mpi Yc;
  mbedtls_mpi Y;

  (void)P_pMemBuf;
  /* Check parameters */
  if ((pECCpubKey == NULL) || (P_pECctx == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /*Put x in yc^2 = x^3 + ax + b */
  /*if yc == y it is a valid key, otherwise it's not */

  /* Initialize mpi objects needed to store the public key information */
  mbedtls_mpi_init(&x);
  mbedtls_mpi_init(&y);
  mbedtls_mpi_init(&a);
  mbedtls_mpi_init(&b);
  mbedtls_mpi_init(&p);

  /* Fill in the mpi objects with public key information */
  wrap_ret_status = bignum_to_mpi(&x, pECCpubKey->pmX);
  wrap_ret_status |= bignum_to_mpi(&y, pECCpubKey->pmY);
  wrap_ret_status |= uint8_t_to_mpi(&a, P_pECctx->pmA, P_pECctx->mAsize);
  wrap_ret_status |= uint8_t_to_mpi(&b, P_pECctx->pmB, P_pECctx->mBsize);
  wrap_ret_status |= uint8_t_to_mpi(&p, P_pECctx->pmP, P_pECctx->mPsize);
  if (wrap_ret_status == WRAP_SUCCESS)
  {
    /* Initialize mpi objects needed to validate the public key */
    mbedtls_mpi_init(&aX);
    mbedtls_mpi_init(&X2);
    mbedtls_mpi_init(&X3);
    mbedtls_mpi_init(&Yc);
    mbedtls_mpi_init(&Y);

    /* Do the computation needed to validate the public key */
    ret_status = mbedtls_mpi_mul_mpi(&X2, &x, &x);        /*X2 = x^2*/
    ret_status += mbedtls_mpi_mul_mpi(&X3, &X2, &x);      /*X3 = x^3 = X2*x*/
    ret_status += mbedtls_mpi_mul_mpi(&aX, &a, &x);       /*X = a*x*/
    ret_status += mbedtls_mpi_add_mpi(&Yc, &X3, &aX);     /*Yc = X^3+ax*/
    ret_status += mbedtls_mpi_add_mpi(&Yc, &Yc, &b);      /*Yc = X^3+aX+b*/
    ret_status += mbedtls_mpi_mod_mpi(&Yc, &Yc, &p);      /*Yc = Yc mod p*/

    if (ret_status == 0)
    {
      ret_status = mbedtls_mpi_mul_mpi(&Y, &y, &y);       /*Y = y^2*/
      ret_status += mbedtls_mpi_mod_mpi(&Y, &Y, &p);      /*Y = Y mod p*/
      if (ret_status == 0)
      {
        ret_status = mbedtls_mpi_cmp_mpi(&Yc, &Y);        /*Yc ?= Y */
        if (ret_status == 0)
        {
          ecc_ret_status = CA_ECC_SUCCESS;
        }
        else
        {
          ecc_ret_status = CA_ECC_ERR_BAD_PUBLIC_KEY;
        }
      }
      else
      {
        ecc_ret_status = CA_ECC_ERR_BAD_CONTEXT;
      }
    }
    else
    {
      ecc_ret_status = CA_ECC_ERR_BAD_CONTEXT;
    }

    /* Free the computation mpi */
    mbedtls_mpi_free(&aX);
    mbedtls_mpi_free(&X2);
    mbedtls_mpi_free(&X3);
    mbedtls_mpi_free(&Yc);
    mbedtls_mpi_free(&Y);
  }

  /* Free all */
  mbedtls_mpi_free(&x);
  mbedtls_mpi_free(&y);
  mbedtls_mpi_free(&a);
  mbedtls_mpi_free(&b);
  mbedtls_mpi_free(&p);

  return ecc_ret_status;
}

/**
  * @brief      Generate an ECC key pair.
  * @param[out] *P_pPrivKey: Initialized object that will contain the generated private key
  * @param[out] *P_pPubKey: Initialized point that will contain the generated public key
  * @param[in]  *P_pRandomState(1): The random engine current state
  * @param[in]  *P_pECctx: Structure describing the curve parameters. This must contain the values of the generator
  * @param[in, out]
                *P_pMemBuf: Pointer to the membuf_stt structure that will be used to store the internal values
                required by computation
  * @retval     CA_ECC_SUCCESS: Operation Successful
  * @retval     CA_ECC_ERR_BAD_PARAMETER: One of input parameters is not valid
  * @retval     CA_ECC_ERR_MISSING_EC_PARAMETER: P_pECctx must contain a, p, n, Gx, Gy
  * @retval     CA_MATH_ERR_INTERNAL Generic MATH error
  */
int32_t CA_ECCkeyGen(CA_ECCprivKey_stt *P_pPrivKey,
                     CA_ECpoint_stt    *P_pPubKey,
                     CA_RNGstate_stt *P_pRandomState,
                     const CA_EC_stt    *P_pECctx,
                     CA_membuf_stt *P_pMemBuf)
{
  mbedtls_ecdsa_context peer_ctx; /* mbed context */
  int32_t mbed_return;            /* return value for mbed function */
  mbedtls_entropy_context _entropy;
  mbedtls_ctr_drbg_context _ctr_drbg;
  const uint8_t *DRBG_PERS = (uint8_t *)"mbed TLS helloword client";
  uint8_t wrap_status;

  /* Control null pointer */
  if ((P_pPrivKey == NULL) || (P_pPubKey == NULL) || (P_pRandomState == NULL) || (P_pECctx == NULL)
      || (P_pMemBuf == NULL))
  {
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Initialize cryptographic underlying layer context for ECDSA computing */
  mbedtls_ecdsa_init(&peer_ctx);

  /* Load the correct curve into the context */
  switch (P_pECctx->mAsize)
  {
    case (CA_CRL_ECC_P192_SIZE):
      mbed_return = mbedtls_ecp_group_load(&peer_ctx.grp, MBEDTLS_ECP_DP_SECP192R1);
      break;

    case (CA_CRL_ECC_P256_SIZE):
      mbed_return = mbedtls_ecp_group_load(&peer_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
      break;

    case (CA_CRL_ECC_P384_SIZE):
      mbed_return = mbedtls_ecp_group_load(&peer_ctx.grp, MBEDTLS_ECP_DP_SECP384R1);
      break;

    default :
      mbedtls_ecdsa_free(&peer_ctx);
      return CA_ECC_ERR_MISSING_EC_PARAMETER;
  }

  if (0 != mbed_return)
  {
    /* Upon error, release context */
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_MATH_ERR_INTERNAL;
  }

  /* Initialize entropy & DRBG */
  mbedtls_entropy_init(&_entropy);
  mbedtls_ctr_drbg_init(&_ctr_drbg);

  /* Seed DRBG with entropy */
  if (0 != mbedtls_ctr_drbg_seed(&_ctr_drbg, mbedtls_entropy_func, &_entropy,
                                 (const uint8_t *) DRBG_PERS, sizeof(DRBG_PERS)))
  {
    /* Upon error, release contexts */
    mbedtls_ctr_drbg_free(&_ctr_drbg);
    mbedtls_entropy_free(&_entropy);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Generate keys */
  if (0 != mbedtls_ecdsa_genkey(&peer_ctx, peer_ctx.grp.id, mbedtls_entropy_func, &_entropy))
  {
    /* Upon error, release contexts */
    mbedtls_ctr_drbg_free(&_ctr_drbg);
    mbedtls_entropy_free(&_entropy);
    mbedtls_ecdsa_free(&peer_ctx);
    return CA_ECC_ERR_BAD_PARAMETER;
  }

  /* Copy secret key */
  wrap_status = mpi_to_BigNum(&(peer_ctx.d), P_pPrivKey->pmD);

  /* Copy public key */
  wrap_status |= mpi_to_BigNum(&(peer_ctx.Q.X), P_pPubKey->pmX);
  wrap_status |= mpi_to_BigNum(&(peer_ctx.Q.Y), P_pPubKey->pmY);
  wrap_status |= mpi_to_BigNum(&(peer_ctx.Q.Z), P_pPubKey->pmZ);

  /* Cleanup */
  mbedtls_ctr_drbg_free(&_ctr_drbg);
  mbedtls_entropy_free(&_entropy);
  mbedtls_ecdsa_free(&peer_ctx);

  if (WRAP_SUCCESS != wrap_status)
  {
    return CA_MATH_ERR_INTERNAL;
  }

  return CA_ECC_SUCCESS;
}

#endif /* (CA_ROUTE_ECC_ECDSA & CA_ROUTE_MASK) == CA_ROUTE_MBED */




#endif /* CA_ECC_MBED_C */
#endif /* CA_CORE_C */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

