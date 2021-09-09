/**
  ******************************************************************************
  * @file    se_user_code.c
  * @author  MCD Application Team
  * @brief   Secure Engine User code example module.
  *          This file demonstrates how to call user defined services running
  *          in Secure Engine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"
#include "com.h"
#include "common.h"
#include "stm32g4xx_hal.h"
#include "se_user_code.h"
#include "se_interface_application.h"
#include "sfu_fwimg_regions.h"
#include <string.h> /* needed for memset */


/** @addtogroup USER_APP User App Example
  * @{
  */

/** @addtogroup  SE_USER_CODE Secure Engine User Code Example
  * @brief Example of user defined code running in Secure Engine.
  *        This code provides user defined services to the user application.
  * @{
  */

/** @defgroup  SE_USER_CODE_Private_Variables Private Variables
  * @{
  */

/**
  * @}
  */

/** @defgroup  SE_USER_CODE_Private_Functions Private Functions
  * @{
  */

/**
  * @brief  Display the SE_USER_CODE Menu choices on hyperterminal
  * @param  None.
  * @retval None.
  */
static void SE_USER_CODE_PrintMenu(void)
{
  printf("\r\n=== Call User Defined Code running in Secure Engine ===\r\n\n");
  printf("  Get firmware information of SLOT_ACTIVE_1 ------------- 1\r\n\n");
  printf("  Get firmware information of SLOT_ACTIVE_2 ------------- 2\r\n\n");
  printf("  Get firmware information of SLOT_ACTIVE_3 ------------- 3\r\n\n");
  printf("  Previous Menu ----------------------------------------- x\r\n\n");
  printf("  Selection :\r\n\n");
}

/**
  * @brief  Get FW information.
  * @param  SlotNumber slot identification
  * @retval HAL Status.
  */
static void SE_USER_CODE_GetFwInfo(uint32_t SlotNumber)
{
  SE_ErrorStatus se_retCode = SE_ERROR;
  SE_StatusTypeDef se_Status = SE_KO;
  SE_APP_ActiveFwInfo_t sl_FwInfo;

  memset(&sl_FwInfo, 0xFF, sizeof(SE_APP_ActiveFwInfo_t));


  printf("If the Secure User Memory is enabled you should not be able to call a SE service and get stuck.\r\n\n");
  printf("  -- Calling FwInfo service.\r\n\n");
  printf("Press the RESET button to restart the device (or wait until IWDG expires if enabled).\r\n\n");

  /* Get FW info */
  se_retCode = SE_APP_GetActiveFwInfo(&se_Status, SlotNumber, &sl_FwInfo);

  if ((SE_SUCCESS == se_retCode) && (SE_OK == se_Status))
  {
    /* Print the result */
    printf("Firmware Info:\r\n");
    printf("\tActiveFwVersion: %d\r\n", sl_FwInfo.ActiveFwVersion);
    printf("\tActiveFwSize: %d bytes\r\n", sl_FwInfo.ActiveFwSize);
  }
  else
  {
    /* Failure */
    printf("  -- !!Operation failed!! \r\n\n");
  }

  /* This point should not be reached */
  printf("  -- !! Secure User Memory protection is NOT ENABLED !!\r\n\n");
}


/**
  * @}
  */

/** @addtogroup  SE_USER_CODE_Exported_Functions
  * @{
  */

/**
  * @brief  Run get firmware info menu.
  * @param  None
  * @retval HAL Status.
  */
void SE_USER_CODE_RunMenu(void)
{
  uint8_t key = 0U;
  uint32_t exit = 0U;
  uint32_t slot_number = 0U;

  /*Print Main Menu message*/
  SE_USER_CODE_PrintMenu();

  while (exit == 0U)
  {
    key = 0U;
    slot_number = 0U;

    /* If the SecureBoot configured the IWDG, UserApp must reload IWDG counter with value defined in the reload
       register */
    WRITE_REG(IWDG->KR, IWDG_KEY_RELOAD);

    /* Clean the input path */
    COM_Flush();

    /* Receive key */
    if (COM_Receive(&key, 1U, RX_TIMEOUT) == HAL_OK)
    {
      switch (key)
      {
        case '1' :
          slot_number = SLOT_ACTIVE_1;
          break;
        case '2' :
          slot_number = SLOT_ACTIVE_2;
          break;
        case '3' :
          slot_number = SLOT_ACTIVE_3;
          break;
        case 'x' :
          exit = 1U;
          break;
        default:
          printf("Invalid Number !\r");
          break;
      }

      if (exit != 1U)
      {
        if (SlotStartAdd[slot_number] == 0U)
        {
          printf("SLOT_ACTIVE_%d is not configured !\r", slot_number);
        }
        else
        {
          SE_USER_CODE_GetFwInfo(slot_number);
        }

        /*Print Main Menu message*/
        SE_USER_CODE_PrintMenu();
      }
    }
  }
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


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
