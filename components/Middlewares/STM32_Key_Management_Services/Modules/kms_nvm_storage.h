/**
  ******************************************************************************
  * @file    kms_nvm_storage.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module Non Volatile Memory storage services.
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
#ifndef KMS_NVM_STORAGE_H
#define KMS_NVM_STORAGE_H

#if defined(KMS_NVM_ENABLED)

#include "nvms_low_level.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_NVM NVM storage
  * @{
  */


/*===========================================================================*/
/* Module constants                                                          */
/*===========================================================================*/

/**
  * @brief   Magic number 1.
  */
#define NVMS_HEADER_MAGIC1                    0x5AA5F731U

/**
  * @brief   Magic number 2.
  */
#define NVMS_HEADER_MAGIC2                    0x137FA55AU

/**
  * @brief   Slot number for main block header.
  */
#define NVMS_SLOT_MAIN_HEADER                  0U

/**
  * @brief   Default data type value.
  */
#define NVMS_DATA_TYPE_DEFAULT                 0xFFFFFFFFU



/*===========================================================================*/
/* Module pre-compile time settings                                          */
/*===========================================================================*/

/**
  * @brief   Maximum number of storage repair attempts on initialization.
  */
#if !defined(NVMS_CFG_MAX_REPAIR_ATTEMPTS) || defined(__DOXYGEN__)
#define NVMS_CFG_MAX_REPAIR_ATTEMPTS          3U
#endif /* !defined(NVMS_CFG_MAX_REPAIR_ATTEMPTS) || defined(__DOXYGEN__) */

/**
  * @brief   Maximum number of distinct slots.
  * @details The slot identifier will range from 0 to @p NVMS_CFG_NUM_SLOTS-1.
  */
#if !defined(NVMS_CFG_NUM_SLOTS) || defined(__DOXYGEN__)
#define NVMS_CFG_NUM_SLOTS                     (KMS_NVM_SLOT_NUMBERS)
#endif /* !defined(NVMS_CFG_NUM_SLOTS) || defined(__DOXYGEN__) */

/**
  * @brief   Enforces a read for verification after a write.
  */
#if !defined(NVMS_CFG_WRITE_VERIFY) || defined(__DOXYGEN__)
#define NVMS_CFG_WRITE_VERIFY                 TRUE
#endif /* !defined(NVMS_CFG_WRITE_VERIFY) || defined(__DOXYGEN__) */

/*===========================================================================*/
/* Derived constants and error checks                                        */
/*===========================================================================*/

/*===========================================================================*/
/* Module data structures and types                                          */
/*===========================================================================*/

/**
  * @brief   Type of a block status.
  */
typedef enum
{
  NVMS_STATUS_ERASED = 0,
  NVMS_STATUS_NORMAL = 1,
  NVMS_STATUS_PARTIAL = 2,
  NVMS_STATUS_BROKEN = 3
} nvms_block_status_t;

/**
  * @brief   Type of a slot status.
  */
typedef enum
{
  NVMS_SLOT_STATUS_ERASED = 0,
  NVMS_SLOT_STATUS_OK = 1,
  NVMS_SLOT_STATUS_CRC = 2,
  NVMS_SLOT_STATUS_BROKEN = 3
} nvms_slot_status_t;

/**
  * @brief   Type of an error.
  */
typedef enum
{
  NVMS_NOERROR = 0,
  NVMS_NOTINIT = 1,
  NVMS_WARNING = 2,
  NVMS_FLASH_FAILURE = 3,
  NVMS_SLOT_INVALID = 4,
  NVMS_DATA_NOT_FOUND = 5,
  NVMS_CRC = 6,
  NVMS_OUT_OF_MEM = 7,
  NVMS_INTERNAL = 8
} nvms_error_t;

/**
  * @brief   Type of a warning.
  */
typedef enum
{
  NVMS_WARNING_ONE_BLOCK_CORRUPTED  = 0,  /* One Block is erased, and the other is partially corrupted  */
  NVMS_WARNING_ONE_BLOCK_BROKEN = 1,      /* One Block is erased, and the orher is broken  */
  NVMS_WARNING_TWO_BLOCKS_NORMAL = 2,     /* 2 Blocks are marked as NORMAL */
  NVMS_WARNING_TWO_BLOCK_CORRUPTED = 3,   /* Both block appear to be partially corrupted, */
  NVMS_WARNING_ONE_BLOCK_PARTIAL = 4,     /* One Block is normal, the other is partial */
  NVMS_WARNING_ONE_BLOCK_PARTIAL_AND_ONE_BROKEN = 5,  /* One Block is partial, the other is briken */
  NVMS_WARNING_TWO_BLOCKS_BROKEN = 6                  /* Two blocks are Broken */
} nvms_warning_t;


/**
  * @brief   Slot identifier type.
  */
typedef uint32_t nvms_slot_t;

/**
  * @brief   Data type.
  */
typedef uint32_t nvms_data_type_t;

/*
  * @brief   Header of a data in flash.
  */
typedef union nvms_data_header nvms_data_header_t;

/*
  * @brief   Header of data in flash.
  */
union nvms_data_header
{
  struct
  {
    uint32_t                    magic1;           /*!< First magic */
    uint32_t                    magic2;           /*!< Second magic */
    nvms_slot_t                 slot;             /*!< Object slot number */
    uint32_t                    instance;         /*!< Instance number of this object */
    nvms_data_header_t          *next;            /*!< Next unused space into storage area */
    nvms_data_type_t            data_type;        /*!< Data type */
    size_t                      data_size;        /*!< Object data size */
    uint32_t                    data_checksum;    /*!< Object data checksum */
  } fields;                                       /*!< NVM data header fields */
  uint8_t                       hdr8[32];         /*!< Alias to access fields with byte resolution */
  uint32_t                      hdr32[8];         /*!< Alias to access fields with 4-bytes resolution */
};

/**
  * @brief   Key Storage internal state structure.
  */
struct nvms_state
{
  /**
    * @brief   Pointer to the current block header.
    * @note    It is @p NULL if the slots have to be re-scanned.
    */
  nvms_data_header_t         *header;

  /**
    * @brief   Block in use.
    */
  nvms_block_t             block;

  /**
    * @brief   Buffer of the current slots.
    */
  nvms_data_header_t         *slots[NVMS_CFG_NUM_SLOTS];

  /**
    * @brief   Pointer to the first free word of flash.
    */
  nvms_data_header_t         *free_next;

  /**
    * @brief   Size used by the data and headers.
    * @note    The size of older data instances is not included in this
    *          value, the size of erase headers is not included too.
    */
  size_t                used_size;
};

/*===========================================================================*/
/* Module macros                                                             */
/*===========================================================================*/

/**
  * @brief   Adds or updates a data.
  * @details If the slot is empty then a new slot is added, else the
  *          existing slot is updated.
  *
  * @param[in] slot          slot number
  * @param[in] size          data size
  * @param[in] data_p        pointer to the data
  *
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_WARNING       if some repair work has been performed or
  *                            garbage collection happened.
  * @retval NVMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval NVMS_KEY_INVALID   if the key identifier is out of range.
  * @retval NVMS_FLASH_FAILURE if the flash memory is unusable because HW
  *                            failures.
  * @retval NVMS_OUT_OF_MEM    if the key space is exhausted.
  * @retval NVMS_INTERNAL      if an internal error occurred.
  */
#define NVMS_WRITE_DATA(slot, size, data_p)                                         \
  NVMS_WriteDataWithType((slot), (size), NVMS_DATA_TYPE_DEFAULT, (data_p))

/*
  * @brief   Retrieves data for a given slot.
  * @note    The returned pointer is valid only until to the next call to
  *          the subsystem because data can be updated and moved in the flash
  *          array.
  *
  * @param[in] slot          slot number
  * @param[out] size_p       pointer to a variable that will contain the data
  *                          size after the call
  * @param[out] key_pp       pointer to a pointer to the data
  *
  * @return                     The operation status.
  * @retval NVMS_NOERROR        if the operation has been successfully completed.
  * @retval NVMS_NOTINIT        if the subsystem has not been initialized yet.
  * @retval NVMS_SLOT_INVALID   if the slot is out of range.
  * @retval NVMS_DATA_NOT_FOUND if the data does not exists.
  */
#define NVMS_GET_DATA(slot, size_p, key_pp)                                         \
  NVMS_GetDataWithType((slot), (size_p), NULL, (key_pp))

/*===========================================================================*/
/* External declarations                                                     */
/*===========================================================================*/

nvms_error_t NVMS_Init(void);
void NVMS_Deinit(void);
nvms_error_t NVMS_Erase(void);
nvms_error_t NVMS_WriteDataWithType(nvms_slot_t slot, size_t size, nvms_data_type_t type,
                                    const uint8_t *data_p);
nvms_error_t NVMS_EraseData(nvms_slot_t slot);
nvms_error_t NVMS_GetDataWithType(nvms_slot_t slot, size_t *size_p, nvms_data_type_t *type_p,
                                  uint8_t **key_pp);

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions                                                   */
/*===========================================================================*/

#endif /* KMS_NVM_ENABLED */
#endif /* KMS_NVM_STORAGE_H */
