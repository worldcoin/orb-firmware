/**
  ******************************************************************************
  * @file    sfu_low_level_security.c
  * @author  MCD Application Team
  * @brief   SFU Security Low Level Interface module
  *          This file provides set of firmware functions to manage SFU security
  *          low level interface.
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
#include "main.h"
#include "sfu_low_level_security.h"
#include "sfu_low_level_flash_int.h"
#include "sfu_low_level.h"
#include "sfu_trace.h"
#include "sfu_boot.h"
#include "sfu_fsm_states.h" /* needed for sfu_error.h */
#include "sfu_error.h"
#include "sfu_standalone_loader.h"
#include "stm32g4xx_it.h"
#include "sfu_mpu_isolation.h"        /* Secure Engine isolated thanks to MPU */

#ifndef  SFU_WRP_PROTECT_ENABLE
#warning "SFU_WRP_PROTECT_DISABLED"
#endif /* SFU_WRP_PROTECT_ENABLE */

#ifndef  SFU_RDP_PROTECT_ENABLE
#warning "SFU_RDP_PROTECT_DISABLED"
#endif /* SFU_RDP_PROTECT_ENABLE */

#ifndef  SFU_PCROP_PROTECT_ENABLE
#warning "SFU_PCROP_PROTECT_DISABLED"
#endif /* SFU_PCROP_PROTECT_ENABLE */

#ifndef  SFU_MPU_PROTECT_ENABLE
#warning "SFU_MPU_PROTECT_DISABLED"
#endif /* SFU_MPU_PROTECT_ENABLE */



#ifndef  SFU_TAMPER_PROTECT_ENABLE
#warning "SFU_TAMPER_PROTECT_DISABLED"
#endif /* SFU_TAMPER_PROTECT_ENABLE */

#ifndef  SFU_DAP_PROTECT_ENABLE
#warning "SFU_DAP_PROTECT_DISABLED"
#endif /* SFU_DAP_PROTECT_ENABLE */

#ifndef  SFU_DMA_PROTECT_ENABLE
#warning "SFU_DMA_PROTECT_DISABLED"
#endif /* SFU_DMA_PROTECT_ENABLE */

#ifndef  SFU_IWDG_PROTECT_ENABLE
#warning "SFU_IWDG_PROTECT_DISABLED"
#endif /* SFU_IWDG_PROTECT_ENABLE */

#ifndef  SFU_SECURE_USER_PROTECT_ENABLE
#warning "SFU_SECURE_USER_PROTECT_DISABLED"
#endif /* SFU_SECURE_USER_PROTECT_ENABLE */

/* Private typedef -----------------------------------------------------------*/
typedef enum
{
  SFU_FALSE = 0U,
  SFU_TRUE = !SFU_FALSE
} SFU_BoolTypeDef;

typedef struct
{
  uint8_t                Number;            /*!< Specifies the number of the region to protect. This parameter can be a
                                                 value of CORTEX_MPU_Region_Number */
  uint32_t               BaseAddress;       /*!< Specifies the base address of the region to protect. */
  uint8_t                Size;              /*!< Specifies the size of the region to protect. */
  uint8_t                AccessPermission;  /*!< Specifies the region access permission type. This parameter can be a
                                                 value of CORTEX_MPU_Region_Permission_Attributes */
  uint8_t                DisableExec;       /*!< Specifies the instruction access status. This parameter can be a value
                                                 of  CORTEX_MPU_Instruction_Access */
  uint8_t                SubRegionDisable;  /*!< Specifies the sub region field (region is divided in 8 slices) when bit
                                                 is 1 region sub region is disabled */
  uint8_t                Tex;               /*!< Specifies the tex value  */
  uint8_t                Cacheable;         /*!< Specifies the cacheable value  */
  uint8_t                Bufferable;        /*!< Specifies the cacheable value  */
} SFU_MPU_InitTypeDef;

typedef uint32_t      SFU_ProtectionTypeDef;  /*!<   SFU HAL IF Protection Type Def*/

/* Private variables ---------------------------------------------------------*/
#ifdef  SFU_IWDG_PROTECT_ENABLE
static IWDG_HandleTypeDef   IwdgHandle;

#endif /* SFU_IWDG_PROTECT_ENABLE */
#ifdef  SFU_MPU_PROTECT_ENABLE
static SFU_MPU_InitTypeDef MpuAreas[] =

{
  /*  se execution */
  {
    SFU_PROTECT_MPU_EXEC_SE_RGNV, SFU_PROTECT_MPU_EXEC_SE_START, SFU_PROTECT_MPU_EXEC_SE_SIZE,
    SFU_PROTECT_MPU_EXEC_SE_PERM, SFU_PROTECT_MPU_EXEC_SE_EXECV, SFU_PROTECT_MPU_EXEC_SE_SREG,
    SFU_PROTECT_MPU_EXEC_SE_TEXV, SFU_PROTECT_MPU_EXEC_SE_C, SFU_PROTECT_MPU_EXEC_SE_B
  },

  /* se ram  */
  {
    SFU_PROTECT_MPU_SRAM_SE_RGNV, SFU_PROTECT_MPU_SRAM_SE_START, SFU_PROTECT_MPU_SRAM_SE_SIZE,
    SFU_PROTECT_MPU_SRAM_SE_PERM, SFU_PROTECT_MPU_SRAM_SE_EXECV, SFU_PROTECT_MPU_SRAM_SE_SREG,
    SFU_PROTECT_MPU_SRAM_SE_TEXV, SFU_PROTECT_MPU_SRAM_SE_C, SFU_PROTECT_MPU_SRAM_SE_B
  },

  /* SRAM access */
  {
    SFU_PROTECT_MPU_SRAMACC_RGNV, SFU_PROTECT_MPU_SRAMACC_START, SFU_PROTECT_MPU_SRAMACC_SIZE,
    SFU_PROTECT_MPU_SRAMACC_PERM, SFU_PROTECT_MPU_SRAMACC_EXECV, SFU_PROTECT_MPU_SRAMACC_SREG,
    SFU_PROTECT_MPU_SRAMACC_TEXV, SFU_PROTECT_MPU_SRAMACC_C, SFU_PROTECT_MPU_SRAMACC_B
  },

  /* install header : read /write only for privileged */
  {
    SFU_PROTECT_MPU_HEADER_RGNV, SFU_PROTECT_MPU_HEADER_START, SFU_PROTECT_MPU_HEADER_SIZE,
    SFU_PROTECT_MPU_HEADER_PERM, SFU_PROTECT_MPU_HEADER_EXECV, SFU_PROTECT_MPU_HEADER_SREG,
    SFU_PROTECT_MPU_HEADER_TEXV, SFU_PROTECT_MPU_HEADER_C, SFU_PROTECT_MPU_HEADER_B
  },

  /*  Flash execution */
  {
    SFU_PROTECT_MPU_FLASHEXE_RGNV, SFU_PROTECT_MPU_FLASHEXE_START, SFU_PROTECT_MPU_FLASHEXE_SIZE,
    SFU_PROTECT_MPU_FLASHEXE_PERM, SFU_PROTECT_MPU_FLASHEXE_EXECV, SFU_PROTECT_MPU_FLASHEXE_SREG,
    SFU_PROTECT_MPU_FLASHEXE_TEXV, SFU_PROTECT_MPU_FLASHEXE_C, SFU_PROTECT_MPU_FLASHEXE_B
  },
  /*  Flash access for read write, slot  , swap */
  {
    SFU_PROTECT_MPU_FLASHACC_RGNV, SFU_PROTECT_MPU_FLASHACC_START, SFU_PROTECT_MPU_FLASHACC_SIZE,
    SFU_PROTECT_MPU_FLASHACC_PERM, SFU_PROTECT_MPU_FLASHACC_EXECV, SFU_PROTECT_MPU_FLASHACC_SREG,
    SFU_PROTECT_MPU_FLASHACC_TEXV, SFU_PROTECT_MPU_FLASHACC_C, SFU_PROTECT_MPU_FLASHACC_B
  },
  /*  peripheral  */
  {
    SFU_PROTECT_MPU_PERIPH_1_RGNV, SFU_PROTECT_MPU_PERIPH_1_START, SFU_PROTECT_MPU_PERIPH_1_SIZE,
    SFU_PROTECT_MPU_PERIPH_1_PERM, SFU_PROTECT_MPU_PERIPH_1_EXECV, SFU_PROTECT_MPU_PERIPH_1_SREG,
    SFU_PROTECT_MPU_PERIPH_1_TEXV, SFU_PROTECT_MPU_PERIPH_1_C, SFU_PROTECT_MPU_PERIPH_1_B
  },
  {
    SFU_PROTECT_MPU_PERIPH_2_RGNV, SFU_PROTECT_MPU_PERIPH_2_START, SFU_PROTECT_MPU_PERIPH_2_SIZE,
    SFU_PROTECT_MPU_PERIPH_2_PERM, SFU_PROTECT_MPU_PERIPH_2_EXECV, SFU_PROTECT_MPU_PERIPH_2_SREG,
    SFU_PROTECT_MPU_PERIPH_2_TEXV, SFU_PROTECT_MPU_PERIPH_2_C, SFU_PROTECT_MPU_PERIPH_2_B
  }
};
#endif /* SFU_MPU_PROTECT_ENABLE */

/* Private function prototypes -----------------------------------------------*/
static SFU_ErrorStatus SFU_LL_SECU_CheckFlashConfiguration(FLASH_OBProgramInitTypeDef *psFlashOptionBytes);
static SFU_ErrorStatus SFU_LL_SECU_SetFlashConfiguration(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                         SFU_BoolTypeDef *pbIsProtectionToBeApplied);

#ifdef SFU_RDP_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionRDP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                    SFU_BoolTypeDef *pbIsProtectionToBeApplied);
#endif /*SFU_RDP_PROTECT_ENABLE*/

#ifdef SFU_WRP_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_CheckProtectionWRP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes);
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionWRP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                    SFU_BoolTypeDef *pbIsProtectionToBeApplied);
#endif /*SFU_WRP_PROTECT_ENABLE*/

#ifdef SFU_PCROP_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_CheckProtectionPCROP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes);
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionPCROP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                      SFU_BoolTypeDef *pbIsProtectionToBeApplied);
#endif /*SFU_PCROP_PROTECT_ENABLE*/

#ifndef SFU_DAP_PROTECT_ENABLE
#endif /*SFU_DAP_PROTECT_ENABLE*/


#ifdef SFU_IWDG_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionIWDG(void);
#endif /*SFU_IWDG_PROTECT_ENABLE*/

#ifdef SFU_CLCK_MNTR_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionCLOCK_MONITOR(void);
#endif /*SFU_CLCK_MNTR_PROTECT_ENABLE*/

#ifdef SFU_TEMP_MNTR_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionTEMP_MONITOR(void);
#endif /*SFU_TEMP_MNTR_PROTECT_ENABLE*/

#ifdef  SFU_SECURE_USER_PROTECT_ENABLE
static SFU_ErrorStatus SFU_LL_SECU_CheckProtectionSecUser(FLASH_OBProgramInitTypeDef *psFlashOptionBytes);
static SFU_ErrorStatus SFU_LL_SECU_SetProtectionSecUser(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                        SFU_BoolTypeDef *pbIsProtectionToBeApplied);
#endif /* SFU_SECURE_USER_PROTECT_ENABLE */


/* Functions Definition : helper ---------------------------------------------*/
#ifdef SFU_MPU_PROTECT_ENABLE
/**
  * @brief  Check MPU configuration
  * @param  MPU_InitStruct Configuration to be checked
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus MPU_CheckConfig(MPU_Region_InitTypeDef *pMPUInitStruct)
{
  uint32_t mpu_rasr = 0UL;

  /* Set the Region number */
  MPU->RNR = pMPUInitStruct->Number;

  mpu_rasr |= (((uint32_t)pMPUInitStruct->DisableExec        << MPU_RASR_XN_Pos) & MPU_RASR_XN_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->AccessPermission   << MPU_RASR_AP_Pos) & MPU_RASR_AP_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->TypeExtField       << MPU_RASR_TEX_Pos) & MPU_RASR_TEX_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->IsShareable        << MPU_RASR_S_Pos) & MPU_RASR_S_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->IsCacheable        << MPU_RASR_C_Pos) & MPU_RASR_C_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->IsBufferable       << MPU_RASR_B_Pos) & MPU_RASR_B_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->SubRegionDisable   << MPU_RASR_SRD_Pos) & MPU_RASR_SRD_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->Size               << MPU_RASR_SIZE_Pos) & MPU_RASR_SIZE_Msk);
  mpu_rasr |= (((uint32_t)pMPUInitStruct->Enable             << MPU_RASR_ENABLE_Pos) & MPU_RASR_ENABLE_Msk);

  if (((MPU->RBAR & MPU_RBAR_ADDR_Msk) == pMPUInitStruct->BaseAddress) && (MPU->RASR == mpu_rasr))
  {
    return SFU_SUCCESS;
  }
  else
  {
    return SFU_ERROR;
  }
}
#endif /* SFU_MPU_PROTECT_ENABLE */

/* Functions Definition ------------------------------------------------------*/

/**
  * @brief  Check and if not applied apply the Static security  protections to
  *         all the SfuEn Sections in Flash: RDP, WRP, PCRoP. Static security protections
  *         those protections not impacted by a Reset. They are set using the Option Bytes
  *         When the device is locked (RDP Level2), these protections cannot be changed anymore
  * @param  None
  * @note   By default, the best security protections are applied to the different
  *         flash sections in order to maximize the security level for the specific MCU.
  * @retval uint32_t CRC (returned value is the combination of all the applied protections.
  *         If different from SFU_STD_PROTECTION_ALL, 1 or more protections cannot be applied)
  */
SFU_ErrorStatus SFU_LL_SECU_CheckApplyStaticProtections(void)
{
  FLASH_OBProgramInitTypeDef flash_option_bytes;
  SFU_BoolTypeDef is_protection_to_be_applied = SFU_FALSE;
  SFU_ErrorStatus e_ret_status = SFU_SUCCESS;

  /* Unlock the Flash to enable the flash control register access *************/
  (void) HAL_FLASH_Unlock();

  /* Clear OPTVERR bit set on virgin samples */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  /* Unlock the Options Bytes *************************************************/
  (void) HAL_FLASH_OB_Unlock();

  /* Get Option Bytes status for FLASH_BANK_1: WRP AREA_A, PCRoP, SecureArea **/
  flash_option_bytes.WRPArea     = SFU_PROTECT_WRP_AREA_1;
  flash_option_bytes.PCROPConfig = FLASH_BANK_1;
  flash_option_bytes.SecBank = FLASH_BANK_1;
  (void) HAL_FLASHEx_OBGetConfig(&flash_option_bytes);

  /* Check/Apply RDP_Level 1. This is the minimum protection allowed */
  /* if RDP_Level 2 is already applied it's not possible to modify the OptionBytes anymore */
  if (flash_option_bytes.RDPLevel == OB_RDP_LEVEL_2)
  {
    /* Sanity check of the (enabled) static protections */
    if (SFU_LL_SECU_CheckFlashConfiguration(&flash_option_bytes) != SFU_SUCCESS)
    {
      LOG_DEBUG("= [SBOOT] Flash configuration failed! Product blocked.");
      /* Security issue : execution stopped ! */
      SFU_EXCPT_Security_Error();
    }

#ifdef SFU_WRP_PROTECT_ENABLE
    if (SFU_LL_SECU_CheckProtectionWRP(&flash_option_bytes) != SFU_SUCCESS)
    {
      LOG_DEBUG("= [SBOOT] System Security Configuration failed! Product blocked.");
      /* Security issue : execution stopped ! */
      SFU_EXCPT_Security_Error();
    }
#endif /* SFU_WRP_PROTECT_ENABLE */

#ifdef SFU_PCROP_PROTECT_ENABLE
    if (SFU_LL_SECU_CheckProtectionPCROP(&flash_option_bytes) != SFU_SUCCESS)
    {
      LOG_DEBUG("= [SBOOT] System Security Configuration failed! Product blocked.");
      /* Security issue : execution stopped ! */
      SFU_EXCPT_Security_Error();
    }
#endif /* SFU_PCROP_PROTECT_ENABLE */

#ifdef SFU_SECURE_USER_PROTECT_ENABLE
    if (SFU_LL_SECU_CheckProtectionSecUser(&flash_option_bytes) != SFU_SUCCESS)
    {
      LOG_DEBUG("= [SBOOT] System Security Configuration failed! Product blocked.");
      /* Security issue : execution stopped ! */
      SFU_EXCPT_Security_Error();
    }
#endif  /* SFU_SECURE_USER_PROTECT_ENABLE */

    /*RDP level 2 ==> Flow control by-passed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_RDP, FLOW_CTRL_RDP);
  }
  else
  {
    /* Check/Set Flash configuration *******************************************/
    e_ret_status = SFU_LL_SECU_SetFlashConfiguration(&flash_option_bytes, &is_protection_to_be_applied);

    /* Check/Apply WRP ********************************************************/
#ifdef SFU_WRP_PROTECT_ENABLE
    if (e_ret_status == SFU_SUCCESS)
    {
      e_ret_status = SFU_LL_SECU_SetProtectionWRP(&flash_option_bytes, &is_protection_to_be_applied);
    }
#endif /* SFU_WRP_PROTECT_ENABLE */

    /* Check/Apply PCRoP ******************************************************/
#ifdef SFU_PCROP_PROTECT_ENABLE
    if (e_ret_status == SFU_SUCCESS)
    {
      e_ret_status = SFU_LL_SECU_SetProtectionPCROP(&flash_option_bytes, &is_protection_to_be_applied);
    }
#endif /* SFU_PCROP_PROTECT_ENABLE */

    /* Check/Apply Secure User Memory *****************************************/
#ifdef SFU_SECURE_USER_PROTECT_ENABLE
    if (e_ret_status == SFU_SUCCESS)
    {
      e_ret_status = SFU_LL_SECU_SetProtectionSecUser(&flash_option_bytes, &is_protection_to_be_applied);
    }
#endif  /* SFU_SECURE_USER_PROTECT_ENABLE */

    /* Check/Apply RDP : RDP-L2 should be done as last option bytes configuration */
#ifdef SFU_RDP_PROTECT_ENABLE
    if (e_ret_status == SFU_SUCCESS)
    {
      e_ret_status = SFU_LL_SECU_SetProtectionRDP(&flash_option_bytes, &is_protection_to_be_applied);
    }
#endif  /* SFU_RDP_PROTECT_ENABLE */

    if (e_ret_status == SFU_SUCCESS)
    {
      if (is_protection_to_be_applied)
      {
        /* Generate System Reset to reload the new option byte values *************/
        /* WARNING: This means that if a protection can't be set, there will be a reset loop! */
        (void) HAL_FLASH_OB_Launch();
      }
    }
  }


  /* Lock the Options Bytes ***************************************************/
  (void) HAL_FLASH_OB_Lock();

  /* Lock the Flash to disable the flash control register access (recommended
  to protect the FLASH memory against possible unwanted operation) *********/
  (void) HAL_FLASH_Lock();

  /* If it was not possible to apply one of the above mandatory protections, the
  Option bytes have not been reloaded. Return the error status in order for the
  caller function to take the right actions */
  return e_ret_status;

}

/**
  * @brief  Apply Runtime security  protections.
  *         Runtime security protections have to be re-configured at each Reset.
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_CheckApplyRuntimeProtections(uint8_t uStep)
{
  SFU_ErrorStatus e_ret_status = SFU_SUCCESS;
  SFU_ProtectionTypeDef runtime_protection = SFU_PROTECTIONS_NONE;
  /* Check/Apply  IWDG **************************************************/
#ifdef SFU_IWDG_PROTECT_ENABLE
  if (SFU_LL_SECU_SetProtectionIWDG() == SFU_SUCCESS)
  {
    runtime_protection |= SFU_RUNTIME_PROTECTION_IWDG;
  }
  else
  {
    /* When a protection cannot be set then SFU_ERROR is returned */
    e_ret_status = SFU_ERROR;
  }
#endif /* SFU_IWDG_PROTECT_ENABLE */

#ifdef SFU_MPU_PROTECT_ENABLE
  /* Check/Apply MPU ************************************************/
  if (uStep == SFU_INITIAL_CONFIGURATION)
  {
    if (SFU_LL_SECU_SetProtectionMPU(uStep) == SFU_SUCCESS)
    {
      runtime_protection |= SFU_RUNTIME_PROTECTION_MPU;
    }
    else
    {
      /* When a protection cannot be set then SFU_ERROR is returned */
      e_ret_status = SFU_ERROR;
    }
  }
  else
  {
    /* Privileged mode required for MPU second-configuration
       Errors caught by FLOW_CONTROL */
    SFU_MPU_SysCall((uint32_t)SB_SYSCALL_MPU_CONFIG);
  }
#endif /* SFU_MPU_PROTECT_ENABLE */



  /* Check/Apply disable DMAs  ************************************************/
#ifdef SFU_DMA_PROTECT_ENABLE
  if (uStep == SFU_INITIAL_CONFIGURATION)
  {
    if (SFU_LL_SECU_SetProtectionDMA() == SFU_SUCCESS)
    {
      runtime_protection |= SFU_RUNTIME_PROTECTION_DMA;
    }
    else
    {
      /* When a protection cannot be set then SFU_ERROR is returned */
      e_ret_status = SFU_ERROR;
    }
  }
  else
  {
    /* Privileged mode required for DMA second-configuration in order to activate the clock through RCC
       Errors caught by FLOW_CONTROL */
    SFU_MPU_SysCall((uint32_t)SB_SYSCALL_DMA_CONFIG);
  }
#endif /* SFU_DMA_PROTECT_ENABLE */


  /* Check/Apply  DAP *********************************************************/
#ifdef SFU_DAP_PROTECT_ENABLE
  if (uStep == SFU_INITIAL_CONFIGURATION)
  {
    if (SFU_LL_SECU_SetProtectionDAP() == SFU_SUCCESS)
    {
      runtime_protection |= SFU_RUNTIME_PROTECTION_DAP;
    }
    else
    {
      /* When a protection cannot be set then SFU_ERROR is returned */
      e_ret_status = SFU_ERROR;
    }
  }
  else
  {
    /* Privileged mode required for DMA second-configuration in order to activate the clock through RCC
       Errors caught by FLOW_CONTROL */
    SFU_MPU_SysCall((uint32_t)SB_SYSCALL_DAP_CONFIG);
  }
#else
#endif /* SFU_DAP_PROTECT_ENABLE */

  /* Check/Apply  ANTI_TAMPER *************************************************/
#ifdef SFU_TAMPER_PROTECT_ENABLE
  if (uStep == SFU_INITIAL_CONFIGURATION)
  {
    if (SFU_LL_SECU_SetProtectionANTI_TAMPER() == SFU_SUCCESS)
    {
      runtime_protection |= SFU_RUNTIME_PROTECTION_ANTI_TAMPER;
    }
    else
    {
      /* When a protection cannot be set then SFU_ERROR is returned */
      e_ret_status = SFU_ERROR;
    }
  }
  else
  {
    /* Privileged mode required for TAMPER second-configuration in order to activate the clock through RCC
       Errors caught by FLOW_CONTROL */
    SFU_MPU_SysCall((uint32_t)SB_SYSCALL_TAMPER_CONFIG);
  }
#else
#ifdef SFU_TEST_PROTECTION
  if (SFU_LL_RTC_Init() != SFU_SUCCESS)
  {
    e_ret_status = SFU_ERROR;
  }
#endif /* SFU_TEST_PROTECTION */
#endif /* SFU_TAMPER_PROTECT_ENABLE */
  /* Check/Apply  CLOCK_MONITOR **********************************************/
#ifdef SFU_CLCK_MNTR_PROTECT_ENABLE
  if (SFU_LL_SECU_SetProtectionCLOCK_MONITOR() == SFU_SUCCESS)
  {
    runtime_protection |= SFU_RUNTIME_PROTECTION_CLOCK_MONITOR;
  }
  else
  {
    /* When a protection cannot be set then SFU_ERROR is returned */
    e_ret_status = SFU_ERROR;
  }
#endif /* SFU_CLCK_MNTR_PROTECT_ENABLE */

  /* Check/Apply  TEMP_MONITOR **********************************************/
#ifdef SFU_TEMP_MNTR_PROTECT_ENABLE
  if (SFU_LL_SECU_SetProtectionTEMP_MONITOR() == SFU_SUCCESS)
  {
    runtime_protection |= SFU_RUNTIME_PROTECTION_TEMP_MONITOR;
  }
  else
  {
    /* When a protection cannot be set then SFU_ERROR is returned */
    e_ret_status = SFU_ERROR;
  }
#endif /* SFU_TEMP_MNTR_PROTECT_ENABLE */

#if defined(SFU_VERBOSE_DEBUG_MODE)
  LOG_DEBUG("= [SBOOT] RuntimeProtections: %x", runtime_protection);
#endif /* SFU_VERBOSE_DEBUG_MODE */

  return e_ret_status;
}

/**
  * @brief  Return the reset source  detected after a reboot. The related flag is reset
  *         at the end of this function.
  * @param  peResetpSourceId: to be filled with the detected source of reset
  * @note   In case of multiple reset sources this function return only one of them.
  *         It can be improved returning and managing a combination of them.
  * @retval SFU_SUCCESS if successful, SFU_ERROR otherwise
  */
void SFU_LL_SECU_GetResetSources(SFU_RESET_IdTypeDef *peResetpSourceId)
{
  /* Check if the last reset has been generated from a Watchdog exception */
  if ((__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ||
      (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET))
  {
    *peResetpSourceId = SFU_RESET_WDG_RESET;

  }


  /* Check if the last reset has been generated from a Low Power reset */
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
  {
    *peResetpSourceId = SFU_RESET_LOW_POWER;

  }

  /* Check if the last reset has been generated from a Software reset  */
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
  {
    *peResetpSourceId = SFU_RESET_SW_RESET;

  }
  /* Check if the last reset has been generated from an Option Byte Loader reset  */
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST) != RESET)
  {
    *peResetpSourceId = SFU_RESET_OB_LOADER;
  }
  /* Check if the last reset has been generated from a Hw pin reset  */
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
  {
    *peResetpSourceId = SFU_RESET_HW_RESET;

  }
  /* Unknown */
  else
  {
    *peResetpSourceId = SFU_RESET_UNKNOWN;
  }
}

/**
  * @brief  Clear the reset sources. This function should be called after the actions
  *         on the reset sources has been already taken.
  * @param  none
  * @note   none
  * @retval none
  */
void SFU_LL_SECU_ClearResetSources()
{
  /* Clear reset flags  */
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

/**
  * @brief  Refresh Watchdog : reload counter
  *         This function must be called just before jumping to the UserFirmware
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_IWDG_Refresh(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

#ifdef SFU_IWDG_PROTECT_ENABLE
  {
    /* Refresh IWDG: reload counter */
    if (HAL_IWDG_Refresh(&IwdgHandle) == HAL_OK)
    {
      e_ret_status = SFU_SUCCESS;
    }
  }
#else
  e_ret_status = SFU_SUCCESS;
#endif /*SFU_IWDG_PROTECT_ENABLE*/

  return e_ret_status;
}

/**
  * @brief  Check Flash configuration.
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure
  * @retval SFU_ErrorStatus SFU_SUCCESS if Flash configuration is correct, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_CheckFlashConfiguration(FLASH_OBProgramInitTypeDef *psFlashOptionBytes)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check Single bank mode bit **********************************************/
  if ((psFlashOptionBytes->USERConfig & OB_DBANK_64_BITS) != OB_DBANK_64_BITS)
  {
    e_ret_status = SFU_SUCCESS;
  }
  if (e_ret_status == SFU_SUCCESS)
  {
    /* Execution stopped if flow control failed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_UBE, FLOW_CTRL_UBE);
  }
  return e_ret_status;

}

/**
  * @brief  Set Flash configuration.
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure
  * @param  pbIsProtectionToBeApplied: Output parameter to be set as "TRUE" if
  *         this OptByte has to be modified and immediately reloaded.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetFlashConfiguration(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                  SFU_BoolTypeDef *pbIsProtectionToBeApplied)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check Flash configuration */
  if (SFU_LL_SECU_CheckFlashConfiguration(psFlashOptionBytes) == SFU_SUCCESS)
  {
    e_ret_status = SFU_SUCCESS;
  }
  else
  {
    /* Single Bank mode cannot be activated on the FLY : information only */
    LOG_DEBUG("= [SBOOT] Flash Configuration KO: Dual bank mode activated. STOP!");
    /* Security issue : execution stopped ! */
    SFU_EXCPT_Security_Error();
  }

  return e_ret_status;
}

#ifdef SFU_RDP_PROTECT_ENABLE
/**
  * @brief  Apply the RDP protection
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure
  * @param  pbIsProtectionToBeApplied: Output parameter to be set as "TRUE" if
  *         this OptByte has to be modified and immediately reloaded.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionRDP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                             SFU_BoolTypeDef *pbIsProtectionToBeApplied)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check/Apply RDP **********************************************************/
  /* Please consider that the suggested and most secure approach is to set the RDP_LEVEL_2 */
  if (psFlashOptionBytes->RDPLevel == SFU_PROTECT_RDP_LEVEL)
  {
    e_ret_status = SFU_SUCCESS; /*Protection already applied */
    /* Execution stopped if flow control failed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_RDP, FLOW_CTRL_RDP);
  }
  else
  {
#if defined(SECBOOT_OB_DEV_MODE)
#if defined(SFU_FINAL_SECURE_LOCK_ENABLE)
    LOG_DEBUG("\t  Applying RDP-2 Level. Product locked! You might need to unplug/plug the USB cable!");
#else
    LOG_DEBUG("\t  Applying RDP-1 Level. You might need to unplug/plug the USB cable!");
#endif /* SFU_FINAL_SECURE_LOCK_ENABLE */
    psFlashOptionBytes->OptionType      = OPTIONBYTE_RDP;
    psFlashOptionBytes->RDPLevel        = SFU_PROTECT_RDP_LEVEL;
    if (HAL_FLASHEx_OBProgram(psFlashOptionBytes) == HAL_OK)
    {
      *pbIsProtectionToBeApplied |= 1U;
      e_ret_status = SFU_SUCCESS;
      /* Execution stopped if flow control failed */
      FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_RDP, FLOW_CTRL_RDP);
    }
#else
    LOG_DEBUG("= [SBOOT] System Security Configuration failed: RDP is incorrect. STOP!");
    /* Security issue : execution stopped ! */
    SFU_EXCPT_Security_Error();
#endif /* SECBOOT_OB_DEV_MODE */
  }
  return e_ret_status;
}
#endif /*SFU_RDP_PROTECT_ENABLE*/

#ifdef SFU_WRP_PROTECT_ENABLE
/**
  * @brief  Check the WRP protection to the specified Area. It includes the SFU Vector Table
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_CheckProtectionWRP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check WRP ****************************************************************/
  if ((psFlashOptionBytes->WRPStartOffset == SFU_PROTECT_WRP_PAGE_START_1) &&
      (psFlashOptionBytes->WRPEndOffset   == SFU_PROTECT_WRP_PAGE_END_1))
  {
    e_ret_status = SFU_SUCCESS; /*Protection applied */
  }
  if (e_ret_status == SFU_SUCCESS)
  {
    /* Execution stopped if flow control failed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_WRP, FLOW_CTRL_WRP);
  }
  return e_ret_status;
}

/**
  * @brief  Apply the WRP protection to the specified Area. It includes the SFU Vector Table
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure
  * @param  pbIsProtectionToBeApplied: Output parameter to be set as "TRUE" if
  *         this OptByte has to be modified and immediately reloaded.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionWRP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                             SFU_BoolTypeDef *pbIsProtectionToBeApplied)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check/Apply WRP **********************************************************/
  if (SFU_LL_SECU_CheckProtectionWRP(psFlashOptionBytes) == SFU_SUCCESS)
  {
    e_ret_status = SFU_SUCCESS; /*Protection already applied */
  }
  else
  {
#if defined(SECBOOT_OB_DEV_MODE)
    psFlashOptionBytes->OptionType     = OPTIONBYTE_WRP;
    psFlashOptionBytes->WRPArea        = SFU_PROTECT_WRP_AREA_1;
    psFlashOptionBytes->WRPStartOffset = SFU_PROTECT_WRP_PAGE_START_1;
    psFlashOptionBytes->WRPEndOffset   = SFU_PROTECT_WRP_PAGE_END_1;

    if (HAL_FLASHEx_OBProgram(psFlashOptionBytes) == HAL_OK)
    {
      *pbIsProtectionToBeApplied |= 1U;
      e_ret_status = SFU_SUCCESS;
      /* Execution stopped if flow control failed */
      FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_WRP, FLOW_CTRL_WRP);
    }
#else
    LOG_DEBUG("= [SBOOT] System Security Configuration failed: WRP is incorrect. STOP!");
    /* Security issue : execution stopped ! */
    SFU_EXCPT_Security_Error();
#endif /* SECBOOT_OB_DEV_MODE */
  }

  return e_ret_status;
}
#endif /*SFU_WRP_PROTECT_ENABLE*/



#ifdef SFU_MPU_PROTECT_ENABLE

/**
  * @brief  Apply MPU protection
  * @param  uStep Configuration step : SFU_INITIAL_CONFIGURATION, SFU_SECOND_CONFIGURATION, SFU_THIRD_CONFIGURATION
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionMPU(uint8_t uStep)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
  uint8_t mpu_region_num; /* id of the MPU region being configured */
  MPU_Region_InitTypeDef MPU_InitStruct;


  for (mpu_region_num = 0U; mpu_region_num < (sizeof(MpuAreas) / sizeof(SFU_MPU_InitTypeDef)); mpu_region_num++)
  {
    MPU_InitStruct.Enable               = MPU_REGION_ENABLE;
    MPU_InitStruct.Number               = MpuAreas[mpu_region_num].Number;
    MPU_InitStruct.BaseAddress          = MpuAreas[mpu_region_num].BaseAddress;
    MPU_InitStruct.Size                 = MpuAreas[mpu_region_num].Size;
    MPU_InitStruct.SubRegionDisable     = MpuAreas[mpu_region_num].SubRegionDisable;
    MPU_InitStruct.AccessPermission     = MpuAreas[mpu_region_num].AccessPermission;
    MPU_InitStruct.DisableExec          = MpuAreas[mpu_region_num].DisableExec;
    MPU_InitStruct.IsShareable          = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsBufferable         = MpuAreas[mpu_region_num].Bufferable;
    MPU_InitStruct.IsCacheable          = MpuAreas[mpu_region_num].Cacheable;
    MPU_InitStruct.TypeExtField         = MpuAreas[mpu_region_num].Tex;
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE(" @%d:%x size:%x sub:%x perm:%x exec:%x\r\n",
          MPU_InitStruct.Number, MPU_InitStruct.BaseAddress, MPU_InitStruct.Size, MPU_InitStruct.SubRegionDisable,
          MPU_InitStruct.AccessPermission, MPU_InitStruct.DisableExec);
#endif /* SFU_VERBOSE_DEBUG_MODE */

    if (uStep == SFU_INITIAL_CONFIGURATION)
    {
      HAL_MPU_ConfigRegion(&MPU_InitStruct);
    }
    else
    {
      if (MPU_CheckConfig(&MPU_InitStruct) == SFU_ERROR)
      {
        return SFU_ERROR;
      }
    }
  }

  if (uStep == SFU_INITIAL_CONFIGURATION)
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    LOG_DEBUG("");
#endif /* SFU_VERBOSE_DEBUG_MODE */
    /* Enables the MPU */
    HAL_MPU_Enable(MPU_HARDFAULT_NMI);

#if defined(SCB_SHCSR_MEMFAULTENA_Msk)
    /* Enables memory fault exception */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif /* SCB_SHCSR_MEMFAULTENA_Msk */
  }
  else
  {
    if (MPU->CTRL != (MPU_HARDFAULT_NMI | MPU_CTRL_ENABLE_Msk))
    {
      return SFU_ERROR;
    }
#if defined(SCB_SHCSR_MEMFAULTENA_Msk)
    if ((SCB->SHCSR & SCB_SHCSR_MEMFAULTENA_Msk) != SCB_SHCSR_MEMFAULTENA_Msk)
    {
      return SFU_ERROR;
    }
#endif /* SCB_SHCSR_MEMFAULTENA_Msk */
  }

  e_ret_status = SFU_SUCCESS;

  if (e_ret_status == SFU_SUCCESS)
  {
    /* Execution stopped if flow control failed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_MPU, FLOW_CTRL_MPU);
  }
  return e_ret_status;
}
#endif /*SFU_MPU_PROTECT_ENABLE*/

#ifdef SFU_MPU_PROTECT_ENABLE
/**
  * @brief  Apply MPU protection to activate through RAM execution secure user memory
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionMPU_SecUser(uint8_t Exec_Property)
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  /*  modify executable region 6 to allow execution for secure user memory activation */
  MPU_InitStruct.Enable               = MPU_REGION_ENABLE;
  MPU_InitStruct.Number               = SFU_PROTECT_MPU_SRAM_HDP_RGNV;
  MPU_InitStruct.BaseAddress          = SFU_PROTECT_MPU_SRAM_HDP_START;
  MPU_InitStruct.Size                 = SFU_PROTECT_MPU_SRAM_HDP_SIZE;
  MPU_InitStruct.SubRegionDisable     = SFU_PROTECT_MPU_SRAM_HDP_SREG;
  MPU_InitStruct.AccessPermission     = SFU_PROTECT_MPU_SRAM_HDP_PERM;
  MPU_InitStruct.DisableExec          = Exec_Property;
  MPU_InitStruct.IsShareable          = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsBufferable         = SFU_PROTECT_MPU_SRAM_HDP_B ;
  MPU_InitStruct.IsCacheable          = SFU_PROTECT_MPU_SRAM_HDP_C;
  MPU_InitStruct.TypeExtField         = SFU_PROTECT_MPU_SRAM_HDP_TEXV;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  return SFU_SUCCESS;
}
#endif /* SFU_MPU_PROTECT_ENABLE */


#ifdef SFU_DMA_PROTECT_ENABLE
/**
  * @brief  Apply DMA protection
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionDMA(void)
{
  /*
   * In this function we disable the DMA buses in order to avoid that while the SB/SFU is running
   * some DMA has been already enabled (e.g. through debugger in RDP-1 after reset) in order to access sensitive
   * information in SRAM, FLASH
   */
  /* Disable  DMA1, DMA2 */
  __HAL_RCC_DMA1_CLK_DISABLE();

  __HAL_RCC_DMA2_CLK_DISABLE();

  __HAL_RCC_DMAMUX1_CLK_DISABLE();




  /* Execution stopped if flow control failed */
  FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_DMA, FLOW_CTRL_DMA);
  return SFU_SUCCESS;
}
#endif /*SFU_DMA_PROTECT_ENABLE*/

#ifdef SFU_IWDG_PROTECT_ENABLE
/**
  *  @brief Apply IWDG protection
  *         The IWDG timeout is set to 4 second.
  *         Then, the IWDG reload counter is configured as below to obtain 4 second according
  *         to the measured LSI frequency after setting the prescaler value:
  *         IWDG counter clock Frequency = LSI Frequency / Prescaler value
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionIWDG(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* TIMER could be used to get the LSI frequency in order to have a more precise IWDG.
  This is not used in this implementation because not necessary and in order
  to optimize code-size. If you are interested, please have a look at the IWDG Cube example. */

  /* Configure & Start the IWDG peripheral */
  /* Set counter reload value to obtain 6 sec. IWDG TimeOut.
  IWDG counter clock Frequency = uwLsiFreq
  Set Prescaler to 64 (IWDG_PRESCALER_64)
  Timeout Period = (Reload Counter Value * 64) / uwLsiFreq
  So Set Reload Counter Value = (6 * uwLsiFreq) / 64 */
  IwdgHandle.Instance = IWDG;
  IwdgHandle.Init.Prescaler = IWDG_PRESCALER_64;
  IwdgHandle.Init.Reload = (SFU_IWDG_TIMEOUT * LSI_VALUE / 64U);
  IwdgHandle.Init.Window = IWDG_WINDOW_DISABLE;

  {
    if (HAL_IWDG_Init(&IwdgHandle) == HAL_OK)
    {
      e_ret_status = SFU_SUCCESS;
      /* Execution stopped if flow control failed */
      FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_IWDG, FLOW_CTRL_IWDG);
    }
  }

  return e_ret_status;
}
#endif /*SFU_IWDG_PROTECT_ENABLE*/

#ifdef SFU_DAP_PROTECT_ENABLE
/**
  * @brief  Set DAP protection status, configuring SWCLK and SWDIO GPIO pins.
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionDAP(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  GPIO_InitTypeDef GPIO_InitStruct;

  /* Enable clock of DBG GPIO port */
  SFU_DBG_CLK_ENABLE();

  /* Enable the DAP protections, so disable the DAP re-configuring SWCLK and SWDIO GPIO pins */
  GPIO_InitStruct.Pin = SFU_DBG_SWDIO_PIN | SFU_DBG_SWCLK_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SFU_DBG_PORT, &GPIO_InitStruct);
  e_ret_status = SFU_SUCCESS;
  /* Execution stopped if flow control failed */
  FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_DAP, FLOW_CTRL_DAP);

  return e_ret_status;
}
#else
#endif /*SFU_DAP_PROTECT_ENABLE*/

#ifdef SFU_TAMPER_PROTECT_ENABLE
/**
  * @brief  Apply ANTI TAMPER protection
  * @param None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionANTI_TAMPER(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
  RTC_TamperTypeDef  stamperstructure;

  /* RTC_TAMPER_2 (PA0) selected. PC13 connected to RTC_TAMPER_1 is also connected to the USER button */
  TAMPER_GPIO_CLK_ENABLE();

  /* Configure Tamper Pin */
  /* tamper is an additional function */
  /* not an alternate Function : config not needed */
  /* Configure the RTC peripheral */
  /* Configure RTC prescaler and RTC data registers */
  /* RTC configured as follows:
  - Hour Format    = Format 24
  - Asynch Prediv  = Value according to source clock
  - Synch Prediv   = Value according to source clock
  - OutPut         = Output Disable
  - OutPutPolarity = High Polarity
  - OutPutType     = Open Drain */
  RtcHandle.Instance            = RTC;
  RtcHandle.Init.HourFormat     = RTC_HOURFORMAT_24;
  RtcHandle.Init.AsynchPrediv   = RTC_ASYNCH_PREDIV;
  RtcHandle.Init.SynchPrediv    = RTC_SYNCH_PREDIV;
  RtcHandle.Init.OutPut         = RTC_OUTPUT_DISABLE;
  RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  RtcHandle.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;

  if (HAL_RTC_Init(&RtcHandle) == HAL_OK)
  {
    /* Configure RTC Tamper */
    stamperstructure.Tamper                       = RTC_TAMPER_ID;
    stamperstructure.Trigger                      = RTC_TAMPERTRIGGER_FALLINGEDGE;
    stamperstructure.Filter                       = RTC_TAMPERFILTER_DISABLE;
    stamperstructure.SamplingFrequency            = RTC_TAMPERSAMPLINGFREQ_RTCCLK_DIV32768;
    stamperstructure.PrechargeDuration            = RTC_TAMPERPRECHARGEDURATION_1RTCCLK;
    stamperstructure.TamperPullUp                 = RTC_TAMPER_PULLUP_ENABLE;
    stamperstructure.TimeStampOnTamperDetection   = RTC_TIMESTAMPONTAMPERDETECTION_DISABLE;
    stamperstructure.NoErase                      = RTC_TAMPER_ERASE_BACKUP_ENABLE;
    stamperstructure.MaskFlag                     = RTC_TAMPERMASK_FLAG_DISABLE;

    if (HAL_RTCEx_SetTamper_IT(&RtcHandle, &stamperstructure) == HAL_OK)
    {

      /* Clear the Tamper interrupt pending bit */
      __HAL_RTC_TAMPER_CLEAR_FLAG(&RtcHandle, RTC_FLAG_TAMP_2);
      e_ret_status = SFU_SUCCESS;
      /* Execution stopped if flow control failed */
      FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_TAMPER, FLOW_CTRL_TAMPER);
    }
  }

  return e_ret_status;
}
#endif /*SFU_TAMPER_PROTECT_ENABLE*/

#ifdef SFU_CLCK_MNTR_PROTECT_ENABLE
/**
  * @brief  Apply CLOCK MONITOR protection
  * @note   This function has been added just as template to be used/customized
  *         if a clock monitor is requested.
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionCLOCK_MONITOR(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Apply the Clock Monitoring */
  /* Add your code here for customization
     e.g. if HSE or LSE is used enable the CSS!
   ...
   ...
  */

  e_ret_status = SFU_SUCCESS;

  return e_ret_status;
}
#endif /*SFU_CLCK_MNTR_PROTECT_ENABLE*/

#ifdef SFU_TEMP_MNTR_PROTECT_ENABLE
/**
  * @brief  Apply TEMP MONITOR protection
  * @note   This function has been added just as template to be used/customized
  *         if a temperature monitor is requested.
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionTEMP_MONITOR(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Apply the Temperature Monitoring */
  /* Add your code here for customization
     ...
     ...
  */

  e_ret_status = SFU_SUCCESS;

  return e_ret_status;
}
#endif /*SFU_TEMP_MNTR_PROTECT_ENABLE*/

#ifdef SFU_PCROP_PROTECT_ENABLE
/**
  * @brief  Check PCROP protection.
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_CheckProtectionPCROP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check/Apply PCRoP ********************************************************/
  /* Check if area is already included in a PCROP region */
  if ((psFlashOptionBytes->PCROPStartAddr <= SFU_PROTECT_PCROP_ADDR_START)
      && (psFlashOptionBytes->PCROPEndAddr >= (SFU_PROTECT_PCROP_ADDR_END - 16U)))
  {
    if ((psFlashOptionBytes->PCROPConfig & OB_PCROP_RDP_ERASE) == OB_PCROP_RDP_ERASE)
    {
      e_ret_status = SFU_SUCCESS;
    }
  }

  if (e_ret_status == SFU_SUCCESS)
  {
    /* Execution stopped if flow control failed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_PCROP, FLOW_CTRL_PCROP);
  }
  return e_ret_status;
}

/**
  * @brief  Apply PCROP protection.
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure.
  * @param  pbIsProtectionToBeApplied: Output parameter to be set as "TRUE" if
  *         this OptByte has to be modified and immediately reloaded.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionPCROP(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                               SFU_BoolTypeDef *pbIsProtectionToBeApplied)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check/Apply PCRoP ********************************************************/

  /* Check if area is already included in a PCROP region */
  if (SFU_LL_SECU_CheckProtectionPCROP(psFlashOptionBytes) == SFU_SUCCESS)
  {
    e_ret_status = SFU_SUCCESS;
  }
  else
  {
#if defined(SECBOOT_OB_DEV_MODE)
    /* Update OB for PCROP */
    psFlashOptionBytes->OptionType = OPTIONBYTE_PCROP;

    psFlashOptionBytes->PCROPConfig = SFU_PROTECT_PCROP_AREA + OB_PCROP_RDP_ERASE; /* Bank1 + erase PCROP when doing a
                                                                                      RDP-level regression (1->0) */
    psFlashOptionBytes->PCROPStartAddr = SFU_PROTECT_PCROP_ADDR_START;
    psFlashOptionBytes->PCROPEndAddr = SFU_PROTECT_PCROP_ADDR_END;

    if (HAL_FLASHEx_OBProgram(psFlashOptionBytes) == HAL_OK)
    {
      *pbIsProtectionToBeApplied |= 1U;
      e_ret_status = SFU_SUCCESS;
      /* Execution stopped if flow control failed */
      FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_PCROP, FLOW_CTRL_PCROP);
    }
#else
    LOG_DEBUG("= [SBOOT] System Security Configuration failed: incorrect PCROP. STOP!");
    /* Security issue : execution stopped ! */
    SFU_EXCPT_Security_Error();
#endif /* SECBOOT_OB_DEV_MODE */
  }

  return e_ret_status;
}
#endif /*SFU_PCROP_PROTECT_ENABLE*/

#ifdef  SFU_SECURE_USER_PROTECT_ENABLE
/**
  * @brief  Check Secure User Memory protection configuration.
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_CheckProtectionSecUser(FLASH_OBProgramInitTypeDef *psFlashOptionBytes)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Test if Secure Memory size is correctly programmed and unique boot entry activated.
   */
  if ((psFlashOptionBytes->SecSize == SFU_NB_PAGE_SEC_USER_MEM)
      && (psFlashOptionBytes->BootEntryPoint == OB_BOOT_LOCK_ENABLE))
  {
    /*
     * In this example secure area covers:
     * - the SB_SFU code
     * - the header of the active slots
     */
#if defined(SFU_VERBOSE_DEBUG_MODE)
    LOG_DEBUG("= [SBOOT] Secure User memory Area settings: [%x]", psFlashOptionBytes->SecSize);
#endif /* SFU_VERBOSE_DEBUG_MODE */
    e_ret_status = SFU_SUCCESS;
    /* Execution stopped if flow control failed */
    FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_SEC_MEM, FLOW_CTRL_SEC_MEM);
  }
  else
  {
    /* else the settings are incorrect: ERROR */
    LOG_DEBUG("= [SBOOT] Incorrect Secure User memory Area settings: [%x]", psFlashOptionBytes->SecSize);
  }

  return e_ret_status;
}


/**
  * @brief  Apply Secure User Memory protection.
  * @param  psFlashOptionBytes: pointer to the Option Bytes structure.
  * @param  pbIsProtectionToBeApplied: Output parameter to be set as "TRUE" if
  *         this OptByte has to be modified and immediately reloaded.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
SFU_ErrorStatus SFU_LL_SECU_SetProtectionSecUser(FLASH_OBProgramInitTypeDef *psFlashOptionBytes,
                                                 SFU_BoolTypeDef *pbIsProtectionToBeApplied)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Check if Secure User Memory is already configured */
  if (SFU_LL_SECU_CheckProtectionSecUser(psFlashOptionBytes) == SFU_SUCCESS)
  {
    e_ret_status = SFU_SUCCESS;
  }
  else
  {
    /* Apply the proper configuration if we are in development mode */
#if defined(SECBOOT_OB_DEV_MODE)
    /* Set the security bit if needed */
    /* ------------------------------ */
    psFlashOptionBytes->OptionType = OPTIONBYTE_SEC | OPTIONBYTE_BOOT_LOCK;
    psFlashOptionBytes->BootEntryPoint = OB_BOOT_LOCK_ENABLE;
    psFlashOptionBytes->SecBank = FLASH_BANK_1;
    psFlashOptionBytes->SecSize = SFU_NB_PAGE_SEC_USER_MEM;
    if (HAL_FLASHEx_OBProgram(psFlashOptionBytes) == HAL_OK)
    {
      *pbIsProtectionToBeApplied |= 1U;
      e_ret_status = SFU_SUCCESS;
      /* Execution stopped if flow control failed */
      FLOW_CONTROL_STEP(uFlowProtectValue, FLOW_STEP_SEC_MEM, FLOW_CTRL_SEC_MEM);
    }
#else
    LOG_DEBUG("= [SBOOT] System Security Configuration failed: Secure User Memory is incorrect. STOP!");
    /* Security issue : execution stopped ! */
    SFU_EXCPT_Security_Error();
#endif /* SECBOOT_OB_DEV_MODE */
  }

  return e_ret_status;
}

#endif /* SFU_SECURE_USER_PROTECT_ENABLE */

/**
  * @brief  Activate Secure User Memory protection and jump into user application.
  * @param  Address of the code to jump into
  * @retval none.
  */

/* Place code in a specific section and remove specific warning */
#if defined(__ICCARM__)
#pragma diag_suppress=TA022
#pragma diag_suppress=TA023
#elif defined(__CC_ARM)
#pragma arm section code = ".SB_HDP_Code"
#elif defined(__ARMCC_VERSION)
__attribute__((section(".SB_HDP_Code")))
#endif /* __ICCARM__ */

__RAM_FUNC void SFU_LL_SECU_ActivateSecUser(uint32_t Address)
{
  /* Unlock the Flash to enable the flash control register access */
  (void) HAL_FLASH_Unlock();

  /* Avoid systick interruption during UserApp start-up
     systick will be re-initialized by HAL_Init() during execution of main() from UserApp */
  HAL_SuspendTick();

#if defined(SFU_SECURE_USER_PROTECT_ENABLE)
  /* Ensure all actions are completed before activating secure user memory */
  __ISB();
  /* Secure coding  : volatile variable usage to force compiler to reload FLASH->CR register address */
  __IO uint32_t read_reg = (uint32_t) &FLASH->CR;

  do
  {
    /* Activate secure user memory */
    FLASH->CR |= FLASH_CR_SEC_PROT1;
  } while (((* (uint32_t *)read_reg) & FLASH_CR_SEC_PROT1) != FLASH_CR_SEC_PROT1);

  if (((* (uint32_t *)read_reg) & FLASH_CR_SEC_PROT1) != FLASH_CR_SEC_PROT1)
  {
    /* Security issue : execution stopped ! */
    NVIC_SystemReset();
  }
  else
  {
    __DSB();
#else
  if (1 == 1)
  {
#endif /* (SFU_SECURE_USER_PROTECT_ENABLE) */
#if defined(SFU_MPU_PROTECT_ENABLE)
    /* Make sure outstanding transfers are done */
    __DMB();

    /* Disable the MPU and clear the control register*/
    MPU->CTRL  = 0U;

#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION)
    /* Disable the UART. This may be done only after the MPU is disabled */
    /* Use of macro that manipulate register instead of function call since secure user memory is activated */
    SFU_UART_CLK_DISABLE();
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION) */
#endif /* SFU_MPU_PROTECT_ENABLE */

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    /* Reset FPU context */
    SCB->CPACR &= ~((3UL << 10*2)|(3UL << 11*2));  /* reset CP10 and CP11 Full Access */
    FPU->FPCCR &= ~FPU_FPCCR_LSPEN_Msk; /* Disable automatic lazy state preservation for floating-point context */
    FPU->FPCCR &= ~FPU_FPCCR_LSPACT_Msk; /* Clear the lazy state preservation for floating-point context */
#endif /* (__FPU_PRESENT == 1) && (__FPU_USED == 1) */

    /* clear process stack & unprivileged bit */
    __set_CONTROL(__get_CONTROL() & ~0x3U);

    /* returns from interrupt into application */
    launch_application(Address, (uint32_t)jump_to_function);
  }

  /*we should never reach this point */
  NVIC_SystemReset();
}

#if defined(__CC_ARM)
#pragma arm section code
#endif /* __CC_ARM */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
