/**
  ******************************************************************************
  * @file    kms_vm_storage.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          module Volatile Memory storage services.
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
#ifndef KMS_VM_STORAGE_H
#define KMS_VM_STORAGE_H

#if defined(KMS_VM_DYNAMIC_ENABLED)

#include "vms_low_level.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_VM VM storage
  * @{
  */


/*===========================================================================*/
/* Module constants                                                          */
/*===========================================================================*/

/**
  * @brief   Magic number 1.
  */
#define VMS_HEADER_MAGIC1                     0x5AA5F731U

/**
  * @brief   Magic number 2.
  */
#define VMS_HEADER_MAGIC2                     0x137FA55AU

/**
  * @brief   Slot number for main header.
  */
#define VMS_SLOT_MAIN_HEADER                  0U

/**
  * @brief   Default data type value.
  */
#define VMS_DATA_TYPE_DEFAULT                 0xFFFFFFFFU



/*===========================================================================*/
/* Module pre-compile time settings                                          */
/*===========================================================================*/

/**
  * @brief   Maximum number of storage repair attempts on initialization.
  */
#if !defined(VMS_CFG_MAX_REPAIR_ATTEMPTS) || defined(__DOXYGEN__)
#define VMS_CFG_MAX_REPAIR_ATTEMPTS          3U
#endif /* !defined(VMS_CFG_MAX_REPAIR_ATTEMPTS) || defined(__DOXYGEN__) */

/**
  * @brief   Maximum number of distinct slots.
  * @details The slot identifier will range from 0 to @p VMS_CFG_NUM_SLOTS-1.
  */
#if !defined(VMS_CFG_NUM_SLOTS) || defined(__DOXYGEN__)
#define VMS_CFG_NUM_SLOTS                     (KMS_VM_SLOT_NUMBERS)
#endif /* !defined(VMS_CFG_NUM_SLOTS) || defined(__DOXYGEN__) */

/**
  * @brief   Enforces a read for verification after a write.
  */
#if !defined(VMS_CFG_WRITE_VERIFY) || defined(__DOXYGEN__)
#define VMS_CFG_WRITE_VERIFY                 TRUE
#endif /* !defined(VMS_CFG_WRITE_VERIFY) || defined(__DOXYGEN__) */

/*===========================================================================*/
/* Derived constants and error checks                                        */
/*===========================================================================*/

/*===========================================================================*/
/* Module data structures and types                                          */
/*===========================================================================*/

/**
  * @brief   Type of the data storage status.
  */
typedef enum
{
  VMS_STATUS_ERASED = 0,
  VMS_STATUS_NORMAL = 1,
  VMS_STATUS_CORRUPTED = 2,
  VMS_STATUS_BROKEN = 3
} vms_data_storage_status_t;

/**
  * @brief   Type of a slot status.
  */
typedef enum
{
  VMS_SLOT_STATUS_ERASED = 0,
  VMS_SLOT_STATUS_OK = 1,
  VMS_SLOT_STATUS_CRC = 2,
  VMS_SLOT_STATUS_BROKEN = 3
} vms_slot_status_t;

/**
  * @brief   Type of an error.
  */
typedef enum
{
  VMS_NOERROR = 0,
  VMS_NOTINIT = 1,
  VMS_WARNING = 2,
  VMS_RAM_FAILURE = 3,
  VMS_SLOT_INVALID = 4,
  VMS_DATA_NOT_FOUND = 5,
  VMS_CRC = 6,
  VMS_OUT_OF_MEM = 7,
  VMS_INTERNAL = 8
} vms_error_t;

/**
  * @brief   Type of a warning.
  */
typedef enum
{
  VMS_WARNING_DATA_STORAGE_NORMAL = 0,    /* Data storage is are marked as NORMAL */
  VMS_WARNING_DATA_STORAGE_CORRUPTED = 1, /* Data storage is partially corrupted, */
  VMS_WARNING_DATA_STORAGE_BROKEN = 2     /* Data storage is Broken */
} vms_warning_t;


/**
  * @brief   Slot identifier type.
  */
typedef uint32_t vms_slot_t;

/**
  * @brief   Data type.
  */
typedef uint32_t vms_data_type_t;

/*
  * @brief   Header of a data in RAM.
  */
typedef union vms_data_header vms_data_header_t;

/*
  * @brief   Header of data in RAM.
  */
union vms_data_header
{
  struct
  {
    uint32_t                    magic1;           /*!< First magic */
    uint32_t                    magic2;           /*!< Second magic */
    vms_slot_t                  slot;             /*!< Object slot number */
    vms_data_header_t          *next;             /*!< Next unused space into storage area */
    vms_data_type_t             data_type;        /*!< Data type */
    size_t                      data_size;        /*!< Object data size */
    uint32_t                    data_checksum;    /*!< Object data checksum */
  } fields;                                       /*!< VM data header fields */
  uint8_t                       hdr8[32];         /*!< Alias to access fields with byte resolution */
  uint32_t                      hdr32[8];         /*!< Alias to access fields with 4-bytes resolution */
};

/**
  * @brief   Key Storage internal state structure.
  */
struct vms_state
{
  /**
    * @brief   Pointer to the header.
    * @note    It is @p NULL if the slots have to be re-scanned.
    */
  vms_data_header_t         *header;

  /**
    * @brief   Buffer of the current slots.
    */
  vms_data_header_t         *slots[VMS_CFG_NUM_SLOTS];

  /**
    * @brief   Pointer to the first free word of RAM.
    */
  vms_data_header_t         *free_next;

  /**
    * @brief   Size used by the data and headers.
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
  * @retval VMS_NOERROR        if the operation has been successfully completed.
  * @retval VMS_WARNING        if some repair work has been performed or
  *                            garbage collection happened.
  * @retval VMS_NOTINIT        if the subsystem has not been initialized yet.
  * @retval VMS_KEY_INVALID    if the key identifier is out of range.
  * @retval VMS_OUT_OF_MEM     if the key space is exhausted.
  * @retval VMS_INTERNAL       if an internal error occurred.
  */
#define VMS_WRITE_DATA(slot, size, data_p)                                         \
  VMS_WriteDataWithType((slot), (size), VMS_DATA_TYPE_DEFAULT, (data_p))

/*
  * @brief   Retrieves data for a given slot.
  * @note    The returned pointer is valid only until to the next call to
  *          the subsystem because data can be updated and moved in the RAM
  *          array.
  *
  * @param[in] slot          slot number
  * @param[out] size_p       pointer to a variable that will contain the data
  *                          size after the call
  * @param[out] key_pp       pointer to a pointer to the data
  *
  * @return                     The operation status.
  * @retval VMS_NOERROR         if the operation has been successfully completed.
  * @retval VMS_NOTINIT         if the subsystem has not been initialized yet.
  * @retval VMS_SLOT_INVALID    if the slot is out of range.
  * @retval VMS_DATA_NOT_FOUND  if the data does not exists.
  */
#define VMS_GET_DATA(slot, size_p, key_pp)                                         \
  VMS_GetDataWithType((slot), (size_p), NULL, (key_pp))

/*===========================================================================*/
/* External declarations                                                     */
/*===========================================================================*/

vms_error_t VMS_Init(void);
void VMS_Deinit(void);
vms_error_t VMS_Erase(void);
vms_error_t VMS_WriteDataWithType(vms_slot_t slot, size_t size, vms_data_type_t type,
                                  const uint8_t *data_p);
vms_error_t VMS_EraseData(vms_slot_t slot);
vms_error_t VMS_GetDataWithType(vms_slot_t slot, size_t *size_p, vms_data_type_t *type_p,
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

#endif /* KMS_VM_DYNAMIC_ENABLED */
#endif /* KMS_VM_STORAGE_H */
