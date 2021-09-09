/**
  ******************************************************************************
  * @file    kms_nvm_storage.c
  * @author  MCD Application Team
  * @brief   This file contains implementations for Key Management Services (KMS)
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

/* Includes ------------------------------------------------------------------*/
#include "kms.h"
#if defined(KMS_ENABLED)
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kms_nvm_storage.h"

#if defined(KMS_NVM_ENABLED)
/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_NVM NVM storage
  * @{
  */

/* Private types -------------------------------------------------------------*/
/**
  * @brief   Type for a slot scan callback function.
  */
typedef void (*nvms_found_slot_t)(nvms_data_header_t *hdr_p);

/**
  * @brief   Type for a slots scan callback function.
  */
typedef void (*nvms_end_slot_t)(nvms_data_header_t *hdr_p);
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/**
  * @brief   Error check helper.
  */
#define CHECK_ERROR(err) do {                                               \
                              nvms_error_t e = (err);                       \
                              if (e != NVMS_NOERROR) {                      \
                                return e;                                   \
                              }                                             \
                            } while (false)

/* Private variables ---------------------------------------------------------*/

/**
  * @brief   NVM Storage internal state.
  * @details This variable is public for testability.
  */
static struct nvms_state nvm;

#if defined(KMS_DEBUG_MODE)
uint32_t  latest_warning;
#endif /* KMS_DEBUG_MODE */

/* Private function prototypes -----------------------------------------------*/
/* Private function ----------------------------------------------------------*/
/**
  * @brief   Erases a block.
  *
  * @param[in] block         the block identifier
  * @return                  The error code.
  * @retval NVMS_NOERROR       if the operation is successful.
  * @retval NVMS_FLASH_FAILURE if the operation failed while writing.
  */
static nvms_error_t block_erase(nvms_block_t block)
{
  bool result;

  result = NVMS_LL_BlockErase(block);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }
  return NVMS_NOERROR;
}

/**
  * @brief   Invalidates the current state.
  */
static void reset(void)
{
  uint32_t i;

  nvm.header = NULL;
  nvm.block = NVMS_BLOCK0;
  for (i = 0; i < NVMS_CFG_NUM_SLOTS; i++)
  {
    nvm.slots[i] = NULL;
  }
  nvm.free_next = NULL;
  nvm.used_size = 0;
}

/**
  * @brief   Returns the initialization status.
  *
  * @return                  The initialization status.
  * @retval false            if the subsystem is not initialized.
  * @retval true             if the subsystem is initialized.
  */
static inline bool is_initialized(void)
{
  return nvm.header != NULL;
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
  * @param[in] block         the block identifier
  * @param[in] hdrp          pointer to the data header
  *
  * @return                        The slot state.
  * @retval NVMS_SLOT_STATUS_ERASED if the header is erased.
  * @retval NVMS_SLOT_STATUS_OK      if the header is valid and the slot matches the
  *                                CRC.
  * @retval NVMS_SLOT_STATUS_CRC     if the slot does not match the CRC.
  * @retval NVMS_SLOT_STATUS_BROKEN if the header is corrupt.
  */
static nvms_slot_status_t check_slot_instance(nvms_block_t block,
                                              nvms_data_header_t *hdrp)
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
    if (hdrp->hdr32[i] != NVMS_LL_ERASED)
    {
      /* Check on the pointer to the next block, it must be aligned to an
         header boundary */
      if (((uint32_t)hdrp->fields.next & (NVMS_LL_PAGE_SIZE - 1UL)) != 0UL)
      {
        return NVMS_SLOT_STATUS_BROKEN;
      }

      /* Checks on the pointer to the next block, the address must be comprised
         between the next header position and the end of the flash array */
      startp = (uint8_t *)NVMS_LL_GetBlockAddress(block);
      endp = &startp[NVMS_LL_GetBlockSize()];
      if ((hdrp->fields.next->hdr8 < (hdrp->hdr8 + sizeof(nvms_data_header_t))) ||
          (hdrp->fields.next->hdr8 > endp))
      {
        return NVMS_SLOT_STATUS_BROKEN;
      }

      /* Check on the magic numbers */
      if ((hdrp->fields.magic1 != NVMS_HEADER_MAGIC1) ||
          (hdrp->fields.magic2 != NVMS_HEADER_MAGIC2))
      {
        return NVMS_SLOT_STATUS_BROKEN;
      }

      /* Check on slot identifier */
      if (hdrp->fields.slot >= NVMS_CFG_NUM_SLOTS)
      {
        return NVMS_SLOT_STATUS_BROKEN;
      }

      /* Check on the instance field */
      if (hdrp->fields.instance == NVMS_LL_ERASED)
      {
        return NVMS_SLOT_STATUS_BROKEN;
      }

      /* Checks on the data size */
      if ((hdrp->hdr8 + sizeof(nvms_data_header_t) + hdrp->fields.data_size) >
          hdrp->fields.next->hdr8)
      {
        return NVMS_SLOT_STATUS_BROKEN;
      }

      /* Payload checksum */
      checksum = do_checksum(hdrp->hdr8 + sizeof(nvms_data_header_t),
                             hdrp->fields.data_size);
      if (checksum != hdrp->fields.data_checksum)
      {
        return NVMS_SLOT_STATUS_CRC;
      }

      return NVMS_SLOT_STATUS_OK;
    }
  }

  return NVMS_SLOT_STATUS_ERASED;
}

/**
  * @brief   Scans a block searching for slots.
  * @note    The block integrity is strongly checked.
  *
  * @param[in] block         the block identifier
  * @param[in] slotcallback   callback to be called for each found slot
  * @param[in] endcallback   callback to be called after scanning.
  *
  * @return                  The block state.
  * @retval NVMS_STATUS_NORMAL    if the block contains valid data.
  * @retval NVMS_STATUS_PARTIAL   if the block contains errors but the data is still
  *                          readable.
  * @retval NVMS_STATUS_BROKEN    if the block contains unreadable garbage. The
  *                          @p endcallback function is not called in this
  *                          case.
  */
static nvms_block_status_t scan_slots(nvms_block_t block,
                                      nvms_found_slot_t slotcallback,
                                      nvms_end_slot_t endcallback)
{
  nvms_data_header_t *hdrp;
  uint8_t *startp;
  const uint8_t *endp;
  nvms_slot_status_t slotsts;
  bool warning = false;
  nvms_block_status_t status = NVMS_STATUS_BROKEN;
  bool status_found = false;

  /* Limits */
  startp = (uint8_t *)NVMS_LL_GetBlockAddress(block);
  endp = &startp[NVMS_LL_GetBlockSize()];

  /* Checking the main header */
  hdrp = (nvms_data_header_t *)(uint32_t)startp;
  slotsts = check_slot_instance(block, hdrp);
  if (slotsts != NVMS_SLOT_STATUS_OK)
  {
    return NVMS_STATUS_BROKEN;
  }

  /* Scanning the slots chain */
  while (!status_found)
  {

    /* Point to next slot header */
    hdrp = (nvms_data_header_t *)hdrp->fields.next;

    /* Special case end-of-chain */
    if (hdrp->hdr8 == endp)
    {
      /* Calling end-of-scan callback if defined */
      endcallback(hdrp);
      if (warning)
      {
        return NVMS_STATUS_PARTIAL;
      }
      else
      {
        return NVMS_STATUS_NORMAL;
      }
    }

    /* Header check */
    slotsts = check_slot_instance(block, hdrp);
    switch (slotsts)
    {
      case NVMS_SLOT_STATUS_ERASED:
        /* Calling end-of-scan callback if defined */
        endcallback(hdrp);

        /* An erased header mark the end of the chain */
        if (warning)
        {
          status = NVMS_STATUS_PARTIAL;
          status_found = true;
        }
        else
        {
          status = NVMS_STATUS_NORMAL;
          status_found = true;
        }
        break;

      case NVMS_SLOT_STATUS_OK:
        /* Normal header, continue scan */
        slotcallback(hdrp);
        break;

      case NVMS_SLOT_STATUS_CRC:
        /* Key data corrupted */
        warning = true;
#if defined(KMS_DEBUG_MODE)
        latest_warning = NVMS_SLOT_STATUS_CRC;
#endif /* KMS_DEBUG_MODE */
        break;

      case NVMS_SLOT_STATUS_BROKEN:
        /* Calling end-of-scan callback if defined */
        endcallback(hdrp);

        /* Problems, stopping the scan here */
        status = NVMS_STATUS_PARTIAL;
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
  * @brief   Retrieves the latest instance of a slot in a block.
  * @pre     The source block is at least partially readable. The
  *          main header must be correct.
  *
  * @param[in] block         the block identifier
  * @param[in] slot           slot identifier
  * @param[out] hdrpp        pointer to a header pointer
  *
  * @return                    The error code.
  * @retval NVMS_NOERROR        if the operation is successful.
  * @retval NVMS_CRC            if the slot is corrupted.
  * @retval NVMS_DATA_NOT_FOUND if the slot does not exists in the block.
  */
static nvms_error_t find_slot(nvms_block_t block,
                              uint32_t slot,
                              nvms_data_header_t **hdrpp)
{
  nvms_data_header_t *hdrp;
  nvms_data_header_t *slotp;
  uint8_t *startp;
  const uint8_t *endp;
  nvms_slot_status_t slotsts;
  bool crcerr;
  bool status_found = false;
  nvms_error_t status = NVMS_INTERNAL;

  /* Limits */
  startp = (uint8_t *)NVMS_LL_GetBlockAddress(block);
  endp = &startp[NVMS_LL_GetBlockSize()];

  /* Scanning the slots chain */
  hdrp = (nvms_data_header_t *)(uint32_t)startp;
  slotp = NULL;
  crcerr = false;
  while (!status_found)
  {
    /* Point to next slot header */
    hdrp = (nvms_data_header_t *)hdrp->fields.next;

    /* Special case end-of-chain */
    if (hdrp->hdr8 == endp)
    {
      if (slotp == NULL)
      {
        return NVMS_DATA_NOT_FOUND;
      }
      *hdrpp = slotp;
      if (crcerr)
      {
        return NVMS_CRC;
      }
      else
      {
        return NVMS_NOERROR;
      }
    }

    /* Header check */
    slotsts = check_slot_instance(block, hdrp);
    switch (slotsts)
    {
      case NVMS_SLOT_STATUS_ERASED:
      case NVMS_SLOT_STATUS_BROKEN:
        /* An erased header or a broken header mark the end of the chain */
        if (slotp == NULL)
        {
          status = NVMS_DATA_NOT_FOUND;
        }
        else
        {
          *hdrpp = slotp;
          if (crcerr)
          {
            status = NVMS_CRC;
          }
          else
          {
            status = NVMS_NOERROR;
          }
        }
        status_found = true;
        break;

      case NVMS_SLOT_STATUS_OK:
        /* Normal header and valid data, checking if it is a newer version
           of the data we are looking for */
        if (hdrp->fields.slot == slot)
        {
          slotp = hdrp;
          crcerr = false;
        }
        break;

      case NVMS_SLOT_STATUS_CRC:
        /* Corrupt data but the header is fine */
        if (hdrp->fields.slot == slot)
        {
          slotp = hdrp;
          crcerr = true;
        }
        break;

      default:
        /* No action */
        break;
    }
  }
  return status;
}

/**
  * @brief   Copies slot to a new position.
  * @note    The slot size is supposed to be greater than zero because,
  *          zero sized slots must not be copied across banks.
  *
  * @param[out] rhdrp        pointer to the source header
  * @param[out] whdrp        pointer to the destination header
  *
  * @return                    The error code.
  * @retval NVMS_NOERROR       if the operation is successful.
  * @retval NVMS_FLASH_FAILURE if the operation failed while writing.
  */
static nvms_error_t copy_slot(const nvms_data_header_t *rhdrp,
                              nvms_data_header_t *whdrp)
{
  bool result;
  nvms_data_header_t hdr;
  size_t size;
  uint8_t *p_next;
  uint8_t *p_free;

  size = rhdrp->fields.data_size;
  p_next = whdrp->hdr8;
  p_free = &p_next[(((sizeof(nvms_data_header_t) + size - 1UL) | (NVMS_LL_PAGE_SIZE - 1UL)) + 1UL)];

  /* Writing the header without the magic numbers, this way it is
     not yet validated but the write is tested */

  hdr.fields.magic1             = NVMS_LL_ERASED;
  hdr.fields.magic2             = NVMS_LL_ERASED;
  hdr.fields.slot                = rhdrp->fields.slot;
  hdr.fields.instance           = 1;    /* Resetting instance number */
  hdr.fields.next               = (nvms_data_header_t *)(uint32_t)p_free;
  hdr.fields.data_type       = rhdrp->fields.data_type;
  hdr.fields.data_size       = size;
  hdr.fields.data_checksum   = rhdrp->fields.data_checksum;

  /* Do not write the full structure (including the 2 initial words set to the ERASED FLASH value), but only the
  meaningful data
  Rationale : writing the erased_default value on some platform prevent any further update of the flash
  */
  result = NVMS_LL_Write(&((const uint8_t *)&hdr)[8], &p_next[8], sizeof(nvms_data_header_t) - 8UL);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }

  /* Writing data, the slot size is supposed to be greater than zero
     because, zero sized slots must not be copied across bank */
  result = NVMS_LL_Write(rhdrp->hdr8 + sizeof(nvms_data_header_t),
                         &p_next[sizeof(nvms_data_header_t)],
                         size);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }

  /* Writing the magic numbers validates the header */
  hdr.fields.magic1 = NVMS_HEADER_MAGIC1;
  hdr.fields.magic2 = NVMS_HEADER_MAGIC2;
  result = NVMS_LL_Write((const uint8_t *)&hdr, p_next, sizeof(uint32_t) * 2UL);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }
  return NVMS_NOERROR;
}

/**
  * @brief   Copies valid slots from one block to another.
  * @pre     The source block is at least partially readable. The
  *          main header must be correct.
  * @pre     The destination block is erased.
  * @post    The destination block contains the latest instance of all
  *          the readable slots in the source block, the instance counter
  *          is increased by one for each slot.
  *
  * @param[in] source_block  the source block identifier
  * @param[in] dest_block    the destination block identifier
  *
  * @return                  The error code.
  * @retval NVMS_NOERROR       if the operation is successful.
  * @retval NVMS_FLASH_FAILURE if the operation failed while writing.
  */
static nvms_error_t copy_slots(nvms_block_t source_block,
                               nvms_block_t dest_block)
{
  uint32_t slot;
  nvms_data_header_t *whdrp;

  whdrp = (nvms_data_header_t *)NVMS_LL_GetBlockAddress(dest_block) + 1;
  for (slot = 0; slot < NVMS_CFG_NUM_SLOTS; slot++)
  {
    nvms_error_t err;
    nvms_data_header_t *rhdrp;

    err = find_slot(source_block, slot, &rhdrp);
    if ((err == NVMS_NOERROR) && (rhdrp->fields.data_size > 0UL))
    {
      err = copy_slot(rhdrp, whdrp);
      if (err != NVMS_NOERROR)
      {
        return err;
      }
      whdrp = (nvms_data_header_t *)whdrp->fields.next;
    }
  }

  return NVMS_NOERROR;
}

/**
  * @brief   Writes the main header validating a block.
  * @pre     The header area must be in erased state.
  *
  * @param[in] block         the block identifier
  * @param[in] instance      the instance number to write into the header
  *
  * @return                  The error code.
  * @retval NVMS_NOERROR       if the operation is successful.
  * @retval NVMS_FLASH_FAILURE if the operation failed while writing.
  */
static nvms_error_t validate(nvms_block_t block, uint32_t instance)
{
  bool result;
  uint8_t *dp = (uint8_t *)NVMS_LL_GetBlockAddress(block);
  nvms_data_header_t hdr;

  /* Writing the header without the magic numbers, this way it is
     not yet validated but the write is tested.
     Note, the header is given a slot id zero, this does not impact
     real slots */
  hdr.fields.magic1             = NVMS_LL_ERASED;
  hdr.fields.magic2             = NVMS_LL_ERASED;
  hdr.fields.slot               = NVMS_SLOT_MAIN_HEADER;
  hdr.fields.instance           = instance;
  hdr.fields.next               = &((nvms_data_header_t *)(uint32_t)dp)[1];
  hdr.fields.data_type       = NVMS_LL_ERASED;
  hdr.fields.data_size       = 0;
  hdr.fields.data_checksum   = 0;

  /* Do not write the full structure (including the 2 initial words set to the ERASED FLASH value), but only the
  meaningful data
  Rationale : writing the erased_default value on some platform prevent any further update of the flash
  */
  result = NVMS_LL_Write(&hdr.hdr8[8], &dp[8], sizeof(nvms_data_header_t) - 8UL);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }

  /* Writing the magic numbers validates the header */
  hdr.fields.magic1 = NVMS_HEADER_MAGIC1;
  hdr.fields.magic2 = NVMS_HEADER_MAGIC2;
  result = NVMS_LL_Write(hdr.hdr8, dp, sizeof(uint32_t) * 2UL);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }

  return NVMS_NOERROR;
}

/**
  * @brief   Null callback.
  */
static void null_callback(nvms_data_header_t *hdrp)
{

  (void)(hdrp);
}

/**
  * @brief   Private callback of @p use().
  */
static void use_slot_callback(nvms_data_header_t *hdrp)
{

  nvm.slots[hdrp->fields.slot] = hdrp;
}

/**
  * @brief   Private callback of @p use().
  */
static void use_end_callback(nvms_data_header_t *hdrp)
{

  nvm.free_next = hdrp;
}

/**
  * @brief   Put a block in use as current block.
  * @note    If a block is being put in use then it is assumed to contain
  *          no errors of any kind.
  *
  * @param[in] block           the block identifier
  *
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_INTERNAL      if an internal error occurred.
  */
static nvms_error_t use(nvms_block_t block)
{
  uint32_t i;
  nvms_block_status_t status;
  nvms_data_header_t *hdrp = (nvms_data_header_t *)NVMS_LL_GetBlockAddress(block);

  /* Resetting state */
  reset();

  /* Global info */
  nvm.header    = hdrp;
  nvm.block     = block;
  nvm.used_size = sizeof(nvms_data_header_t);
  nvm.free_next = &hdrp[1];

  /* The block should have been checked before calling use() so any
     kind of anomaly in the block is considered an internal error */
  status = scan_slots(block, use_slot_callback, use_end_callback);
  if (status != NVMS_STATUS_NORMAL)
  {
    reset();
    return NVMS_INTERNAL;
  }

  /* Scanning found slots */
  for (i = 0; i < NVMS_CFG_NUM_SLOTS; i++)
  {
    hdrp = nvm.slots[i];

    /* If the pointer is not NULL then there is a slot in this position */
    if (hdrp != NULL)
    {
      /* Zero sized slots are discarded because indicate that the slot
         has been erased */
      if (hdrp->fields.data_size == 0UL)
      {
        nvm.slots[hdrp->fields.slot] = NULL;
        continue;
      }

      /* Adding the slot used space to the total */
      nvm.used_size += hdrp->fields.next->hdr8 - hdrp->hdr8;
    }
  }

  return NVMS_NOERROR;
}

/**
  * @brief   Determines the state of a flash block.
  *
  * @param[in] block         the block identifier
  * @param[out] instance     block instance number, only valid if the block
  *                          is not in the @p NVMS_STATUS_BROKEN or
  *                          @p NVMS_STATUS_ERASED states.
  *
  * @return                       The block state.
  * @retval NVMS_STATUS_ERASED    if the block is fully erased.
  * @retval NVMS_STATUS_NORMAL    if the block contains valid data.
  * @retval NVMS_STATUS_PARTIAL   if the block contains errors but the data is still
  *                               readable.
  * @retval NVMS_STATUS_BROKEN    if the block contains unreadable garbage.
  */
static nvms_block_status_t determine_block_state(nvms_block_t block,
                                                 uint32_t *instance)
{
  /* Special case where the block is fully erased */
  if (NVMS_LL_IsBlockErased(block))
  {
    return NVMS_STATUS_ERASED;
  }

  /* Returning the block instance number */
  *instance = ((nvms_data_header_t *)NVMS_LL_GetBlockAddress(block))->fields.instance;

  /* Checking block integrity by just scanning it */
  return scan_slots(block, null_callback, null_callback);
}

/**
  * @brief   Appends a slot instance to the block in use.
  *
  * @param[in] slot           slot identifier
  * @param[in] size           slot size
  * @param[in] type           slot type
  * @param[in] slotp          pointer to the slot
  * @param[in] instance       instance number to attribute to the data
  *
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval NVMS_SLOT_INVALID  if the slot identifier is out of range.
  * @retval NVMS_FLASH_FAILURE if the flash memory is unusable because HW
  *                            failures.
  */
static nvms_error_t append_slot(nvms_slot_t slot, size_t size, nvms_data_type_t type,
                                const uint8_t *slotp, uint32_t instance)
{
  bool result;
  nvms_data_header_t hdr;
  uint8_t *p_next;
  uint8_t *p_free;

  p_next = nvm.free_next->hdr8;
  p_free = &p_next[(((sizeof(nvms_data_header_t) + size - 1UL) | (NVMS_LL_PAGE_SIZE - 1UL)) + 1UL)];

  /* Writing the header without the magic numbers, this way it is
     not yet validated but the write is tested */
  hdr.fields.magic1             = NVMS_LL_ERASED;
  hdr.fields.magic2             = NVMS_LL_ERASED;
  hdr.fields.slot               = slot;
  hdr.fields.instance           = instance;
  hdr.fields.next               = (nvms_data_header_t *)(uint32_t)p_free;
  hdr.fields.data_type          = type;
  hdr.fields.data_size          = size;
  hdr.fields.data_checksum      = do_checksum(slotp, size);

  /* Do not write the full structure (including the 2 initial words set to the ERASED FLASH value), but only the
  meaningful data
  Rationale : writing the erased_default value on some platform prevent any further update of the flash
  */
  result = NVMS_LL_Write(&((const uint8_t *)&hdr)[8], &p_next[8], sizeof(nvms_data_header_t) - 8UL);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }

  /* Writing data */
  if (size > 0UL)
  {
    result = NVMS_LL_Write(slotp, &p_next[sizeof(nvms_data_header_t)], size);
    if (result)
    {
      return NVMS_FLASH_FAILURE;
    }
  }

  /* Writing the magic numbers validates the header */
  hdr.fields.magic1 = NVMS_HEADER_MAGIC1;
  hdr.fields.magic2 = NVMS_HEADER_MAGIC2;
  result = NVMS_LL_Write((const uint8_t *)&hdr,
                         nvm.free_next->hdr8,
                         sizeof(uint32_t) * 2UL);
  if (result)
  {
    return NVMS_FLASH_FAILURE;
  }
  /* Updating the global pointer */
  nvm.free_next = (nvms_data_header_t *)(uint32_t)p_free;

  return NVMS_NOERROR;
}

/**
  * @brief   Enforces a garbage collection.
  * @details Storage data is compacted into a single bank.
  */
static nvms_error_t garbage_collect(void)
{

  if (nvm.block == NVMS_BLOCK0)
  {
    CHECK_ERROR(copy_slots(NVMS_BLOCK0, NVMS_BLOCK1));
    CHECK_ERROR(validate(NVMS_BLOCK1, nvm.header->fields.instance + 1UL));
    CHECK_ERROR(block_erase(NVMS_BLOCK0));
    CHECK_ERROR(use(NVMS_BLOCK1));
  }
  else
  {
    CHECK_ERROR(copy_slots(NVMS_BLOCK1, NVMS_BLOCK0));
    CHECK_ERROR(validate(NVMS_BLOCK0, nvm.header->fields.instance + 1UL));
    CHECK_ERROR(block_erase(NVMS_BLOCK1));
    CHECK_ERROR(use(NVMS_BLOCK0));
  }

  return NVMS_NOERROR;
}

/**
  * @brief   Performs a flash initialization attempt.
  *
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_WARNING       if the initialization is complete but some repair
  *                            work has been performed.
  * @retval NVMS_FLASH_FAILURE if the flash memory is unusable because HW
  *                            failures.
  * @retval NVMS_INTERNAL      if an internal error occurred.
  */
static nvms_error_t try_boot(void)
{
  nvms_block_status_t sts0;
  nvms_block_status_t sts1;
  uint32_t seq0 = 0;
  uint32_t seq1 = 0;

  sts0 = determine_block_state(NVMS_BLOCK0, &seq0);
  sts1 = determine_block_state(NVMS_BLOCK1, &seq1);

  /* Case 1 - Both block erased, performs an initialization of block zero
     and starts using it */
  if ((sts0 == NVMS_STATUS_ERASED) && (sts1 == NVMS_STATUS_ERASED))
  {
    CHECK_ERROR(validate(NVMS_BLOCK0, 1));
    CHECK_ERROR(use(NVMS_BLOCK0));

    return NVMS_NOERROR;
  }

  /* Cases 2, 3, 4 - Block zero is erased */
  if (sts0 == NVMS_STATUS_ERASED)
  {

    /* Case 2 - Block zero is erased, block one is normal */
    if (sts1 == NVMS_STATUS_NORMAL)
    {
      CHECK_ERROR(use(NVMS_BLOCK1));

      return NVMS_NOERROR;
    }

    /* Case 3 - Block zero is erased, block one is partially corrupted */
    if (sts1 == NVMS_STATUS_PARTIAL)
    {
      CHECK_ERROR(copy_slots(NVMS_BLOCK1, NVMS_BLOCK0));
      CHECK_ERROR(validate(NVMS_BLOCK0, seq1 + 1UL));
      CHECK_ERROR(block_erase(NVMS_BLOCK1));
      CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_ONE_BLOCK_CORRUPTED;
#endif /* KMS_DEBUG_MODE */
      return NVMS_WARNING;
    }

    /* Case 4 - Block zero is erased, block one is broken. Note, the
       variable "sts1" is assumed to have value "NVMS_STATUS_BROKEN" */
    CHECK_ERROR(block_erase(NVMS_BLOCK1));
    CHECK_ERROR(validate(NVMS_BLOCK0, 1));
    CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
    latest_warning = NVMS_WARNING_ONE_BLOCK_BROKEN;
#endif /* KMS_DEBUG_MODE */
    return NVMS_WARNING;
  }

  /* Cases 5, 6, 7 - Block one is erased */
  if (sts1 == NVMS_STATUS_ERASED)
  {
    /* Case 5 - Block one is erased, block zero is normal */
    if (sts0 == NVMS_STATUS_NORMAL)
    {
      CHECK_ERROR(use(NVMS_BLOCK0));

      return NVMS_NOERROR;
    }

    /* Case 6 - Block one is erased, block zero is partially corrupted */
    if (sts0 == NVMS_STATUS_PARTIAL)
    {
      CHECK_ERROR(copy_slots(NVMS_BLOCK0, NVMS_BLOCK1));
      CHECK_ERROR(validate(NVMS_BLOCK1, seq0 + 1UL));
      CHECK_ERROR(block_erase(NVMS_BLOCK0));
      CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_ONE_BLOCK_CORRUPTED;
#endif /* KMS_DEBUG_MODE */
      return NVMS_WARNING;
    }

    /* Case 7 - Block one is erased, block zero is broken */
    if (sts0 == NVMS_STATUS_BROKEN)
    {
      CHECK_ERROR(block_erase(NVMS_BLOCK0));
      CHECK_ERROR(validate(NVMS_BLOCK1, 1));
      CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_ONE_BLOCK_BROKEN;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
  }

  /* Case 8 - Both block appear to be correct, we must use the most
     recent one and erase the other */
  if ((sts0 == NVMS_STATUS_NORMAL) && (sts1 == NVMS_STATUS_NORMAL))
  {
    if (seq0 > seq1)
    {
      /* Block 0 is newer */
      CHECK_ERROR(block_erase(NVMS_BLOCK1));
      CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_TWO_BLOCKS_NORMAL;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
    else
    {
      /* Block 1 is newer */
      CHECK_ERROR(block_erase(NVMS_BLOCK0));
      CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_TWO_BLOCKS_NORMAL;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
  }

  /* Case 9 - Both block appear to be partially corrupted, we must use the
     most recent one and erase the other */
  if ((sts0 == NVMS_STATUS_PARTIAL) && (sts1 == NVMS_STATUS_PARTIAL))
  {
    if (seq0 > seq1)
    {
      /* Block 0 is newer, copying in block1 and using it */
      CHECK_ERROR(block_erase(NVMS_BLOCK1));
      CHECK_ERROR(copy_slots(NVMS_BLOCK0, NVMS_BLOCK1));
      CHECK_ERROR(validate(NVMS_BLOCK1, seq0 + 1UL));
      CHECK_ERROR(block_erase(NVMS_BLOCK0));
      CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_TWO_BLOCKS_NORMAL;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
    else
    {
      /* Block 1 is newer, copying in block0 and using it */
      CHECK_ERROR(block_erase(NVMS_BLOCK0));
      CHECK_ERROR(copy_slots(NVMS_BLOCK1, NVMS_BLOCK0));
      CHECK_ERROR(validate(NVMS_BLOCK0, seq1 + 1UL));
      CHECK_ERROR(block_erase(NVMS_BLOCK1));
      CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_TWO_BLOCKS_NORMAL;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
  }

  /* Cases 10, 11 - Block zero is normal */
  if (sts0 == NVMS_STATUS_NORMAL)
  {
    /* Case 10 - Block zero is normal, block one is partial */
    if (sts1 == NVMS_STATUS_PARTIAL)
    {
      if (seq0 > seq1)
      {
        /* Normal block (0) is more recent than the partial block (1), the
           partial block needs to be erased */
        CHECK_ERROR(block_erase(NVMS_BLOCK1));
        CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
        latest_warning = NVMS_WARNING_ONE_BLOCK_PARTIAL;
#endif /* KMS_DEBUG_MODE */

        return NVMS_WARNING;
      }
      else
      {
        /* Partial block (1) is more recent than the normal block (0) */
        CHECK_ERROR(block_erase(NVMS_BLOCK0));
        CHECK_ERROR(copy_slots(NVMS_BLOCK1, NVMS_BLOCK0));
        CHECK_ERROR(validate(NVMS_BLOCK0, seq1 + 1UL));
        CHECK_ERROR(block_erase(NVMS_BLOCK1));
        CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
        latest_warning = NVMS_WARNING_ONE_BLOCK_PARTIAL;
#endif /* KMS_DEBUG_MODE */

        return NVMS_WARNING;
      }
    }

    /* Case 11 - Block zero is normal, block one is broken */
    if (sts1 == NVMS_STATUS_BROKEN)
    {
      CHECK_ERROR(block_erase(NVMS_BLOCK1));
      CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_ONE_BLOCK_BROKEN;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
  }

  /* Cases 12, 13 - Block one is normal */
  if (sts1 == NVMS_STATUS_NORMAL)
  {
    /* Case 12 - Block one is normal, block zero is partial */
    if (sts0 == NVMS_STATUS_PARTIAL)
    {
      if (seq1 > seq0)
      {
        /* Normal block (1) is more recent than the partial block (0), the
           partial block needs to be erased */
        CHECK_ERROR(block_erase(NVMS_BLOCK0));
        CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
        latest_warning = NVMS_WARNING_ONE_BLOCK_PARTIAL;
#endif /* KMS_DEBUG_MODE */

        return NVMS_WARNING;
      }
      else
      {
        /* Partial block (0) is more recent than the normal block (1) */
        CHECK_ERROR(block_erase(NVMS_BLOCK1));
        CHECK_ERROR(copy_slots(NVMS_BLOCK0, NVMS_BLOCK1));
        CHECK_ERROR(validate(NVMS_BLOCK1, seq0 + 1UL));
        CHECK_ERROR(block_erase(NVMS_BLOCK0));
        CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
        latest_warning = NVMS_WARNING_ONE_BLOCK_PARTIAL;
#endif /* KMS_DEBUG_MODE */

        return NVMS_WARNING;
      }
    }

    /* Case 13 - Block one is normal, block zero is broken */
    if (sts0 == NVMS_STATUS_BROKEN)
    {
      CHECK_ERROR(block_erase(NVMS_BLOCK0));
      CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
      latest_warning = NVMS_WARNING_ONE_BLOCK_BROKEN;
#endif /* KMS_DEBUG_MODE */

      return NVMS_WARNING;
    }
  }

  /* Case 14 - Block zero is partial, block one is broken */
  if ((sts0 == NVMS_STATUS_PARTIAL) && (sts1 == NVMS_STATUS_BROKEN))
  {
    CHECK_ERROR(block_erase(NVMS_BLOCK1));
    CHECK_ERROR(copy_slots(NVMS_BLOCK0, NVMS_BLOCK1));
    CHECK_ERROR(validate(NVMS_BLOCK1, seq0 + 1UL));
    CHECK_ERROR(block_erase(NVMS_BLOCK0));
    CHECK_ERROR(use(NVMS_BLOCK1));

#if defined(KMS_DEBUG_MODE)
    latest_warning = NVMS_WARNING_ONE_BLOCK_PARTIAL_AND_ONE_BROKEN;
#endif /* KMS_DEBUG_MODE */

    return NVMS_WARNING;
  }

  /* Case 15 - Block zero is broken, block one is partial */
  if ((sts0 == NVMS_STATUS_BROKEN) && (sts1 == NVMS_STATUS_PARTIAL))
  {

    CHECK_ERROR(block_erase(NVMS_BLOCK0));
    CHECK_ERROR(copy_slots(NVMS_BLOCK1, NVMS_BLOCK0));
    CHECK_ERROR(validate(NVMS_BLOCK0, seq0 + 1UL));
    CHECK_ERROR(block_erase(NVMS_BLOCK1));
    CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
    latest_warning = NVMS_WARNING_ONE_BLOCK_PARTIAL_AND_ONE_BROKEN;
#endif /* KMS_DEBUG_MODE */

    return NVMS_WARNING;
  }

  /* Case 16 - Both bank broken */
  CHECK_ERROR(block_erase(NVMS_BLOCK0));
  CHECK_ERROR(block_erase(NVMS_BLOCK1));
  CHECK_ERROR(validate(NVMS_BLOCK0, 1));
  CHECK_ERROR(use(NVMS_BLOCK0));

#if defined(KMS_DEBUG_MODE)
  latest_warning = NVMS_WARNING_TWO_BLOCKS_BROKEN;
#endif /* KMS_DEBUG_MODE */

  return NVMS_WARNING;
}

/* Exported variables --------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
  * @brief   Subsystem initialization.
  *
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_WARNING       if the initialization is complete but some repair
  *                            work has been performed.
  * @retval NVMS_FLASH_FAILURE if the flash memory is unusable because HW
  *                            failures.
  * @retval NVMS_INTERNAL      if an internal error occurred.
  */
nvms_error_t NVMS_Init(void)
{
  NVMS_LL_Init();
  reset();

  for (uint32_t i = NVMS_CFG_MAX_REPAIR_ATTEMPTS; i > 0UL; i--)
  {
    nvms_error_t err = try_boot();
    if ((err == NVMS_NOERROR) || (err == NVMS_WARNING))
    {
      return err;
    }
  }
  return NVMS_FLASH_FAILURE;
}

/**
  * @brief   Subsystem de-initialization.
  * @details The function cannot fail and does nothing if the system has not
  *          been yet initialized.
  */
void NVMS_Deinit(void)
{
  /* Clearing data structures */
  reset();
}

/**
  * @brief   Destroys the state of the data storage by erasing the flash.
  *
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval NVMS_FLASH_FAILURE if the flash memory is unusable because HW
  *                            failures.
  * @retval NVMS_INTERNAL      if an internal error occurred.
  */
nvms_error_t NVMS_Erase(void)
{
  /* Check on initialization */
  if (!is_initialized())
  {
    return NVMS_NOTINIT;
  }

  /* Erasing both block */
  CHECK_ERROR(block_erase(NVMS_BLOCK0));
  CHECK_ERROR(block_erase(NVMS_BLOCK1));

  /* Re-initializing the flash as an empty slot storage */
  CHECK_ERROR(validate(NVMS_BLOCK0, 1));
  CHECK_ERROR(use(NVMS_BLOCK0));

  return NVMS_NOERROR;
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
  * @return                    The operation status.
  * @retval NVMS_NOERROR       if the operation has been successfully completed.
  * @retval NVMS_WARNING       if some repair work has been performed or
  *                            garbage collection happened.
  * @retval NVMS_NOTINIT       if the subsystem has not been initialized yet.
  * @retval NVMS_SLOT_INVALID  if the slot identifier is out of range.
  * @retval NVMS_FLASH_FAILURE if the flash memory is unusable because HW
  *                            failures.
  * @retval NVMS_OUT_OF_MEM    if the slot space is exhausted.
  * @retval NVMS_INTERNAL      if an internal error occurred.
  */
nvms_error_t NVMS_WriteDataWithType(nvms_slot_t slot, size_t size, nvms_data_type_t type,
                                    const uint8_t *slotp)
{
  bool warning = false;
  uint32_t instance;
  size_t free;
  size_t oldused;
  nvms_error_t err;
  nvms_data_header_t *hdrp;

  /* Check on initialization */
  if (!is_initialized())
  {
    return NVMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot >= NVMS_CFG_NUM_SLOTS)
  {
    return NVMS_SLOT_INVALID;
  }

  /* Checking for immediately available space */
  free = NVMS_LL_GetBlockSize() - (nvm.free_next->hdr8 -
                                   nvm.header->hdr8);

  /* If the requested space is out of the compacted block size then an error is returned.
   * NOTE: The space for one header is reserved in order to allow for a
   * data erase operation after the space has been fully allocated.
   */
  if ((sizeof(nvms_data_header_t) + size) > (NVMS_LL_GetBlockSize() -
                                             nvm.used_size -
                                             sizeof(nvms_data_header_t)))
  {
    return NVMS_OUT_OF_MEM;
  }

  /* This is the condition where we need to compact the current block in
     order to obtain enough space for the new data instance */
  if ((sizeof(nvms_data_header_t) + size) > free)
  {
    warning = true;
    err = garbage_collect();
    if (err != NVMS_NOERROR)
    {
      return err;
    }
  }

  /* Index for the new data */
  if (nvm.slots[slot] == NULL)
  {
    instance = 1;
    oldused = 0;
  }
  else
  {
    instance = nvm.slots[slot]->fields.instance + 1UL;
    oldused = (uint32_t)(nvm.slots[slot]->fields.next->hdr8) - (uint32_t)(nvm.slots[slot]->hdr8);
  }

  /* Position of the new data instance */
  hdrp = nvm.free_next;

  /* Writing the new instance */
  err = append_slot(slot, size, type, slotp, instance);
  if (err != NVMS_NOERROR)
  {
    return err;
  }

  /* Adjusting the counter of the effective used size */
  nvm.slots[slot]  = hdrp;
  nvm.used_size -= oldused;
  nvm.used_size += nvm.slots[slot]->fields.next->hdr8 - nvm.slots[slot]->hdr8;

  return warning ? NVMS_WARNING : NVMS_NOERROR;
}

/**
  * @brief  Erases a slot.
  *
  * @param[in] slot             slot identifier
  *
  * @return                     The operation status.
  * @retval NVMS_NOERROR        if the operation has been successfully completed.
  * @retval NVMS_WARNING        if some repair work has been performed or
  *                             garbage collection happened.
  * @retval NVMS_NOTINIT        if the subsystem has not been initialized yet.
  * @retval NVMS_FLASH_FAILURE  if the flash memory is unusable because HW.
  *                             failures.
  * @retval NVMS_DATA_NOT_FOUND if the data does not exists.
  * @retval NVMS_INTERNAL       if an internal error occurred.
  */
nvms_error_t NVMS_EraseData(nvms_slot_t slot)
{
  bool warning = false;
  uint32_t instance;
  size_t free;
  size_t oldused;
  nvms_error_t err;

  /* Check on initialization */
  if (!is_initialized())
  {
    return NVMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot >= NVMS_CFG_NUM_SLOTS)
  {
    return NVMS_SLOT_INVALID;
  }

  /* Check slot presence */
  if (nvm.slots[slot] == NULL)
  {
    return NVMS_DATA_NOT_FOUND;
  }

  /* Checking for immediately available space */
  free = NVMS_LL_GetBlockSize() - (nvm.free_next->hdr8 -
                                   nvm.header->hdr8);

  /* If the requested space is out of the compacted block size then an
     error is returned.
     NOTE: This condition SHOULD NEVER HAPPEN because the slot write operation
     makes sure to leave to leave enough space for an erase operation */
  if (sizeof(nvms_data_header_t) > (NVMS_LL_GetBlockSize() - nvm.used_size))
  {
    return NVMS_INTERNAL;
  }

  /* This is the condition where we need to compact the current block in
     order to obtain enough space for the new slot instance */
  if (sizeof(nvms_data_header_t) > free)
  {
    warning = true;
    err = garbage_collect();
    if (err != NVMS_NOERROR)
    {
      return err;
    }
  }

  /* Index for the new slot */
  instance = nvm.slots[slot]->fields.instance + 1UL;
  oldused = (uint32_t)(nvm.slots[slot]->fields.next->hdr8) - (uint32_t)(nvm.slots[slot]->hdr8);

  /* Writing the new instance */
  err = append_slot(slot, 0, NVMS_DATA_TYPE_DEFAULT, NULL, instance);
  if (err != NVMS_NOERROR)
  {
    return err;
  }

  /* Adjusting the counter of the effective used size */
  nvm.slots[slot]  = NULL;
  nvm.used_size -= oldused;

  return warning ? NVMS_WARNING : NVMS_NOERROR;
}

/**
  * @brief   Retrieves data for a slot.
  * @note    The returned pointer is valid only until to the next call to
  *          the subsystem because slots can be updated and moved in the flash
  *          array.
  *
  * @param[in] slot           slot identifier
  * @param[out] size_p        pointer to a variable that will contain the slot
  *                           size after the call or @p NULL
  * @param[out] type_p        pointer to a variable that will contain the data
  *                           type after the call or @p NULL
  * @param[out] data_pp       pointer to a pointer to the data or @p NULL
  *
  * @return                     The operation status.
  * @retval NVMS_NOERROR        if the operation has been successfully completed.
  * @retval NVMS_NOTINIT        if the subsystem has not been initialized yet.
  * @retval NVMS_SLOT_INVALID   if the slot number is out of range.
  * @retval NVMS_DATA_NOT_FOUND if the data does not exists.
  */
nvms_error_t NVMS_GetDataWithType(nvms_slot_t slot, size_t *size_p, nvms_data_type_t *type_p,
                                  uint8_t **data_pp)
{
  /* Check on initialization */
  if (!is_initialized())
  {
    return NVMS_NOTINIT;
  }

  /* Check on the slot identifier */
  if (slot >= NVMS_CFG_NUM_SLOTS)
  {
    return NVMS_SLOT_INVALID;
  }

  /* Check slot presence */
  if (nvm.slots[slot] == NULL)
  {
    return NVMS_DATA_NOT_FOUND;
  }

  if (size_p != NULL)
  {
    *size_p = nvm.slots[slot]->fields.data_size;
  }

  if (type_p != NULL)
  {
    *type_p = nvm.slots[slot]->fields.data_type;
  }

  if (data_pp != NULL)
  {
    *data_pp = nvm.slots[slot]->hdr8 + sizeof(nvms_data_header_t);
  }

  return NVMS_NOERROR;
}

/**
  * @}
  */

/**
  * @}
  */

#endif /* KMS_NVM_ENABLED */
#endif /* KMS_ENABLED */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
