/**
  ******************************************************************************
  * @file    se_interface_application.c
  * @author  MCD Application Team
  * @brief   Secure Engine Interface module.
  *          This file provides set of firmware functions to manage SE Interface
  *          functionalities. These services are used by the application.
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

/* Place code in a specific section*/
#if defined(__ICCARM__)
#pragma default_function_attributes = @ ".SE_IF_Code"
#elif defined(__CC_ARM)
#pragma arm section code = ".SE_IF_Code"
#endif /* __ICCARM__ */

/* Includes ------------------------------------------------------------------*/
#include "main.h"                       /* se_interface_application.c is compiled in SBSFU project using main.h
                                         * from this project
                                         */
#include "se_low_level.h"               /* This file is part of SE_CoreBin and adapts the Secure Engine
                                         * (and its interface) to the STM32 board specificities
                                         */
#include "se_interface_common.h"
#include "se_callgate.h"
#include "se_interface_application.h"
#include "se_intrinsics.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @defgroup  SE_INTERFACE Secure Engine Interface
  * @brief APIs the SE users (bootloader, user app) can call.
  * @note Please note that this code is compiled in the SB_SFU project (not in SE_CoreBin).
  *       Nevertheless, this file also includes "se_low_level.h" from SE_CoreBin to make sure the SE interface is in
  *       sync with the SE code.
  * @{
  */

/** @defgroup  SE_INTERFACE_APPLICATION Secure Engine Interface for Application
  * @brief Interface functions the User Application calls to use SE services.
  * @{
  *
  * "se_interface" is built and set in the context of the SB_SFU project.
  * Then some symbols are exported to the User Application for the User Application
  * to call some Secure Engine services through "se_interface"
  * (SB_SFU and the User Application are exclusive, they do not run in parallel).
  * As "se_interface" is set by SB_SFU but can be used by the User Application, it is important
  * not to introduce global variables if you do not want to create dependencies between the SB_SFU RAM mapping (SRAM1)
  * and the User Application RAM mapping.
  */

/* DO NOT ADD ANY VARIABLE HERE, SEE EXPLANATIONS ABOVE */

/** @defgroup SE_INTERFACE_APPLICATION_Exported_Functions Exported Functions
  * @ Functions the User Application calls to run the SE services.
  * @{
  */

/**
  * @brief Service called by the User Application to retrieve the Active Firmware Info.
  * @param peSE_Status Secure Engine Status.
  * @param SlotNumber index of the slot in the list
  * @param pFwInfo Active Firmware Info structure that will be filled.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
__root SE_ErrorStatus SE_APP_GetActiveFwInfo(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                             SE_APP_ActiveFwInfo_t *pFwInfo)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {SlotNumber, (uint32_t)pFwInfo};
    SE_SysCall(&e_ret_status, SE_APP_GET_ACTIVE_FW_INFO, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /*Enter Secure Mode*/
    SE_EnterSecureMode(&primask_bit);

    /*Secure Engine Call*/
    e_ret_status = (*SE_CallGatePtr)(SE_APP_GET_ACTIVE_FW_INFO, peSE_Status, primask_bit, SlotNumber, pFwInfo);

    /*Exit Secure Mode*/
    SE_ExitSecureMode(primask_bit);

#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

#if defined(ENABLE_IMAGE_STATE_HANDLING)
/**
  * @brief Service called by the User Application to validate a Active Firmware.
  * @param peSE_Status Secure Engine Status.
  * @param SlotNumber index of the slot in the list
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
__root SE_ErrorStatus SE_APP_ValidateFw(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[1] = {SlotNumber};
    SE_SysCall(&e_ret_status, SE_APP_VALIDATE_FW, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /*Enter Secure Mode*/
    SE_EnterSecureMode(&primask_bit);

    /*Secure Engine Call*/
    e_ret_status = (*SE_CallGatePtr)(SE_APP_VALIDATE_FW, peSE_Status, primask_bit, SlotNumber);

    /*Exit Secure Mode*/
    SE_ExitSecureMode(primask_bit);

#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}
#endif /* ENABLE_IMAGE_STATE_HANDLING */

#ifdef ENABLE_IMAGE_STATE_HANDLING
/**
  * @brief Service called by the User Application to retrieve an Active Firmware state.
  * @param peSE_Status Secure Engine Status.
  * @param SlotNumber index of the slot in the list
  * @param pFwState Active Firmware state structure that will be filled.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
__root SE_ErrorStatus SE_APP_GetActiveFwState(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                              SE_FwStateTypeDef *pFwState)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {SlotNumber, (uint32_t)pFwState};
    SE_SysCall(&e_ret_status, SE_APP_GET_FW_STATE, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /*Enter Secure Mode*/
    SE_EnterSecureMode(&primask_bit);

    /*Secure Engine Call*/
    e_ret_status = (*SE_CallGatePtr)(SE_APP_GET_FW_STATE, peSE_Status, primask_bit, SlotNumber, pFwState);

    /*Exit Secure Mode*/
    SE_ExitSecureMode(primask_bit);

#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}
#endif /* ENABLE_IMAGE_STATE_HANDLING */

/**
  * @brief Service called by the User Application to disable all IRQ.
  * @param peSE_Status Secure Engine Status.
  * @param pIrqState buffer where current IRQ states will be saved to be restored later
  * @param IrqStateNb number of IRQ states that can be saved
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
#if defined(SFU_ISOLATE_SE_WITH_MPU) && defined(UPDATE_IRQ_SERVICE)
__root SE_ErrorStatus SE_SYS_SaveDisableIrq(SE_StatusTypeDef *peSE_Status, uint32_t *pIrqState, uint32_t IrqStateNb)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pIrqState, IrqStateNb};
    SE_SysCall(&e_ret_status, SE_SYS_SAVE_DISABLE_IRQ, peSE_Status, &params);
  }
  else
  {
    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /*Enter Secure Mode*/
    SE_EnterSecureMode(&primask_bit);

    /*Secure Engine Call*/
    e_ret_status = (*SE_CallGatePtr)(SE_SYS_SAVE_DISABLE_IRQ, peSE_Status, primask_bit, pIrqState, IrqStateNb);

    /*Exit Secure Mode*/
    SE_ExitSecureMode(primask_bit);
  }
  return e_ret_status;
}
#endif /* SFU_ISOLATE_SE_WITH_MPU && UPDATE_IRQ_SERVICE */

/**
  * @brief Service called by the User Application to restore IRQ.
  * @param peSE_Status Secure Engine Status.
  * @param pIrqState buffer containing IRQ states to restore
  * @param IrqStateNb number of IRQ states to restore
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
#if defined(SFU_ISOLATE_SE_WITH_MPU) && defined(UPDATE_IRQ_SERVICE)
__root SE_ErrorStatus SE_SYS_RestoreEnableIrq(SE_StatusTypeDef *peSE_Status, uint32_t *pIrqState, uint32_t IrqStateNb)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pIrqState, IrqStateNb};
    SE_SysCall(&e_ret_status, SE_SYS_RESTORE_ENABLE_IRQ, peSE_Status, &params);
  }
  else
  {
    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /*Enter Secure Mode*/
    SE_EnterSecureMode(&primask_bit);

    /*Secure Engine Call*/
    e_ret_status = (*SE_CallGatePtr)(SE_SYS_RESTORE_ENABLE_IRQ, peSE_Status, primask_bit, pIrqState, IrqStateNb);

    /*Exit Secure Mode*/
    SE_ExitSecureMode(primask_bit);
  }
  return e_ret_status;
}
#endif /* SFU_ISOLATE_SE_WITH_MPU && UPDATE_IRQ_SERVICE */


#ifdef SFU_ISOLATE_SE_WITH_MPU
__root void SE_APP_SVC_Handler(uint32_t *args)
{
  SE_SVC_Handler(args);
}
#endif /* SFU_ISOLATE_SE_WITH_MPU */

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

/* Stop placing data in specified section*/
#if defined(__ICCARM__)
#pragma default_function_attributes =
#elif defined(__CC_ARM)
#pragma arm section code
#endif /* __ICCARM__ */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
