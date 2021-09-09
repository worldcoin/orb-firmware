/**
  ******************************************************************************
  * @file    ca_types.h
  * @author  MCD Application Team
  * @brief   This file contains the type definitions of the Cryptographic API (CA) module.
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
#ifndef CA_TYPES_H
#define CA_TYPES_H

/* To avoid ST Cryptographic library header inclusion */
#if !defined(__CRL_SK_H__)
#define __CRL_SK_H__
#endif /* __CRL_SK_H__ */
#if !defined(__CRL_BN_H__)
#define __CRL_BN_H__
#endif /* __CRL_BN_H__ */
#if !defined(__CRL_TYPES_H__)
#define __CRL_TYPES_H__
#endif /* __CRL_TYPES_H__ */


/* Configuration defines -----------------------------------------------------*/



/* Includes ------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Symmetric Key Flags
  */
typedef enum
{
  CA_E_SK_DEFAULT = (uint32_t)(0x00000000),                       /*!< User Flag: No flag specified. This is the default
                                                                       value that should be set to this flag  */
  CA_E_SK_DONT_PERFORM_KEY_SCHEDULE = (uint32_t)(0x00000001),     /*!< User Flag: Used to force the init to not
                                                                       reperform key schedule.\n
                                                                       The classic example is where the same key is used
                                                                       on a new message, in this case to redo key
                                                                       scheduling is a useless waste of computation,
                                                                       could be particularly useful on GCM, where key
                                                                       schedule is very complicated  */
  CA_E_SK_FINAL_APPEND = (uint32_t)(0x00000020),                  /*!< User Flag: Must be set in CMAC mode before the
                                                                       final Append call occurs  */
  CA_E_SK_OPERATION_COMPLETED  = (uint32_t)(0x00000002),          /*!< Internal Flag (not to be set/read by user): used
                                                                       to check that the Finish function has been
                                                                       already called */
  CA_E_SK_NO_MORE_APPEND_ALLOWED = (uint32_t)(0x00000004),        /*!< Internal Flag (not to be set/read by user): it is
                                                                       set when the last append has been called. Used
                                                                       where the append is called with an InputSize not
                                                                       multiple of the block size, which means that is
                                                                       the last input */
  CA_E_SK_NO_MORE_HEADER_APPEND_ALLOWED = (uint32_t)(0x00000010), /*!< Internal Flag (not to be set/read by user): only
                                                                       for authenticated encryption modes. It is set
                                                                       when the last header append has been called.
                                                                       Used where the header append is called with an
                                                                       InputSize not multiple of the block size, which
                                                                       means that is the last input */
  CA_E_SK_APPEND_DONE = (uint32_t)(0x00000040),                   /*!< Internal Flag (not to be set/read by user): only
                                                                       for CMAC.It is set when the first append has been
                                                                       called */
  CA_E_SK_SET_COUNTER = (uint32_t)(0x00000080),                   /*!< User Flag: With ChaCha20 this is used to indicate
                                                                       a value for the counter, used to process non
                                                                       contiguous blocks (i.e. jump ahead)*/
} CA_SKflags_et;
typedef CA_SKflags_et SKflags_et;   /*!< Symmetric Key Flags */

/**
  * @brief  Structure used to store a BigNum_stt * integer.
  */
typedef struct
{
  uint32_t *pmDigit;    /*!<  Used to represent the BigNum_stt * integer value; pmDigit[0] = least significant word  */
  uint16_t mNumDigits;  /*!<  Number of significant words of the vector pmDigit used to represent the actual value  */
  uint8_t  mSize;       /*!<  Number of words allocated for the integer */
  int8_t   mSignFlag;   /*!<  Is the integer mSignFlag: CA_SIGN_POSITIVE positive, CA_SIGN_NEGATIVE negative  */
} CA_BigNum_stt;
typedef CA_BigNum_stt BigNum_stt;   /*!< Structure used to store a BigNum_stt * integer */

/** @brief Type definitation for a pre-allocated memory buffer that is required by some functions */
typedef struct
{
  uint8_t *pmBuf;  /*!< Pointer to the pre-allocated memory buffer, this must be set by the user*/
  uint16_t  mSize; /*!< Total size of the pre-allocated memory buffer */
  uint16_t  mUsed; /*!< Currently used portion of the buffer, should be inititalized by user to zero */
} CA_membuf_stt;
typedef CA_membuf_stt membuf_stt;   /*!< Type definitation for a pre-allocated memory buffer that is required by some functions */

#ifdef __cplusplus
}
#endif

#endif /* CA_TYPES_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

