/**
  ******************************************************************************
  * @file    sfu_mpu_isolation.c
  * @author  MCD Application Team
  * @brief   SFU MPU isolation primitives
  *          This file provides functions to manage the MPU isolation of the Secure Engine.
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
#include "sfu_mpu_isolation.h"
#include "sfu_low_level_security.h"
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#include "mapping_sbsfu.h"
#elif defined (__ICCARM__) || defined(__GNUC__)
#include "mapping_export.h"
#endif /* __CC_ARM || __ARMCC_VERSION */

/* Private function prototypes -----------------------------------------------*/
static void SFU_SE_STACK_Clean(void);
static void SFU_SecUserActivationInRam(uint32_t Address);

/* Functions Definition ------------------------------------------------------*/
/**
  * @brief Clean SE Stack.
  *
  */
static void SFU_SE_STACK_Clean(void)
{
  uint32_t sp_base = SE_REGION_RAM_START;
  uint32_t sp_top = SE_REGION_RAM_STACK_TOP;
  uint32_t sp = __get_MSP();
  if ((sp > sp_base) && (sp <= sp_top))
  {
    while (sp_base < sp)
    {
      *(__IO uint32_t *)sp_base = 0U;
      sp_base += 4U;
    }
  }
  else
  {
    /*  error trigger a reset */
    HAL_NVIC_SystemReset();
  }
}

/**
  * @brief This function copy secure user memory activation code from Flash to RAM
  *        then starts  its execution.
  * @note  As soon as the secure user memory is activated this secured area is no longer accessible
  *        Thus, activation should be performed from outside this area
  * @param Address of the code to jump into
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static void SFU_SecUserActivationInRam(uint32_t Address)
{
  uint32_t *psrc;
  uint32_t *psrc_end;
  uint32_t *pdest;
  uint32_t result = 0U;

  /* Copy code from Flash to RAM */
  psrc = (uint32_t *)SB_HDP_REGION_ROM_START;
  psrc_end = (uint32_t *)SB_HDP_REGION_ROM_END;
  pdest = (uint32_t *)SB_HDP_CODE_REGION_RAM_START;
  while (psrc < psrc_end)
  {
    *pdest = *psrc;
    pdest++;
    psrc++;
  }

#if defined(SFU_MPU_PROTECT_ENABLE)
  /* Change MPU configuration to set read only property to this RAM area */
  if (SFU_LL_SECU_SetProtectionMPU_SecUser(MPU_INSTRUCTION_ACCESS_DISABLE) != SFU_SUCCESS)
  {
    HAL_NVIC_SystemReset();
  }
#endif /* (SFU_MPU_PROTECT_ENABLE) */

  /* Verify thecopy code from Flash to RAM */
  psrc = (uint32_t *)(SB_HDP_REGION_ROM_START);
  psrc_end = (uint32_t *)SB_HDP_REGION_ROM_END;
  pdest = (uint32_t *)SB_HDP_CODE_REGION_RAM_START;
  while (psrc < psrc_end)
  {
    result |= (*pdest ^ *psrc);
    pdest++;
    psrc++;
  }

  if (result == 0U)
  {
#if defined(SFU_MPU_PROTECT_ENABLE)
    /* Change MPU configuration to set execution property to this RAM area */
    if (SFU_LL_SECU_SetProtectionMPU_SecUser(MPU_INSTRUCTION_ACCESS_ENABLE) != SFU_SUCCESS)
    {
      HAL_NVIC_SystemReset();
    }
#endif /* (SFU_MPU_PROTECT_ENABLE) */

    /* Execute code in RAM to activate secure user memory */
    SFU_LL_SECU_ActivateSecUser(Address);
  }
  else
  {
    HAL_NVIC_SystemReset();
  }
}


/**
  * @brief This is the Supervisor calls handler.
  * @param args SVC arguments
  * @retval void
  * @note Installed in startup_stm32xxxx.s
  *
  * This handler handles 2 requests:
  * \li Secure Engine SVC: run a Secure Engine privileged operation provided as a parameter
  * \li (see @ref SE_FunctionIDTypeDef)
  * \li Internal SB_SFU SVC: run a SB_SFU privileged operation provided as a parameter (see @ref SFU_MPU_PrivilegedOpId)
  */
void MPU_SVC_Handler(uint32_t *args)
{
  /* in PSP (args) contains caller context as follow *
    * args[0] :R0
    * args[1] :R1
    * args[2] :R2
    * args[3] :R3
    * args[4] :R12
    * args[5] :LR
    * args[6] :PC to return after exception
    * args[7] :xPSR
  */
  /*  read code OP of instruction that generate SVC interrupt
   *  bottom 8-bits of the SVC instruction code OP are SVC value  */
  uint8_t code = ((uint8_t *)args[6])[-2];

  switch (code)
  {
    case 0:
      /* A Secure Engine service is called */
      SE_SVC_Handler(args);
      break;
    case 1:
      /* Internal SB_SFU privileged service */
      SFU_MPU_SVC_Handler(args);
      break;
    default:
      /* Force a reset */
      HAL_NVIC_SystemReset();
      break;
  }
}

/**
  * @brief This function triggers a SB_SFU Privileged Operation requested with SB_SysCall
  * @param args arguments
  *             The first argument is the identifier of the requested operation.
  * @retval void
  */
void SFU_MPU_SVC_Handler(uint32_t *args)
{
  switch (args[0])
  {
    case SB_SYSCALL_LAUNCH_APP:
      /* Clean SE STACK RAM */
      SFU_SE_STACK_Clean();
#if defined(SFU_MPU_PROTECT_ENABLE)
#endif /* SFU_MPU_PROTECT_ENABLE */
      SFU_SecUserActivationInRam(args[1]);
      break;
    case SB_SYSCALL_RESET:
      HAL_NVIC_SystemReset();
      break;
    case SB_SYSCALL_MPU_CONFIG:
#if defined(SFU_MPU_PROTECT_ENABLE)
      /* Privileged mode required for MPU re-configuration */
      (void)SFU_LL_SECU_SetProtectionMPU(SFU_SECOND_CONFIGURATION);
#endif /* SFU_MPU_PROTECT_ENABLE */
      break;
    case SB_SYSCALL_DMA_CONFIG:
#if defined(SFU_DMA_PROTECT_ENABLE)
      /* Privileged mode required for DMA re-configuration (clocks access required privilege mode) */
      (void)SFU_LL_SECU_SetProtectionDMA();
#endif /* SFU_DMA_PROTECT_ENABLE */
      break;
    case SB_SYSCALL_DAP_CONFIG:
#if defined(SFU_DAP_PROTECT_ENABLE)
      /* Privileged mode required for DAP re-configuration (clock access required privilege mode) */
      (void)SFU_LL_SECU_SetProtectionDAP();
#endif /* SFU_DAP_PROTECT_ENABLE */
      break;
    case SB_SYSCALL_TAMPER_CONFIG:
#if defined(SFU_TAMPER_PROTECT_ENABLE)
      /* Privileged mode required for TAMPER re-configuration (clock access required privilege mode) */
      (void)SFU_LL_SECU_SetProtectionANTI_TAMPER();
#endif /* SFU_DAP_PROTECT_ENABLE */
      break;
    default:
      /* Force a reset */
      HAL_NVIC_SystemReset();
      break;
  }
}

/**
  * @brief This functions triggers a SB_SFU system call (supervisor call): request privileged operation
  * @param syscall The identifier of the operation to be called (see @ref SFU_MPU_PrivilegedOpId)
  * @param arguments arguments of the privileged operation
  * @retval void
  */
void SFU_MPU_SysCall(uint32_t syscall, ...)
{
  /*
    * You cannot directly change to privileged mode from unprivileged mode without going through an exception, for
    * example an SVC.
    * Handled by @ref MPU_SVC_Handler() and finally @ref SFU_MPU_SVC_Handler()
    */
  __ASM volatile("SVC #1");   /* 1 is the hard-coded value to indicate a SB_SFU syscall */
}

/**
  * @brief This is a helper function to determine if we are currently running in non-privileged mode or not
  * @param void
  * @retval 0 if we are in privileged mode, 1 if we are in non-privileged mode
  */
uint32_t SFU_MPU_IsUnprivileged(void)
{
  return ((__get_IPSR() == 0U) && ((__get_CONTROL() & 1U) == 1U));
}

/**
  * @brief This is a helper function to enter the unprivileged level for software execution
  * @param void
  * @retval void
  */
void SFU_MPU_EnterUnprivilegedMode(void)
{
  __set_PSP(__get_MSP()); /* set up Process Stack Pointer to current stack pointer */
  __set_MSP(SE_REGION_RAM_STACK_TOP); /* change main stack to point on privileged stack */
  __set_CONTROL(__get_CONTROL() | 3U); /* bit 0 = 1: unpriviledged      bit 1=1: stack=PSP */
  __ISB();
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
