/**
  ******************************************************************************
  * @file    se_user_application.c
  * @author  MCD Application Team
  * @brief   Secure Engine USER APPLICATION module.
  *          This file is a placeholder for the code dedicated to the user application.
  *          These services are used by the application.
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
#include "se_user_application.h"
#include "se_low_level.h"
#include "string.h"
#include "sfu_fwimg_regions.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup  SE_APPLI SE Code for Application
  * @brief This file is a placeholder for the code dedicated to the user application.
  * It contains the code written by the end user.
  * The code used by the application to handle the Firmware images is located in se_fwimg.c.
  * @{
  */

/** @defgroup SE_APPLI_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief Service called by the User Application to retrieve the Active Firmware Info.
  * @param  SlotNumber index of the slot in the list
  * @param p_FwInfo Active Firmware Info structure that will be filled.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_APPLI_GetActiveFwInfo(uint32_t SlotNumber, SE_APP_ActiveFwInfo_t *p_FwInfo)
{
  SE_ErrorStatus e_ret_status;
  uint8_t buffer[SE_FW_HEADER_TOT_LEN];     /* to read FW metadata from FLASH */
  SE_FwRawHeaderTypeDef *pfw_image_header;  /* FW metadata */

  /* Check the pointer allocation */
  if (NULL == p_FwInfo)
  {
    return SE_ERROR;
  }

  /* Check Slot_Number value */
  if (SlotNumber > SFU_NB_MAX_ACTIVE_IMAGE)
  {
    return SE_ERROR;
  }
  /*
   * The Firmware Information is available in the header of the active slot.
   */
  e_ret_status = SE_LL_FLASH_Read(buffer, (uint8_t *) SlotHeaderAdd[SlotNumber], sizeof(buffer));
  if (e_ret_status != SE_ERROR)
  {
    pfw_image_header = (SE_FwRawHeaderTypeDef *)(uint32_t)buffer;

    /*
     * We do not check the header validity.
     * We just copy the information.
     */
    p_FwInfo->ActiveFwVersion = pfw_image_header->FwVersion;
    p_FwInfo->ActiveFwSize = pfw_image_header->FwSize;
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

/**
  * @}
  */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
