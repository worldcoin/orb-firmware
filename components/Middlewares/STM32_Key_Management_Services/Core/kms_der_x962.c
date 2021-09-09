/**
  ******************************************************************************
  * @file    kms_der_x962.c
  * @author  MCD Application Team
  * @brief   This file contains utilities for the manipulation of DER & X9.62 format
  *          of Key Management Services (KMS)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "kms.h"                /* PKCS11 definitions */
#if defined(KMS_ENABLED)
#include "kms_der_x962.h"       /* KMS DER & X962 utilities */

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_DER_X962 DER & ANS X9.62 utilities
  * @{
  */

/* Private types -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/** @addtogroup KMS_DER_X962_Exported_Functions Exported Functions
  * @{
  */
/**
  * @brief  This function compute length of an DER octet string
  * @param  pDER DER Octet string pointer
  * @param  pLen Length of the data within the DER octet string
  * @retval CKR_OK
  *         CKR_FUNCTION_FAILED
  */
CK_RV KMS_DerX962_OctetStringLength(uint8_t *pDER, uint32_t *pLen)
{
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  uint8_t tmp;
  uint8_t *ptr = pDER;

  if (pDER[0] == KMS_DER_OCTET_STRING)
  {
    ptr++;
    /* ASN.1 encoding:
     * (T,L,V): <1 byte type> <1 or more bytes length><x data>
     * 0x04: Type Octet String
     * 1 byte length if inferior to 128 bytes
     * more bytes length otherwise: first byte is 0x80 + number of bytes that will encode length
     * ex: Length = 0x23 => 1 byte length: 0x23
     *        Length = 0x89 => 2 bytes length: 0x81 0x89
     *        Length = 0x123 => 3 bytes length: 0x82 0x01 0x23
     */
    if ((ptr[0] & 0x80U) == 0x80U)
    {
      tmp = ptr[0] & 0x7FU;
      if (tmp > 4U)
      {
        e_ret_status = CKR_FUNCTION_FAILED;
      }
      else
      {
        for (uint8_t i = 0; i < tmp; i++)
        {
          *pLen = (uint32_t)(*pLen << 8);
          *pLen = *pLen + (uint32_t)(ptr[i + 1U]);
        }
        e_ret_status = CKR_OK;
      }
    }
    else
    {
      *pLen = ptr[0];
      e_ret_status = CKR_OK;
    }
  }
  return e_ret_status;
}

/**
  * @brief  This function compute the data offset of an DER octet string
  * @param  pDER DER Octet string pointer
  * @param  pOff Offset of the data within the DER octet string
  * @retval CKR_OK
  *         CKR_FUNCTION_FAILED
  */
CK_RV KMS_DerX962_OctetStringDataOffset(uint8_t *pDER, uint32_t *pOff)
{
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  uint8_t *ptr = pDER;

  if (ptr[0] == KMS_DER_OCTET_STRING)
  {
    ptr++;
    /* ASN.1 encoding:
     * (T,L,V): <1 byte type> <1 or more bytes length><x data>
     * 0x04: Type Octet String
     * 1 byte length if inferior to 128 bytes
     * more bytes length otherwise: first byte is 0x80 + number of bytes that will encode length
     * ex: Length = 0x23 => 1 byte length: 0x23
     *        Length = 0x89 => 2 bytes length: 0x81 0x89
     *        Length = 0x123 => 3 bytes length: 0x82 0x01 0x23
     */
    if ((ptr[0] & 0x80U) == 0x80U)
    {
      *pOff = 2UL + ((uint32_t)(ptr[0]) & 0x7FUL);
    }
    else
    {
      *pOff = 2;
    }
    e_ret_status = CKR_OK;
  }
  return e_ret_status;
}

/**
  * @brief  This function extract from a DER encoded octet string the X & Y coordinates of a EC public key
  * @param  pDER DER Octet string pointer
  * @param  pX   X coordinates buffer pointer
  * @param  pY   Y coordinates buffer pointer
  * @param  ksize Size of each coordinate of the public key
  * @retval CKR_OK
  *         CKR_FUNCTION_FAILED
  */
CK_RV KMS_DerX962_ExtractPublicKeyCoord(uint8_t *pDER, uint8_t *pX, uint8_t *pY, uint32_t ksize)
{
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  uint32_t offset_for_data;

  /* We expect an EC Point in DER format uncompressed */
  if (KMS_DerX962_OctetStringDataOffset(pDER, &offset_for_data) == CKR_OK)
  {
    if (KMS_IS_X962_UNCOMPRESSED_FORM(pDER[offset_for_data]))
    {
      /* Copy the Pub_x */
      (void)memcpy(pX, &pDER[offset_for_data + 1UL], (size_t)ksize);

      /* Copy the Pub_y */
      (void)memcpy(pY, &pDER[offset_for_data + 1UL + ksize], (size_t)ksize);

      e_ret_status = CKR_OK;
    }
  }
  return e_ret_status;
}

/**
  * @brief  This function construct a DER encoded octet string from the X & Y coordinates of a EC public key
  * @param  pX   X coordinates buffer pointer
  * @param  pY   Y coordinates buffer pointer
  * @param  ksize Size of each coordinate of the public key
  * @param  pDER  DER Octet string pointer
  * @param  dsize Size of the DER octet string
  * @retval CKR_OK
  *         CKR_FUNCTION_FAILED
  */
CK_RV KMS_DerX962_ConstructDERPublicKeyCoord(uint8_t *pX, uint8_t *pY, uint32_t ksize, uint8_t *pDER, uint32_t *dsize)
{
  CK_RV e_ret_status = CKR_FUNCTION_FAILED;
  uint32_t x962_length;
  uint32_t wr_index = 0;

  /* ksize * 2 - 1 must not overload a uint32_t, so check is done to ensure this prior to any computing */
  if (ksize <= ((0xFFFFFFFFU - 1U) / 2U))
  {
    /* DER octet string */
    pDER[wr_index] = KMS_DER_OCTET_STRING;
    wr_index++;

    /* ASN.1 encoding:
     * (T,L,V): <1 byte type> <1 or more bytes length><x data>
     * 0x04: Type Octet String
     * 1 byte length if inferior to 128 bytes
     * more bytes length otherwise: first byte is 0x80 + number of bytes that will encode length
     * ex: Length = 0x23 => 1 byte length: 0x23
     *        Length = 0x89 => 2 bytes length: 0x81 0x89
     *        Length = 0x123 => 3 bytes length: 0x82 0x01 0x23
     */
    x962_length = (ksize * 2UL) + 1UL; /* To store X, Y and uncompressed format info */
    if (x962_length < 0x80UL)
    {
      pDER[wr_index] = (uint8_t)x962_length;
      wr_index++;
    }
    else if (x962_length <= 0xFFUL)
    {
      pDER[wr_index] = 0x81U;
      wr_index++;
      pDER[wr_index] = (uint8_t)((x962_length) & 0xFFUL);
      wr_index++;
    }
    else if (x962_length <= 0xFFFFUL)
    {
      pDER[wr_index] = 0x82U;
      wr_index++;
      pDER[wr_index  ] = (uint8_t)((x962_length >> 8) & 0xFFUL);
      pDER[wr_index + 1UL] = (uint8_t)((x962_length) & 0xFFUL);
      wr_index += 2UL;

    }
    else if (x962_length <= 0xFFFFFFUL)
    {
      pDER[wr_index] = 0x83U;
      wr_index++;
      pDER[wr_index  ] = (uint8_t)((x962_length >> 16) & 0xFFUL);
      pDER[wr_index + 1UL] = (uint8_t)((x962_length >> 8) & 0xFFUL);
      pDER[wr_index + 2UL] = (uint8_t)((x962_length) & 0xFFUL);
      wr_index += 3UL;
    }
    else
    {
      pDER[wr_index] = 0x84U;
      wr_index++;
      pDER[wr_index  ] = (uint8_t)((x962_length >> 24) & 0xFFUL);
      pDER[wr_index + 1UL] = (uint8_t)((x962_length >> 16) & 0xFFUL);
      pDER[wr_index + 2UL] = (uint8_t)((x962_length >> 8) & 0xFFUL);
      pDER[wr_index + 3UL] = (uint8_t)((x962_length) & 0xFFUL);
      wr_index += 4UL;
    }
    /* X9.62 uncompressed format */
    pDER[wr_index] = KMS_X962_UNCOMPRESSED_FORM;
    wr_index++;
    /* X coordinate */
    (void)memcpy(&(pDER[wr_index]), pX, ksize);
    wr_index += ksize;
    /* Y coordinate */
    (void)memcpy(&(pDER[wr_index]), pY, ksize);

    *dsize = wr_index + ksize;

    e_ret_status = CKR_OK;
  }
  return e_ret_status;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
