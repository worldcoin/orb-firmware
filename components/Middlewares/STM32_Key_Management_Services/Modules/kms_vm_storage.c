/**
  ******************************************************************************
  * @file    kms_vm_storage.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"
#if defined(KMS_ENABLED)
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kms_vm_storage.h"

#if defined(KMS_VM_DYNAMIC_ENABLED)
/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_VM VM storage
  * @{
  */

/* Private types -------------------------------------------------------------*/
/**
  * @brief   Type for a slot scan callback function.
  */
typedef void (*vms_found_slot_t)(vms_data_header_t *hdr_p);

/**
  * @brief   Type for a slots scan callback function.
  */
typedef void (*vms_end_slot_t)(vms_data_header_t *hdr_p);
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/**
  * @brief   Error check helper.
  */
#define CHECK_ERROR(err) do {                                               \
                              vms_error_t e = (err);                        \
                              if (e != VMS_NOERROR) {                       \
                                return e;                                   \
                              }                                             \
                            } while (false)

/* Private variables ---------------------------------------------------------*/

/**
  * @brief   VM Storage internal state.
  * @details This variable is public for testability.
  */
static struct vms_state vm;

#if defined(KMS_DEBUG_MODE)
uint32_t  latest_warning;
#endif /* KMS_DEBUG_MODE */

/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/**
  * @brief   Determines if the data storage is in erased state.
  *
  * @return                  The data storage state.
  * @retval false            if the data storage is not in erased state.
  * @retval true             if the data storage is in erased state.
  */
static bool is_data_storage_erased(void)
{
  uint32_t i;
  uint32_t *p = (uint32_t *)(VMS_LL_GetDataStorageAddress());

  for (i = 0; i < (VMS_LL_GetDataStorageSize() / sizeof(uint32_t)); i++)
  {
    if (*p != VMS_LL_ERASED)
    {
      return false;
    }
    p++;
  }
  return true;
}

/**
  * @brief   Erases the data storage.
  *
  * @return                  The error code.
  * @retval VMS_NOERROR       if the operation is successful.
  * @retval VMS_RAM_FAILURE   if the operation failed while writing.
  */
static vms_error_t data_storage_erase(void)
{
  uint32_t *p = (uint32_t *)(VMS_LL_GetDataStorageAddress());

  /* Erasing of the RAM data storage.*/
  (void)memset(p, VMS_LL_ERASED, VMS_LL_GetDataStorageSize());

  if (!is_data_storage_erased())
  {
    return VMS_RAM_FAILURE;
  }
  return VMS_NOERROR;
}

/**
  * @brief   Writes data.
  * @note    The write operation is verified internally.
  * @note    If the write operation partially writes pages then the
  *          unwritten bytes are left to the filler value.
  *
  * @param[in] source        pointer to the data to be written
  * @param[in] destination   pointer to the position
  * @param[in] size          size of data
  *
  * @return                  The operation status.
  * @retval false            if the operation is successful.
  * @retval true             if the write operation failed.
  */
static bool write(const uint8_t *source, uint8_t *destination, size_t size)
{
  (void)memcpy(destination, source, size);

  /* Operation verification.*/
  return (bool)(memcmp(source, destination, size) != 0);
}

/**
  * @brief   Invalidates the current state.
  */
static void reset(void)
{
  uint32_t i;

  vm.header = NULL;
  for (i = 0; i < VMS_CFG_NUM_SLOTS; i++)
  {
    vm.slots[i] = NULL;
  }
  vm.free_next = NULL;
  vm.used_size = 0;
}

/**
  * @brief   Returns the initialization status.
  *
  * @return                 The initialization status.
  * @retval false            if the subsystem is not initialized.
  * @retval true             if the subsystem is initialized.
  */
static inline bool is_initialized(void)
{
  return vm.header != NULL;
}

/**
  * @brief   Calculates checksum.
  *
  * @param[in] data_p        pointer to the data buffer
  * @param[in] size          size of the data buffer
  * @return                  The checksum.
  */
static uint32_t do_checksum(const uint8_t *data_p, size_t size)
{
  uint32_t checksum;
  const uint8_t *p = data_p;
  size_t n = size;

  checksum = 0;
  while (n != 0UL)
  {
    checksum += *p;
    p++;
    n--;
  }
  return checksum;
}

/**
  * @brief   Check for a valid slot.
  *
  * @param[in] hdrp          pointer to the data header
  *
  * @return                        The slot state.
  * @retval VMS_SLOT_STATUS_ERASED if the header is erased.
  * @retval VMS_SLOT_STATUS_OK     if the header is valid and the slot matches the
  *                                CRC.
  * @retval VMS_SLOT_STATUS_CRC    if the slot does not match the CRC.
  * @retval VMS_SLOT_STATUS_BROKEN if the header is corrupt.
  */
static vms_slot_status_t check_slot(vms_data_header_t *hdrp)
{
  uint8_t *startp;
  const uint8_t *endp;
  uint32_t checksum;
  uint8_t i;

  /* First check for an header in erased state */
  for (i = 0; i < 8U; i++)
  {

    /* If the header is not in erased state then it must be checked for
       validity */
    if (hdrp->hdr32[i] != VMS_LL_ERASED)
    {
      /* Check on the pointer, it must be aligned to an header boundary */
      if (((uint32_t)hdrp->fields.next & (VMS_LL_PAGE_SIZE - 1UL)) != 0UL)
      {
        return VMS_SLOT_STATUS_BROKEN;
      }

      /* Checks on the pointer, the address must be comprised between
         the next header position and the end of the ram array */
      startp = (uint8_t *)VMS_LL_GetDataStorageAddress();
      endp = &startp[VMS_LL_GetDataStorageSize()];
      if ((hdrp->fields.next->hdr8 < (hdrp->hdr8 + sizeof(vms_data_header_t))) ||
          (hdrp->fields.next->hdr8 > endp))
      {
        return VMS_SLOT_STATUS_BROKEN;
      }

      /* Check on the magic numbers */
      if ((hdrp->fields.magic1 != VMS_HEADER_MAGIC1) ||
          (hdrp->fields.magic2 != VMS_HEADER_MAGIC2))
      {
        return VMS_SLOT_STATUS_BROKEN;
      }

      /* Check on slot identifier */
      if (hdrp->fields.slot >= VMS_CFG_NUM_SLOTS)
      {
        return VMS_SLOT_STATUS_BROKEN;
      }

      /* Checks on the data size */
      if ((hdrp->hdr8 + sizeof(vms_data_header_t) + hdrp->fields.data_size) >
          hdrp->fields.next->hdr8)
      {
        return VMS_SLOT_STATUS_BROKEN;
      }

      /* Payload Checksum */
      checksum = do_checksum(hdrp->hdr8 + sizeof(vms_data_header_t),
                             hdrp->fields.data_size);
      if (checksum != hdrp->fields.data_checksum)
      {
        return VMS_SLOT_STATUS_CRC;
      }

      return VMS_SLOT_STATUS_OK;
    }
  }

  return VMS_SLOT_STATUS_ERASED;
}

/**
  * @brief   Scans the data storage searching for slots.
  * @note    The data storage integrity is strongly checked.
  *
  * @param[in] slotcallback  callback to be called for each found slot
  * @param[in] endcallback   callback to be called after scanning.
  *
  * @return                  The data storage state.
  * @retval VMS_STATUS_NORMAL    if the data storage contains valid data.
  * @retval VMS_STATUS_CORRUPTED if the data storage contains errors but
  *                              the data is still readable.
  * @retval VMS_STATUS_BROKEN    if the data storage contains unreadable garbage. The
  *                              @p endcallback function is not called in this
  *                              case.
  */
static vms_data_storage_status_t scan_slots(vms_found_slot_t slotcallback,
                                            vms_end_slot_t endcallback)
{
  vms_data_header_t *hdrp;
  uint8_t *startp;
  const uint8_t *endp;
  vms_slot_status_t slotsts;
  bool warning = false;
  vms_data_storage_status_t status = VMS_STATUS_BROKEN;
  bool status_found = false;

  /* Limits */
  startp = (uint8_t *)VMS_LL_GetDataStorageAddress();
  endp = &startp[VMS_LL_GetDataStorageSize()];

  /* Checking the main header */
  hdrp = (vms_data_header_t *)(uint32_t)startp;
  slotsts = check_slot(hdrp);
  if (slotsts != VMS_SLOT_STATUS_OK)
  {
    return VMS_STATUS_BROKEN;
  }

  /* Scanning the slots chain */
  while (!status_found)
  {

    /* Point to next slot header */
    hdrp = (vms_data_header_t *)hdrp->fields.next;

    /* Special case end-of-chain */
    if (hdrp->hdr8 == endp)
    {
      /* Calling end-of-scan callback if defined */
      endcallback(hdrp);
      if (warning)
      {
        return VMS_STATUS_CORRUPTED;
      }
      else
      {
        return VMS_STATUS_NORMAL;
      }
    }

    /* Header check */
    slotsts = check_slot(hdrp);
    switch (slotsts)
    {
      case VMS_SLOT_STATUS_ERASED:
        /* Calling end-of-scan callback if defined */
        endcallback(hdrp);

        /* An erased header mark the end of the chain */
        if (warning)
        {
          status = VMS_STATUS_CORRUPTED;
          status_found = true;
        }
        else
        {
          status = VMS_STATUS_NORMAL;
          status_found = true;
        }
        break;

      case VMS_SLOT_STATUS_OK:
        /* Normal header, continue scan */
        slotcallback(hdrp);
        break;

      case VMS_SLOT_STATUS_CRC:
        /* Key data corrupted */
        warning = true;
#if defined(KMS_DEBUG_MODE)
        latest_warning = VMS_SLOT_STATUS_CRC;
#endif /* KMS_DEBUG_MODE */
        break;

      case VMS_SLOT_STATUS_BROKEN:
        /* Calling end-of-scan callback if defined */
        endcallback(hdrp);

        /* Problems, stopping the scan here */
        status = VMS_STATUS_CORRUPTED;
        status_found = true;
        break;

      default:
        /* No action */
        break;
    }
  }
  return status;
}

/**
  * @brief   Moves a slot to a new position.
  * @note    The slot size is supposed to be greater than zero because,
  *          zero sized slots must not be copied across banks.
  *
  * @param[out] rhdrp        pointer to the source header
  * @param[out] whdrp        pointer to the destination header
  *
  * @return                   The error code.
  * @retval VMS_NOERROR       if the operation is successful.
  * @retval VMS_RAM_FAILURE   if the operation failed while writing.
  */
static vms_error_t move_slot(const vms_data_header_t *rhdrp,
                             vms_data_header_t *whdrp)
{
  bool result;
  vms_data_header_t hdr;
  size_t size;
  uint8_t *p_next;
  uint8_t *p_free;

  size = rhdrp->fields.data_size;
  p_next = whdrp->hdr8;
  p_free = &p_next[(((sizeof(vms_data_header_t) + size - 1UL) | (VMS_LL_PAGE_SIZE - 1UL)) + 1UL)];

  /* Writing the header without the magic numbers, this way it is
     not yet validated but the write is tested */

  hdr.fields.magic1        = VMS_LL_ERASED;
  hdr.fields.magic2        = VMS_LL_ERASED;
  hdr.fields.slot          = rhdrp->fields.slot;
  hdr.fields.next          = (vms_data_header_t *)(uint32_t)p_free;
  hdr.fields.data_type     = rhdrp->fields.data_type;
  hdr.fields.data_size     = size;
  hdr.fields.data_checksum = rhdrp->fields.data_checksum;

  /* Do not write the full structure (including the 2 initial words set to the ERASED RAM value), but only the
  meaningful data
  */
  result = write(&((const uint8_t *)&hdr)[8], &p_next[8], sizeof(vms_data_header_t) - 8UL);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }

  /* Writing data, the slot size is supposed to be greater than zero
     because, zero sized slots must not be copied across bank */
  result = write(rhdrp->hdr8 + sizeof(vms_data_header_t),
                 &p_next[sizeof(vms_data_header_t)],
                 size);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }

  /* Writing the magic numbers validates the header */
  hdr.fields.magic1 = VMS_HEADER_MAGIC1;
  hdr.fields.magic2 = VMS_HEADER_MAGIC2;
  result = write((const uint8_t *)&hdr, p_next, sizeof(uint32_t) * 2UL);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }

  /* Update the VM storage internal state */
  vm.slots[hdr.fields.slot] = (vms_data_header_t *)p_next;

  return VMS_NOERROR;
}

/**
  * @brief   Remove a slot and compress the data storage
  * @note    The slot size is supposed to be greater than zero because,
  *          zero sized slots must not be copied across banks.
  *
  * @param[in] slot_to_erase  slot identifier
  *
  * @return                   The error code.
  * @retval VMS_NOERROR       if the operation is successful.
  * @retval VMS_RAM_FAILURE   if the operation failed while writing.
  */
static vms_error_t remove_slot(vms_slot_t slot_to_erase)
{
  vms_data_header_t *hdrp;
  vms_data_header_t *whdrp = NULL;
  vms_data_header_t *next_hdrp = NULL;
  vms_error_t error_status;
  vms_slot_status_t slotsts;
  bool end_found = false;

  /* Check on initialization */
  if (!is_initialized())
  {
    return VMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot_to_erase >= VMS_CFG_NUM_SLOTS)
  {
    return VMS_SLOT_INVALID;
  }

  /* Check slot presence */
  if (vm.slots[slot_to_erase] == NULL)
  {
    return VMS_DATA_NOT_FOUND;
  }

  /*  Get the slot to erase*/
  whdrp = vm.slots[slot_to_erase];
  hdrp = vm.slots[slot_to_erase];
  next_hdrp = (vms_data_header_t *)whdrp->fields.next;

  /* Compress the VM data storage */
  while (!end_found)
  {
    /* Check if the next header (to be moved) is in erased state */
    slotsts = check_slot(next_hdrp);
    if (slotsts == VMS_SLOT_STATUS_ERASED)
    {
      /* The end of the chain is found */
      end_found = true;

      /* Erase the current slot */
      (void)memset(whdrp, VMS_LL_ERASED, sizeof(vms_data_header_t));

      /* Update the VM storage internal state */
      vm.free_next = whdrp;
    }
    else if (slotsts == VMS_SLOT_STATUS_OK)
    {
      /* Point to next slot header */
      hdrp = next_hdrp;
      next_hdrp = (vms_data_header_t *)hdrp->fields.next;

      /* Move the current slot into the slot to erase */
      error_status = move_slot(hdrp, whdrp);
      if (error_status != VMS_NOERROR)
      {
        return error_status;
      }

      /* Update the slot to erase (as updated by move_slot) */
      whdrp = (vms_data_header_t *)whdrp->fields.next;
    }
    else
    {
      return VMS_INTERNAL;
    }
  }
  return VMS_NOERROR;
}

/**
  * @brief   Writes the main header validating the data storage.
  * @pre     The header area must be in erased state.
  *
  * @return                  The error code.
  * @retval VMS_NOERROR       if the operation is successful.
  * @retval VMS_RAM_FAILURE   if the operation failed while writing.
  */
static vms_error_t validate(void)
{
  bool result;
  uint8_t *dp = (uint8_t *)VMS_LL_GetDataStorageAddress();
  vms_data_header_t hdr;

  /* Writing the header without the magic numbers, this way it is
     not yet validated but the write is tested.
     Note, the header is given a slot id zero, this does not impact
     real slots */
  hdr.fields.magic1          = VMS_LL_ERASED;
  hdr.fields.magic2          = VMS_LL_ERASED;
  hdr.fields.slot            = VMS_SLOT_MAIN_HEADER;
  hdr.fields.next            = &((vms_data_header_t *)(uint32_t)dp)[1];
  hdr.fields.data_type       = VMS_LL_ERASED;
  hdr.fields.data_size       = 0;
  hdr.fields.data_checksum   = 0;

  /* Do not write the full structure (including the 2 initial words set to the ERASED RAM value), but only the
  meaningful data
  */
  result = write(&hdr.hdr8[8], &dp[8], sizeof(vms_data_header_t) - 8UL);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }

  /* Writing the magic numbers validates the header */
  hdr.fields.magic1 = VMS_HEADER_MAGIC1;
  hdr.fields.magic2 = VMS_HEADER_MAGIC2;
  result = write(hdr.hdr8, dp, sizeof(uint32_t) * 2UL);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }

  return VMS_NOERROR;
}

/**
  * @brief   Null callback.
  */
static void null_callback(vms_data_header_t *hdrp)
{

  (void)(hdrp);
}

/**
  * @brief   Private callback of @p use().
  */
static void use_slot_callback(vms_data_header_t *hdrp)
{

  vm.slots[hdrp->fields.slot] = hdrp;
}

/**
  * @brief   Private callback of @p use().
  */
static void use_end_callback(vms_data_header_t *hdrp)
{

  vm.free_next = hdrp;
}

/**
  * @brief   Put the data storage in use.
  * @note    If the data storage is being put in use then it is assumed
  *          to contain no errors of any kind.
  *
  * @return                  The operation status.
  * @retval VMS_NOERROR       if the operation has been successfully completed.
  * @retval VMS_INTERNAL      if an internal error occurred.
  */
static vms_error_t use(void)
{
  uint32_t i;
  vms_data_storage_status_t status;
  vms_data_header_t *hdrp = (vms_data_header_t *)VMS_LL_GetDataStorageAddress();

  /* Resetting state */
  reset();

  /* Global info */
  vm.header    = hdrp;
  vm.used_size = sizeof(vms_data_header_t);
  vm.free_next = &hdrp[1];

  /* The data storage should have been checked before calling use() so any
     kind of anomaly in the data storage is considered an internal error */
  status = scan_slots(use_slot_callback, use_end_callback);
  if (status != VMS_STATUS_NORMAL)
  {
    reset();
    return VMS_INTERNAL;
  }

  /* Scanning found slots */
  for (i = 0; i < VMS_CFG_NUM_SLOTS; i++)
  {
    hdrp = vm.slots[i];

    /* If the pointer is not NULL then there is a slot in this position */
    if (hdrp != NULL)
    {
      /* Zero sized slots are discarded because indicate that the slot
         has been erased */
      if (hdrp->fields.data_size == 0UL)
      {
        vm.slots[hdrp->fields.slot] = NULL;
        continue;
      }

      /* Adding the slot used space to the total */
      vm.used_size += hdrp->fields.next->hdr8 - hdrp->hdr8;
    }
  }

  return VMS_NOERROR;
}

/**
  * @brief   Determines the state of the data storage.
  *
  * @return                     The data storage state.
  * @retval VMS_STATUS_ERASED    if the data storage is fully erased.
  * @retval VMS_STATUS_NORMAL    if the data storage contains valid data.
  * @retval VMS_STATUS_CORRUPTED if the data storage contains errors but the data is still
  *                              readable.
  * @retval VMS_STATUS_BROKEN    if the data storage contains unreadable garbage.
  */
static vms_data_storage_status_t determine_data_storage_state(void)
{
  /* Special case where the data storage is fully erased */
  if (is_data_storage_erased())
  {
    return VMS_STATUS_ERASED;
  }

  /* Checking data storage integrity by just scanning it */
  return scan_slots(null_callback, null_callback);
}

/**
  * @brief   Appends a slot instance to the data storage.
  *
  * @param[in] slot           slot identifier
  * @param[in] size           slot size
  * @param[in] type           slot type
  * @param[in] slotp          pointer to the slot
  *
  * @return                  The operation status.
  * @retval VMS_NOERROR       if the operation has been successfully completed.
  * @retval VMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval VMS_SLOT_INVALID  if the slot identifier is out of range.
  * @retval VMS_RAM_FAILURE   if the ram memory is unusable because HW
  *                           failures.
  */
static vms_error_t append_slot(vms_slot_t slot, size_t size, vms_data_type_t type,
                               const uint8_t *slotp)
{
  bool result;
  vms_data_header_t hdr;
  uint8_t *p_next;
  uint8_t *p_free;

  p_next = vm.free_next->hdr8;
  p_free = &p_next[(((sizeof(vms_data_header_t) + size - 1UL) | (VMS_LL_PAGE_SIZE - 1UL)) + 1UL)];

  /* Writing the header without the magic numbers, this way it is
     not yet validated but the write is tested */
  hdr.fields.magic1             = VMS_LL_ERASED;
  hdr.fields.magic2             = VMS_LL_ERASED;
  hdr.fields.slot               = slot;
  hdr.fields.next               = (vms_data_header_t *)(uint32_t)p_free;
  hdr.fields.data_type          = type;
  hdr.fields.data_size          = size;
  hdr.fields.data_checksum      = do_checksum(slotp, size);

  /* Do not write the full structure (including the 2 initial words set to the ERASED RAM value), but only the
  meaningful data
  */
  result = write(&((const uint8_t *)&hdr)[8], &p_next[8], sizeof(vms_data_header_t) - 8UL);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }

  /* Writing data */
  if (size > 0UL)
  {
    result = write(slotp, &p_next[sizeof(vms_data_header_t)], size);
    if (result)
    {
      return VMS_RAM_FAILURE;
    }
  }

  /* Writing the magic numbers validates the header */
  hdr.fields.magic1 = VMS_HEADER_MAGIC1;
  hdr.fields.magic2 = VMS_HEADER_MAGIC2;
  result = write((const uint8_t *)&hdr,
                 vm.free_next->hdr8,
                 sizeof(uint32_t) * 2UL);
  if (result)
  {
    return VMS_RAM_FAILURE;
  }
  /* Updating the global pointer */
  vm.free_next = (vms_data_header_t *)(uint32_t)p_free;

  return VMS_NOERROR;
}

/**
  * @brief   Performs a ram initialization attempt.
  *
  * @return                  The operation status.
  * @retval VMS_NOERROR       if the operation has been successfully completed.
  * @retval VMS_WARNING       if the initialization is complete but some repair
  *                           work has been performed.
  * @retval VMS_RAM_FAILURE   if the ram memory is unusable because HW
  *                           failures.
  * @retval VMS_INTERNAL      if an internal error occurred.
  */
static vms_error_t try_boot(void)
{
  vms_data_storage_status_t sts;

  sts = determine_data_storage_state();

  /* Case 1 - Data storage erased, performs an initialization
     and starts using it */
  if (sts == VMS_STATUS_ERASED)
  {
    CHECK_ERROR(validate());
    CHECK_ERROR(use());

    return VMS_NOERROR;
  }

  /* Case 2 - Data storage is normal */
  if (sts == VMS_STATUS_NORMAL)
  {
    CHECK_ERROR(use());

    return VMS_NOERROR;
  }

  /* Case 3 - Data storage is partially corrupted */
  if (sts == VMS_STATUS_CORRUPTED)
  {
    CHECK_ERROR(use());

#if defined(KMS_DEBUG_MODE)
    latest_warning = VMS_WARNING_DATA_STORAGE_CORRUPTED;
#endif /* KMS_DEBUG_MODE */
    return VMS_WARNING;
  }

  /* Case 4 - Data storage is broken */
  CHECK_ERROR(data_storage_erase());
  CHECK_ERROR(validate());
  CHECK_ERROR(use());

#if defined(KMS_DEBUG_MODE)
  latest_warning = VMS_WARNING_DATA_STORAGE_BROKEN;
#endif /* KMS_DEBUG_MODE */

  return VMS_WARNING;
}

/* Exported variables --------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
  * @brief   Subsystem initialization.
  *
  * @return                  The operation status.
  * @retval VMS_NOERROR       if the operation has been successfully completed.
  * @retval VMS_WARNING       if the initialization is complete but some repair
  *                           work has been performed.
  * @retval VMS_RAM_FAILURE   if the RAM memory is unusable because HW
  *                           failures.
  * @retval VMS_INTERNAL      if an internal error occurred.
  */
vms_error_t VMS_Init(void)
{
  reset();

  for (uint32_t i = VMS_CFG_MAX_REPAIR_ATTEMPTS; i > 0UL; i--)
  {
    vms_error_t err = try_boot();
    if ((err == VMS_NOERROR) || (err == VMS_WARNING))
    {
      return err;
    }
  }
  return VMS_RAM_FAILURE;
}

/**
  * @brief   Subsystem de-initialization.
  * @details The function cannot fail and does nothing if the system has not
  *          been yet initialized.
  */
void VMS_Deinit(void)
{
  /* Clearing data structures */
  reset();
}

/**
  * @brief   Destroys the state of the data storage by erasing the ram.
  *
  * @return                  The operation status.
  * @retval VMS_NOERROR       if the operation has been successfully completed.
  * @retval VMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval VMS_RAM_FAILURE   if the ram memory is unusable because HW
  *                           failures.
  * @retval VMS_INTERNAL      if an internal error occurred.
  */
vms_error_t VMS_Erase(void)
{
  /* Check on initialization */
  if (!is_initialized())
  {
    return VMS_NOTINIT;
  }

  /* Erasing data_storage */
  CHECK_ERROR(data_storage_erase());

  /* Re-initializing the ram as an empty slot storage */
  CHECK_ERROR(validate());
  CHECK_ERROR(use());

  return VMS_NOERROR;
}

/**
  * @brief   Adds or updates data.
  * @details If the slot identifier is new then a new slot is added, else the
  *          existing slot is updated.
  *
  * @param[in] slot           slot identifier
  * @param[in] size           slot size
  * @param[in] type           slot type
  * @param[in] slotp          pointer to the slot
  *
  * @return                  The operation status.
  * @retval VMS_NOERROR       if the operation has been successfully completed.
  * @retval VMS_WARNING       if some repair work has been performed.
  * @retval VMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval VMS_SLOT_INVALID  if the slot identifier is out of range.
  * @retval VMS_RAM_FAILURE   if the ram memory is unusable because HW
  *                           failures.
  * @retval VMS_OUT_OF_MEM    if the slot space is exhausted.
  * @retval VMS_INTERNAL      if an internal error occurred.
  */
vms_error_t VMS_WriteDataWithType(vms_slot_t slot, size_t size, vms_data_type_t type,
                                  const uint8_t *slotp)
{
  bool warning = false;
  size_t free;
  size_t oldused;
  vms_error_t err;
  vms_data_header_t *hdrp;

  /* Check on initialization */
  if (!is_initialized())
  {
    return VMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot >= VMS_CFG_NUM_SLOTS)
  {
    return VMS_SLOT_INVALID;
  }

  /* Checking for immediately available space */
  free = VMS_LL_GetDataStorageSize() - (vm.free_next->hdr8 -
                                        vm.header->hdr8);

  /* If the requested space is out of the data storage size then an error
   * is returned.
   * NOTE: The space for one header is reserved in order to allow for a
   * data erase operation after the space has been fully allocated.
   */
  if ((sizeof(vms_data_header_t) + size) > (VMS_LL_GetDataStorageSize() -
                                            vm.used_size -
                                            sizeof(vms_data_header_t)))
  {
    return VMS_OUT_OF_MEM;
  }

  /* This condition should not happen with VM data storage */
  if ((sizeof(vms_data_header_t) + size) > free)
  {
    return VMS_INTERNAL;
  }

  /* Index for the new data */
  if (vm.slots[slot] == NULL)
  {
    oldused = 0;
  }
  else
  {
    oldused = (uint32_t)(vm.slots[slot]->fields.next->hdr8) - (uint32_t)(vm.slots[slot]->hdr8);
  }

  /* Position of the new data instance */
  hdrp = vm.free_next;

  /* Writing the new instance */
  err = append_slot(slot, size, type, slotp);
  if (err != VMS_NOERROR)
  {
    return err;
  }

  /* Adjusting the counter of the effective used size */
  vm.slots[slot]  = hdrp;
  vm.used_size -= oldused;
  vm.used_size += vm.slots[slot]->fields.next->hdr8 - vm.slots[slot]->hdr8;

  return warning ? VMS_WARNING : VMS_NOERROR;
}

/**
  * @brief  Erases a slot.
  *
  * @param[in] slot             slot identifier
  *
  * @return                   The operation status.
  * @retval VMS_NOERROR        if the operation has been successfully completed.
  * @retval VMS_WARNING        if some repair work has been performed.
  * @retval VMS_NOTINIT        if the subsystem has not been initialized yet.
  * @retval VMS_RAM_FAILURE    if the ram memory is unusable because HW.
  *                            failures.
  * @retval VMS_DATA_NOT_FOUND if the data does not exists.
  * @retval VMS_INTERNAL       if an internal error occurred.
  */
vms_error_t VMS_EraseData(vms_slot_t slot)
{
  bool warning = false;
  size_t free;
  size_t oldused;
  vms_error_t err;

  /* Check on initialization */
  if (!is_initialized())
  {
    return VMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot >= VMS_CFG_NUM_SLOTS)
  {
    return VMS_SLOT_INVALID;
  }

  /* Check slot presence */
  if (vm.slots[slot] == NULL)
  {
    return VMS_DATA_NOT_FOUND;
  }

  /* Checking for immediately available space */
  free = VMS_LL_GetDataStorageSize() - (vm.free_next->hdr8 -
                                        vm.header->hdr8);

  /* If the requested space is out of the compacted data storage size then an
     error is returned.
     NOTE: This condition SHOULD NEVER HAPPEN because the slot write operation
     makes sure to leave to leave enough space for an erase operation */
  if (sizeof(vms_data_header_t) > (VMS_LL_GetDataStorageSize() - vm.used_size))
  {
    return VMS_INTERNAL;
  }

  /* This condition should not happen with VM data storage */
  if (sizeof(vms_data_header_t) > free)
  {
    return VMS_INTERNAL;
  }

  /* Compute the size used by the slot */
  oldused = (uint32_t)(vm.slots[slot]->fields.next->hdr8) - (uint32_t)(vm.slots[slot]->hdr8);

  /* Remove the slot and compress the VM data storage */
  err = remove_slot(slot);
  if (err != VMS_NOERROR)
  {
    return err;
  }

  /* Adjusting the counter of the effective used size */
  vm.slots[slot]  = NULL;
  vm.used_size -= oldused;

  return warning ? VMS_WARNING : VMS_NOERROR;
}

/**
  * @brief   Retrieves data for a slot.
  * @note    The returned pointer is valid only until to the next call to
  *          the subsystem because slots can be updated and moved in the ram
  *          array.
  *
  * @param[in] slot           slot identifier
  * @param[out] size_p        pointer to a variable that will contain the slot
  *                           size after the call or @p NULL
  * @param[out] type_p        pointer to a variable that will contain the data
  *                           type after the call or @p NULL
  * @param[out] data_pp       pointer to a pointer to the data or @p NULL
  *
  * @return                   The operation status.
  * @retval VMS_NOERROR        if the operation has been successfully completed.
  * @retval VMS_NOTINIT        if the subsystem has not been initialized yet.
  * @retval VMS_SLOT_INVALID   if the slot number is out of range.
  * @retval VMS_DATA_NOT_FOUND if the data does not exists.
  */
vms_error_t VMS_GetDataWithType(vms_slot_t slot, size_t *size_p, vms_data_type_t *type_p,
                                uint8_t **data_pp)
{
  /* Check on initialization */
  if (!is_initialized())
  {
    return VMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot >= VMS_CFG_NUM_SLOTS)
  {
    return VMS_SLOT_INVALID;
  }

  /* Check slot presence */
  if (vm.slots[slot] == NULL)
  {
    return VMS_DATA_NOT_FOUND;
  }

  if (size_p != NULL)
  {
    *size_p = vm.slots[slot]->fields.data_size;
  }

  if (type_p != NULL)
  {
    *type_p = vm.slots[slot]->fields.data_type;
  }

  if (data_pp != NULL)
  {
    *data_pp = vm.slots[slot]->hdr8 + sizeof(vms_data_header_t);
  }

  return VMS_NOERROR;
}

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_VM_DYNAMIC_ENABLED */
#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
