/**
  ******************************************************************************
  * @file    se_fwimg.c
  * @author  MCD Application Team
  * @brief   This file provides a set of firmware functions used by the bootloader
  *          to manage Secure Firmware Update functionalities.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"
#include "se_fwimg.h"
#include "se_low_level.h"
#include "se_exception.h"
#include "string.h"

/** @addtogroup SE
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup SE_IMG SE Firmware Image
  * @brief Code used to handle the Firmware Images.
  * This contains functions used by the bootloader.
  * @{
  */


/** @defgroup SE_IMG_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief  Write in Flash protected area
  * @param  pDestination pointer to destination area in Flash
  * @param  pSource pointer to input buffer
  * @param  Length number of bytes to be written
  * @retval SE_SUCCESS if successful, otherwise SE_ERROR
  */

SE_ErrorStatus SE_IMG_Write(uint8_t *pDestination, const uint8_t *pSource, uint32_t Length)
{
  SE_ErrorStatus ret;
  uint32_t i;
  uint32_t inside_header = 0U;
  uint32_t areabegin = (uint32_t)pDestination;
  uint32_t areaend = areabegin + Length - 1U;

  /* Is destination area part of 1 of the firmware image headers ?
     Headers are located inside protected memory   */
  for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
  {
    if ((areabegin >= SlotHeaderAdd[SLOT_ACTIVE_1 + i]) &&
        (areaend < (SlotHeaderAdd[SLOT_ACTIVE_1 + i] + SFU_IMG_IMAGE_OFFSET)))
    {
      inside_header = 1U;
    }
  }

  /* Destination area part of 1 of the firmware image headers */
  if (inside_header == 1U)
  {
    /* Writing in protected memory */
    ret = SE_LL_FLASH_Write(pDestination, pSource, Length);
  }
  else
  {
    /* Abnormal case: this primitive should not be used to access this address */
    ret = SE_ERROR;
  }
  return ret;
}

/**
  * @brief  Read from Flash protected area
  * @param  pDestination pointer to output buffer
  * @param  pSource pointer to source area in Flash
  * @param  Length number of bytes to be read
  * @retval SE_SUCCESS if successful, otherwise SE_ERROR
  */

SE_ErrorStatus SE_IMG_Read(uint8_t *pDestination, const uint8_t *pSource, uint32_t Length)
{
  SE_ErrorStatus ret;
  uint32_t i;
  uint32_t inside_header = 0U;
  uint32_t areabegin = (uint32_t)pSource;
  uint32_t areaend = areabegin + Length - 1U;

  /* Is destination area part of 1 of the firmware image headers ?
     Headers are located inside protected memory   */
  for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
  {
    if ((areabegin >= SlotHeaderAdd[SLOT_ACTIVE_1 + i]) &&
        (areaend < (SlotHeaderAdd[SLOT_ACTIVE_1 + i] + SFU_IMG_IMAGE_OFFSET)))
    {
      inside_header = 1U;
    }
  }

  /* Destination area part of 1 of the firmware image headers */
  if (inside_header == 1U)
  {
    /* Accessing protected memory */
    ret = SE_LL_FLASH_Read(pDestination, pSource, Length);
  }
  else
  {
    /* Abnormal case: this primitive should not be used to access this address */
    ret = SE_ERROR;
  }
  return ret;
}

/**
  * @brief  Erase a Flash protected area
  * @param  pDestination pointer to destination area in Flash
  * @param  Length number of bytes to be erased
  * @retval SE_SUCCESS if successful, otherwise SE_ERROR
  */

SE_ErrorStatus SE_IMG_Erase(uint8_t *pDestination, uint32_t Length)
{
  SE_ErrorStatus ret;
  uint32_t i;
  uint32_t inside_header = 0U;
  uint32_t areabegin = (uint32_t)pDestination;
  uint32_t areaend = areabegin + Length - 1U;

  /* Is destination area part of 1 of the firmware image headers ?
     Headers are located inside protected memory   */
  for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
  {
    if ((areabegin >= SlotHeaderAdd[SLOT_ACTIVE_1 + i]) &&
        (areaend < (SlotHeaderAdd[SLOT_ACTIVE_1 + i] + SFU_IMG_IMAGE_OFFSET)))
    {
      inside_header = 1U;
    }
  }

  /* Destination area part of 1 of the firmware image headers */
  if (inside_header == 1U)
  {
    /* Accessing protected memory */
    ret = SE_LL_FLASH_Erase(pDestination, Length);
  }
  else
  {
    /* Abnormal case: this primitive should not be used to access this address */
    ret = SE_ERROR;
  }
  return ret;
}

#ifdef ENABLE_IMAGE_STATE_HANDLING
/**
  * @brief Service called by the Bootloader to Set the Active Firmware State.
  * The Bootloader allowed state transitions are:
  * FWIMG_STATE_NEW to FWIMG_STATE_SELFTEST
  * FWIMG_STATE_SELFTEST to FWIMG_STATE_INVALID
  * FWIMG_STATE_VALID to FWIMG_STATE_INVALID
  * @param SlotNumber index of the slot in the list
  * @param FwState Active Firmware State that will be set.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_IMG_SetActiveFwState(uint32_t SlotNumber, SE_FwStateTypeDef *pFwState)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  SE_FwRawHeaderTypeDef *pfw_image_header;          /* FW metadata */

  const uint8_t zeros_buffer[32] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                   };

  const uint8_t fives_buffer[32] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, \
                                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, \
                                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, \
                                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
                                   };

  SE_FwStateTypeDef current_state = FWIMG_STATE_INVALID;

  /* Control parameter : SlotNumber */
  if ((SlotNumber >= SLOT_ACTIVE_1) && (SlotNumber < (SLOT_ACTIVE_1 + SFU_NB_MAX_ACTIVE_IMAGE)))
  {
    pfw_image_header = (SE_FwRawHeaderTypeDef *) SlotHeaderAdd[SlotNumber];
  }
  else
  {
    return SE_ERROR;
  }

  /*
   * The Firmware State is available in the header of the slot #0.
   * In the header (SE_FwRawHeaderTypeDef.FwImageState), these states are encoded as follows:
   * FWIMG_STATE_INVALID  : 32 * 0x00, 32 * 0x00, 32 * 0x00
   * FWIMG_STATE_VALID    : 32 * 0xFF, 32 * 0x00, 32 * 0x00
   * FWIMG_STATE_VALID_ALL: 32 * 0xFF, 32 * 0x55, 32 * 0x00
   * FWIMG_STATE_SELFTEST : 32 * 0xFF, 32 * 0xFF, 32 * 0x00
   * FWIMG_STATE_NEW      : 32 * 0xFF, 32 * 0xFF, 32 * 0xFF
   */
  /* Read current state multiple times to thwart glitch attacks */
  e_ret_status = SE_IMG_GetActiveFwState(SlotNumber, &current_state);
  if (e_ret_status != SE_SUCCESS)
  {
    return SE_ERROR;
  }

  /* Bootloader can change state
   * NEW to SELFTEST
   * SELFTEST to INVALID or VALID or VALID_ALL
   * VALID to INVALID
   */
  if ((current_state == FWIMG_STATE_NEW) && (*pFwState != FWIMG_STATE_SELFTEST))
  {
    return SE_ERROR;
  }
  if ((current_state == FWIMG_STATE_SELFTEST) && !((*pFwState == FWIMG_STATE_INVALID)
                                                   || (*pFwState == FWIMG_STATE_VALID)
                                                   || (*pFwState == FWIMG_STATE_VALID_ALL)))
  {
    return SE_ERROR;
  }
  if ((current_state == FWIMG_STATE_VALID) && (*pFwState != FWIMG_STATE_INVALID))
  {
    return SE_ERROR;
  }

  switch (*pFwState)
  {
    case FWIMG_STATE_VALID:
    {
      /* state transition 32*0xFF, 32*0xFF, 32*0x00 -> 32*0xFF, 32*0x00, 32*0x00 */
      e_ret_status = SE_LL_FLASH_Write((void *) pfw_image_header->FwImageState[1], zeros_buffer, sizeof(zeros_buffer));
      break;
    }

    case FWIMG_STATE_VALID_ALL:
    {
      /* state transition 32*0xFF, 32*0xFF, 32*0x00 -> 32*0xFF, 32*0x55, 32*0x00 */
      e_ret_status = SE_LL_FLASH_Write((void *) pfw_image_header->FwImageState[1], fives_buffer, sizeof(fives_buffer));
      break;
    }

    case FWIMG_STATE_INVALID:
    {
      /* state transition 32*0xFF, 32*0xFF, 32*0x00 -> 32*0x00, 32*0x00, 32*0x00 Selftest -> Invalid
                       or 32*0xFF, 32*0x00, 32*0x00 -> 32*0x00, 32*0x00, 32*0x00 Valid    -> Invalid */
      e_ret_status = SE_LL_FLASH_Write((void *) pfw_image_header->FwImageState[0], zeros_buffer, sizeof(zeros_buffer));
      if (e_ret_status == SE_SUCCESS)
      {
        if (current_state == FWIMG_STATE_SELFTEST)
        {
          e_ret_status = SE_LL_FLASH_Write((void *) pfw_image_header->FwImageState[1], zeros_buffer,
                                           sizeof(zeros_buffer));
        }
      }
      break;
    }
    case FWIMG_STATE_SELFTEST:
    {
      /* state transition 32*0xFF, 32*0xFF, 32*0xFF -> 32*0xFF, 32*0xFF, 32*0x00 New -> Selftest */
      e_ret_status = SE_LL_FLASH_Write((void *) pfw_image_header->FwImageState[2], zeros_buffer, sizeof(zeros_buffer));
      break;
    }
    default:
    {
      e_ret_status = SE_ERROR;
    }
  }
  return e_ret_status;
}

/**
  * @brief Service called by the User Application to retrieve the Active Firmware State.
  * @param SlotNumber index of the slot in the list
  * @param pFwState Active Firmware State structure that will be filled.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_IMG_GetActiveFwState(uint32_t SlotNumber, SE_FwStateTypeDef *pFwState)
{
  SE_ErrorStatus e_ret_status = SE_ERROR;
  uint8_t buffer[3U * 32U]; /* to read FW State from FLASH */
  SE_FwRawHeaderTypeDef *pfw_image_header;  /* FW metadata */
  const uint8_t ones_buffer[32] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                  };

  const uint8_t zeros_buffer[32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                   };

  const uint8_t fives_buffer[32] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, \
                                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, \
                                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, \
                                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
                                   };

  /* Check the pointer allocation */
  if (NULL == pFwState)
  {
    return SE_ERROR;
  }

  /* Control parameter : SlotNumber */
  if (((SlotNumber >= SLOT_ACTIVE_1) && (SlotNumber < (SLOT_ACTIVE_1 + SFU_NB_MAX_ACTIVE_IMAGE))) ||
      ((SlotNumber >= SLOT_DWL_1) && (SlotNumber < (SLOT_DWL_1 + SFU_NB_MAX_DWL_AREA))))
  {
    pfw_image_header = (SE_FwRawHeaderTypeDef *) SlotHeaderAdd[SlotNumber];
  }
  else
  {
    return SE_ERROR;
  }

  /*
   * The Firmware State is available in the header of the slot #0.
   */
  e_ret_status = SE_LL_FLASH_Read(buffer, &(pfw_image_header->FwImageState[0][0]), sizeof(buffer));

  /*
   * We do not check the header validity.
   * We just copy the information.
  */

  /* In the header (SE_FwRawHeaderTypeDef.FwImageState), the image states are encoded as follows:
   * FWIMG_STATE_INVALID  : 32 * 0x00, 32 * 0x00, 32 * 0x00
   * FWIMG_STATE_VALID    : 32 * 0xFF, 32 * 0x00, 32 * 0x00
   * FWIMG_STATE_VALID_ALL: 32 * 0xFF, 32 * 0x55, 32 * 0x00
   * FWIMG_STATE_SELFTEST : 32 * 0xFF, 32 * 0xFF, 32 * 0x00
   * FWIMG_STATE_NEW      : 32 * 0xFF, 32 * 0xFF, 32 * 0xFF
   */
  if (e_ret_status == SE_SUCCESS)
  {
    if (memcmp(&buffer[0U], ones_buffer, sizeof(ones_buffer)) == 0)
    {
      if (memcmp(&buffer[32U], ones_buffer, sizeof(ones_buffer)) == 0)
      {
        if (memcmp(&buffer[64U], ones_buffer, sizeof(ones_buffer)) == 0)
        {
          *pFwState = FWIMG_STATE_NEW;
        }
        else if (memcmp(&buffer[64U], zeros_buffer, sizeof(zeros_buffer)) == 0)
        {
          *pFwState = FWIMG_STATE_SELFTEST;
        }
        else
        {
          *pFwState = FWIMG_STATE_INVALID;
        }
      }
      else if (memcmp(&buffer[32U], zeros_buffer, sizeof(zeros_buffer)) == 0)
      {
        *pFwState = FWIMG_STATE_VALID;
      }
      else if (memcmp(&buffer[32U], fives_buffer, sizeof(fives_buffer)) == 0)
      {
        *pFwState = FWIMG_STATE_VALID_ALL;
      }
      else
      {
        *pFwState = FWIMG_STATE_INVALID;
      }
    }
    else
    {
      *pFwState = FWIMG_STATE_INVALID;
    }
  }

  return e_ret_status;
}

#endif /* ENABLE_IMAGE_STATE_HANDLING */
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
