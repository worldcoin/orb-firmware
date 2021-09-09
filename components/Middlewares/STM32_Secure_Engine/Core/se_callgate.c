/**
  ******************************************************************************
  * @file    se_callgate.c
  * @author  MCD Application Team
  * @brief   Secure Engine CALLGATE module.
  *          This file provides set of firmware functions to manage SE Callgate
  *          functionalities.
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
/* Place code in a specific section */

/* Includes ------------------------------------------------------------------ */
#include <stdarg.h>
#ifndef KMS_ENABLED
#include "se_crypto_bootloader.h" /* crypto services dedicated to bootloader */
#endif /* !KMS_ENABLED */
#include "se_crypto_common.h"     /* common crypto services for bootloader services and user application services */
#include "se_user_application.h"  /* user application services called by the User Application */
#include "se_callgate.h"
#include "se_key.h"
#include "se_utils.h"
#include "se_fwimg.h"
#include "se_low_level.h"
#include "se_startup.h"
#include "string.h"
#include "se_intrinsics.h"
#if defined(__ARMCC_VERSION)
#include "mapping_sbsfu.h"
#elif defined (__ICCARM__) || defined(__GNUC__)
#include "mapping_export.h"
#endif /* __ICCARM__ || __GNUC__ */
#if defined (CKS_ENABLED)
#include "se_cm0.h"
#endif /* CKS_ENABLED */

/* Include SE interface header files to access interface definitions */
#include "se_interface_common.h"
#include "se_interface_bootloader.h"
#include "se_interface_application.h"

#ifdef KMS_ENABLED
#include "kms.h"
#include "kms_entry.h"
#include "kms_objects.h"
#endif /* KMS_ENABLED */

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup  SE_CALLGATE SE CallGate
  * @brief Implementation of the call gate API.
  *        The call gate allows the execution of code inside the protected area.
  * @{
  */

/* Private typedef ----------------------------------------------------------- */
/** @defgroup SE_CALLGATE_Private_Constants Private Constants
  * @{
  */

/**
  * @brief Secure Engine Lock Status enum definition
  */
typedef enum
{
  SE_UNLOCKED = 0x55555555U,
  SE_LOCKED = 0x7AAAAAAAU
} SE_LockStatus;

/**
  * @}
  */

/* Private macros ------------------------------------------------------------ */
/** @defgroup SE_CALLGATE_Private_Macros Private Macros
  * @{
  */

/**
  * @brief Check if the caller is located in SE Interface region
  */
#define IS_CALLER_SE_IF() \
  do{ \
    if (LR< SE_IF_REGION_ROM_START){\
      NVIC_SystemReset();}\
    if (LR> SE_IF_REGION_ROM_END){\
      NVIC_SystemReset();}\
  }while(0)

/**
  * @brief If lock restriction service enabled, execution is forbidden
  */

#define IS_SE_LOCKED_SERVICES() \
  do{ \
    if (SE_LockRestrictedServices != SE_UNLOCKED){\
      NVIC_SystemReset();}\
  }while(0)

/**
  * @}
  */

/* Global variables ---------------------------------------------------------- */
/** @defgroup SE_CALLGATE_Private_Variables Private Variables
  * @{
  */

#if defined(SFU_ISOLATE_SE_WITH_FIREWALL) || defined(CKS_ENABLED)
/* Variable used to store Appli vector position */
uint32_t AppliVectorsAddr;
#endif /* (SFU_ISOLATE_SE_WITH_FIREWALL) || (CKS_ENABLED) */

#if defined(SFU_ISOLATE_SE_WITH_FIREWALL) && defined(IT_MANAGEMENT)
/* Variable used to store User Primask value */
uint32_t PrimaskValue;
/* Interrupt being handled by SE */
uint32_t IntHand = 0UL; /* 0 means that no interrupt is being handled, 1 means that an interrupt is being handled */
/* Application Active Stack Pointer: 0 means MSP, 1 means PSP */
uint32_t AppliActiveSpMode = 0x0F0F0F0FUL; /* init to invalid value */
/* Application Main Stack Pointer & Active Stack pointer */
uint32_t AppliMsp;
uint32_t AppliActiveSp;
/* SE Main Stack Pointer */
uint32_t SeMsp;
/* SP when entering SE IT Handler */
uint32_t SeExcEntrySp;
/* EXC_RETURN value in SE Handler Mode */
uint32_t SeExcReturn;
/* miscelleaneous register address */
uint32_t ScbVtorAddr = (uint32_t) &(SCB->VTOR);
uint32_t FirewallCrAddr = (uint32_t) &(FIREWALL->CR);
/* set address of SE_UserHandlerWrapper: it is placed in the first entry of SE_IF_REGION_ROM_START */
uint32_t SE_UserHandlerWrapperAddr = SE_IF_REGION_ROM_START + 1UL;
#endif /* SFU_ISOLATE_SE_WITH_FIREWALL && IT_MANAGEMENT */
/**
  * @}
  */

/* Private function prototypes ----------------------------------------------- */
SE_ErrorStatus SE_CallGateService(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *const peSE_Status, va_list arguments);

/**
  * @brief Switch stack pointer from SB RAM region to SE RAM region then call SE_CallGateService
  */
SE_ErrorStatus SE_SP_SMUGGLE(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *const peSE_Status, va_list arguments);

#if defined(IT_MANAGEMENT)
void SE_ExitHandler_Service(void);
#endif /* IT_MANAGEMENT */

/* Functions Definition ------------------------------------------------------ */
/* Place code in a specific section */
#if defined(__ICCARM__)
#pragma default_function_attributes = @ ".SE_CallGate_Code"
#pragma optimize=none
#elif defined(__CC_ARM)
#pragma arm section code = ".SE_CallGate_Code"
#pragma O0
#elif defined(__ARMCC_VERSION)
__attribute__((section(".SE_CallGate_Code")))
__attribute__((optnone))
#elif defined(__GNUC__)
__attribute__((section(".SE_CallGate_Code")))
__attribute__((optimize("O0")))
#endif /* __ICCARM__ */

/** @defgroup SE_CALLGATE_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief Secure Engine CallGate function.
  *        It is the only access/exit point to code inside protected area.
  *        In order to call others functions included in the protected area, the specific ID
  *        has to be specified.
  * @note  It is a variable argument function. The correct number and order of parameters
  *        has to be used in order to call internal function in the right way.
  * @note DO NOT MODIFY THIS FUNCTION.
  *       New services can be implemented in @ref SE_CallGateService.
  * @param eID: Secure Engine protected function ID.
  * @param peSE_Status: Secure Engine Status.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CallGate(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *const peSE_Status, uint32_t PrimaskParam, ...)
{
  SE_ErrorStatus e_ret_status;
  va_list arguments;
  volatile uint32_t LR;

#if defined(__CC_ARM) || defined(__ICCARM__)
  LR = __get_LR();
#elif defined(__ARMCC_VERSION)
  __asm volatile("MOV %0, LR\n" : "=r"(LR));
#elif defined(__GNUC__)
  register unsigned lr asm("lr");
  LR = lr;
#endif /* defined(__CC_ARM) || defined(__ICCARM__) */

  /* Enter the protected area */
  ENTER_PROTECTED_AREA();

  /*
   * Warning : It is mandatory to call NVIC_SystemReset() in case of error
   * instead of return(SE_ERROR) to avoid any attempt of attack by modifying
   * the call stack (LR) in order to execute code inside secure enclave
   */

  /* Check the Callgate was called only from SE Interface */
  IS_CALLER_SE_IF();

  /* Check the pointers allocation */
  if (SE_LL_Buffer_in_ram(peSE_Status, sizeof(*peSE_Status)) != SE_SUCCESS)
  {
    NVIC_SystemReset();
  }
  if (SE_LL_Buffer_part_of_SE_ram(peSE_Status, sizeof(*peSE_Status)) == SE_SUCCESS)
  {
    NVIC_SystemReset();
  }

  /* Double Check to avoid basic fault injection : the Callgate was called only from SE Interface */
  IS_CALLER_SE_IF();

  /* Double Check to avoid basic fault injection : Check the pointers allocation */
  if (SE_LL_Buffer_in_ram(peSE_Status, sizeof(*peSE_Status)) != SE_SUCCESS)
  {
    NVIC_SystemReset();
  }
  if (SE_LL_Buffer_part_of_SE_ram(peSE_Status, sizeof(*peSE_Status)) == SE_SUCCESS)
  {
    NVIC_SystemReset();
  }

#if defined(IT_MANAGEMENT)
  if (eID != SE_EXIT_INTERRUPT)
  {
    /* If an interrupt is being handled, then no service is allowed */
    if (IntHand == 1UL)
    {
      *peSE_Status = SE_BUSY;
      EXIT_PROTECTED_AREA();
      return SE_SUCCESS;
    }
    /* Save user PRIMASK value */
    PrimaskValue = PrimaskParam;

    /* get current Active Stack Pointer */
    AppliActiveSpMode = (__get_CONTROL() >> 1UL) & 0x00000001UL;
  }
  else /* An exit interrupt service is requested */
  {
    /* Whereas there is no interrupt handling on going */
    if (IntHand != 1UL)
    {
      *peSE_Status = SE_OK;
      EXIT_PROTECTED_AREA();
      return SE_ERROR;
    }

    /* Requested service = SE_EXIT_INTERRUPT,
    so we should be in Handler Mode,
    and so, current SP mode should be MSP
    */
    if (((__get_CONTROL() >> 1UL) & 0x00000001UL) != 0x00000000UL)
    {
      *peSE_Status = SE_BUSY;
      EXIT_PROTECTED_AREA();
      return SE_SUCCESS;
    }
  }
#else
  /* Prevent unused argument(s) compilation warning */
  UNUSED(PrimaskParam);
#endif /* IT_MANAGEMENT) */

#if defined(SFU_ISOLATE_SE_WITH_FIREWALL) || defined(CKS_ENABLED)
  /* Save Appli Vector Table Address   */
  AppliVectorsAddr = SCB->VTOR;

  /* Set SE vector */
  SCB->VTOR = (uint32_t)&SeVectorsTable;
#endif /* SFU_ISOLATE_SE_WITH_FIREWALL || CKS_ENABLED */

  *peSE_Status =  SE_OK;

  /* Initializing arguments to store all values after peSE_Status */
  va_start(arguments, PrimaskParam);

  /* Call service implementation , this is split to have a fixed size */
#if defined(SFU_ISOLATE_SE_WITH_FIREWALL)
  /* Set SE specific stack before executing SE service */
  e_ret_status =  SE_SP_SMUGGLE(eID, peSE_Status, arguments);
#else
  /* No need to use a specific Stack */
  e_ret_status =  SE_CallGateService(eID, peSE_Status, arguments);
#endif /* SFU_ISOLATE_SE_WITH_FIREWALL */

  /* Clean up arguments list*/
  va_end(arguments);

  /* Restore Appli Vector Value   */
#if defined(CKS_ENABLED)
  HAL_NVIC_DisableIRQ(IPCC_C1_RX_IRQn);
  HAL_NVIC_DisableIRQ(IPCC_C1_TX_IRQn);
  __ISB();
  SCB->VTOR = AppliVectorsAddr;
#endif /* CKS_ENABLED */
#if defined(SFU_ISOLATE_SE_WITH_FIREWALL)
  SCB->VTOR = AppliVectorsAddr;
#endif /* SFU_ISOLATE_SE_WITH_FIREWALL */

  /* Exit the protected area */
  EXIT_PROTECTED_AREA();

  return e_ret_status;
}
/* Stop placing data in specified section */
#if defined(__ICCARM__)
#pragma default_function_attributes =
#pragma optimize=none
#elif defined(__CC_ARM)
#pragma arm section code
#endif /* __ICCARM__ */
/**
  * @brief Dispatch function used by the Secure Engine CallGate function.
  *        Calls other functions included in the protected area based on the eID parameter.
  * @note  It is a variable argument function. The correct number and order of parameters
  *        has to be used in order to call internal function in the right way.
  * @param eID: Secure Engine protected function ID.
  * @param peSE_Status: Secure Engine Status.
  * @param arguments: argument list to be extracted. Depends on each protected function ID.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CallGateService(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *const peSE_Status, va_list arguments)
{

  /*
   * For the time being we consider that the user keys can be handled in SE_CallGate.
   * Nevertheless, if this becomes too crypto specific then it will have to be moved to the user application code.
   */
  static SE_LockStatus SE_LockRestrictedServices = SE_UNLOCKED;

  SE_ErrorStatus e_ret_status = SE_ERROR;

  switch (eID)
  {
    /* ==================================== */
    /* ===== INTERRUPT HANDLING PART  ===== */
    /* ==================================== */
    case SE_EXIT_INTERRUPT:
    {
#if defined(IT_MANAGEMENT)
      /* Exit the Handler Mode */
      SE_ExitHandler_Service();

      /* We shall not raise this point ! */
      NVIC_SystemReset();
#endif /* IT_MANAGEMENT */
      break;
    }


    /* ============================ */
    /* ===== BOOTLOADER PART  ===== */
    /* ============================ */
    case SE_INIT_ID:
    {
      uint32_t se_system_core_clock;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      se_system_core_clock = va_arg(arguments, uint32_t);

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Initialization of SystemCoreClock variable in Secure Engine binary */
      SE_SetSystemCoreClock(se_system_core_clock);

#if defined(CKS_ENABLED)
      /* Init communication for keys management with CPU2 */
      CM0_Init();
#endif /* defined(CKS_ENABLED) */

      *peSE_Status = SE_OK;
      e_ret_status = SE_SUCCESS;

      /* NOTE : Other initialization may be added here. */
      break;
    }

    case SE_CRYPTO_LL_DECRYPT_INIT_ID:
    {
#ifndef KMS_ENABLED
      SE_FwRawHeaderTypeDef *p_x_se_Metadata;
      uint32_t se_FwType;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_x_se_Metadata = va_arg(arguments, SE_FwRawHeaderTypeDef *);
      se_FwType = va_arg(arguments, uint32_t);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the Init structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the Fw Type parameter value */
      if ((se_FwType != SE_FW_IMAGE_COMPLETE) && (se_FwType != SE_FW_IMAGE_PARTIAL))
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the Init structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      /*
       * The se_callgate code must be crypto-agnostic.
       * But, when it comes to decrypt, we can use:
       * - either a real decrypt operation (AES GCM or AES CBC FW encryption)
       * - or no decrypt (clear FW)
       * As a consequence, retrieving the key cannot be done at the call gate stage.
       * This is implemented in the SE_CRYPTO_Decrypt_Init code.
       */

      /* Call SE CRYPTO function */
      e_ret_status = SE_CRYPTO_Decrypt_Init(p_x_se_Metadata, se_FwType);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_CRYPTO_LL_DECRYPT_APPEND_ID:
    {
#ifndef KMS_ENABLED
      const uint8_t *input_buffer;
      int32_t      input_size;
      uint8_t      *output_buffer;
      int32_t      *output_size;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      input_buffer = va_arg(arguments, const uint8_t *);
      input_size = va_arg(arguments, int32_t);
      output_buffer = va_arg(arguments, uint8_t *);
      output_size = va_arg(arguments, int32_t *);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the pointers allocation */
      if (input_size <= 0)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(input_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the pointers allocation */
      if (input_size <= 0)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(input_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Call SE CRYPTO function */
      e_ret_status = SE_CRYPTO_Decrypt_Append(input_buffer, input_size, output_buffer, output_size);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_CRYPTO_LL_DECRYPT_FINISH_ID:
    {
#ifndef KMS_ENABLED
      uint8_t      *output_buffer;
      int32_t      *output_size;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      output_buffer = va_arg(arguments, uint8_t *);
      output_size = va_arg(arguments, int32_t *);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the pointers allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      /* In AES-GCM 16 bytes can be written  */
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)16U) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the pointers allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* In AES-GCM 16 bytes can be written  */
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)16U) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Call SE CRYPTO function */
      e_ret_status =  SE_CRYPTO_Decrypt_Finish(output_buffer, output_size);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_CRYPTO_LL_AUTHENTICATE_FW_INIT_ID:
    {
#ifndef KMS_ENABLED
      SE_FwRawHeaderTypeDef *p_x_se_Metadata;
      uint32_t se_FwType;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_x_se_Metadata = va_arg(arguments, SE_FwRawHeaderTypeDef *);
      se_FwType = va_arg(arguments, uint32_t);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the Init structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the Fw Type parameter value */
      if ((se_FwType != SE_FW_IMAGE_COMPLETE) && (se_FwType != SE_FW_IMAGE_PARTIAL))
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the Init structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /*
       * The se_callgate code must be crypto-agnostic.
       * But, when it comes to FW authentication, we can use:
       * - either AES GCM authentication
       * - or SHA256 (stored in an authenticated FW header)
       * So this service can rely:
       * - either on the symmetric key
       * - or a SHA256
       * As a consequence, retrieving the key cannot be done at the call gate stage.
       * This is implemented in the SE_CRYPTO_AuthenticateFW_Init code.
       */

      /* Call SE CRYPTO function */
      e_ret_status = SE_CRYPTO_AuthenticateFW_Init(p_x_se_Metadata, se_FwType);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_CRYPTO_LL_AUTHENTICATE_FW_APPEND_ID:
    {
#ifndef KMS_ENABLED
      const uint8_t *input_buffer;
      int32_t      input_size;
      uint8_t       *output_buffer;
      int32_t      *output_size;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      input_buffer = va_arg(arguments, const uint8_t *);
      input_size = va_arg(arguments, int32_t);
      output_buffer = va_arg(arguments, uint8_t *);
      output_size = va_arg(arguments, int32_t *);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the pointers allocation */
      if (input_size <= 0)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(input_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the pointers allocation */
      if (input_size <= 0)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(input_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)input_size) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Call SE CRYPTO function */
      e_ret_status = SE_CRYPTO_AuthenticateFW_Append(input_buffer, input_size, output_buffer, output_size);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_CRYPTO_LL_AUTHENTICATE_FW_FINISH_ID:
    {
#ifndef KMS_ENABLED
      uint8_t       *output_buffer;
      int32_t       *output_size;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* retrieve argument(s) */
      output_buffer = va_arg(arguments, uint8_t *);
      output_size = va_arg(arguments, int32_t *);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the pointers allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      /* In AES-GCM 16 bytes can be written  */
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)16U) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the pointers allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(output_size, sizeof(*output_size)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      /* In AES-GCM 16 bytes can be written  */
      if (SE_LL_Buffer_in_SBSFU_ram(output_buffer, (uint32_t)16U) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Call SE CRYPTO function */
      e_ret_status = SE_CRYPTO_AuthenticateFW_Finish(output_buffer, output_size);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_CRYPTO_HL_AUTHENTICATE_METADATA:
    {
#ifndef KMS_ENABLED
      /*
       * The se_callgate code must be crypto-agnostic.
       * But, when it comes to metadata authentication, we can use:
       * - either AES GCM authentication
       * - or SHA256 signed with ECCDSA authentication
       * So this service can rely:
       * - either on the symmetric key
       * - or on the asymmetric keys
       * As a consequence, retrieving the appropriate key cannot be done at the call gate stage.
       * This is implemented in the SE_CRYPTO_Authenticate_Metadata code.
       */
      SE_FwRawHeaderTypeDef *p_x_se_Metadata;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_x_se_Metadata = va_arg(arguments, SE_FwRawHeaderTypeDef *);

      /* CRC configuration may have been changed by application */
      if (SE_LL_CRC_Config() == SE_ERROR)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Check the metadata structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : Check the metadata structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      /* Call SE CRYPTO function */
      e_ret_status = SE_CRYPTO_Authenticate_Metadata(p_x_se_Metadata);
#endif /* !KMS_ENABLED */
      break;
    }

    case SE_IMG_READ:
    {
      uint8_t *p_destination;
      const uint8_t *p_source;
      uint32_t length;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_destination = va_arg(arguments, uint8_t *);
      p_source = va_arg(arguments, const uint8_t *);
      length = va_arg(arguments, uint32_t);
      /* Check the destination buffer */
      if (SE_LL_Buffer_in_SBSFU_ram(p_destination, length) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : check the destination buffer */
      if (SE_LL_Buffer_in_SBSFU_ram(p_destination, length) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Read Flash memory */
      e_ret_status = SE_IMG_Read(p_destination, p_source, length);
      break;
    }

    case SE_IMG_WRITE:
    {
      uint8_t *p_destination;
      const uint8_t *p_source;
      uint32_t length;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_destination = va_arg(arguments, uint8_t *);
      p_source = va_arg(arguments, const uint8_t *);
      length = va_arg(arguments, uint32_t);

      /* Check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram(p_source, length) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram(p_source, length) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Write Flash memory */
      e_ret_status = SE_IMG_Write(p_destination, p_source, length);
      break;
    }

    case SE_IMG_ERASE:
    {
      uint8_t *p_destination;
      uint32_t length;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_destination = va_arg(arguments, uint8_t *);
      length = va_arg(arguments, uint32_t);

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Erase Flash memory */
      e_ret_status = SE_IMG_Erase(p_destination, length);
      break;
    }

#if defined(CKS_ENABLED)
    case SE_CM0_UPDATE:
    {
      /* Check that the Secure Engine services are not locked, LOCK shall be called only once */
      IS_SE_LOCKED_SERVICES();

      /* FUS or wireless stack update process is managed by CM0 */
      e_ret_status = CM0_Update();

      break;
    }
#endif /* CKS_ENABLED */

    case SE_LOCK_RESTRICT_SERVICES:
    {
#if defined(CKS_ENABLED)
      /* Lock and remove the keys from AES HW */
      SE_CRYPTO_Lock_CKS_Keys();

      /*
       *  SE_LOCK_RESTRICT_SERVICES is called twice to avoid basic fault injection
       *  But, CM0_DeInit should be called only the second time to be able to maintain the communication link
       *  with CM0
       */
      if (SE_LockRestrictedServices == SE_LOCKED)
      {
        /* Ends communication with CM0 in order to allow UserApp to restart the communication */
        CM0_DeInit();
      }
#endif /* CKS_ENABLED */

      /*
       * Clean-up secure engine RAM area for series with secure memory isolation because Flash is hidden when secure
       * memory is activated but RAM is still accessible
       */
      SE_LL_CORE_Cleanup();

      /* Lock restricted services */
      SE_LockRestrictedServices = SE_LOCKED;
      e_ret_status = SE_SUCCESS;

      /* As soon as the SBSFU is done, we can LOCK the keys */
      if (SE_LL_Lock_Keys() != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double instruction to avoid basic fault injection */
      SE_LockRestrictedServices = SE_LOCKED;
      break;
    }

#ifdef OTFDEC_ENABLED
    case SE_EXTFLASH_DECRYPT_INIT:
    {
      SE_FwRawHeaderTypeDef *p_x_se_Metadata;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      p_x_se_Metadata = va_arg(arguments, SE_FwRawHeaderTypeDef *);

      /* Check the Init structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Check the Init structure allocation */
      if (SE_LL_Buffer_in_SBSFU_ram(p_x_se_Metadata, sizeof(*p_x_se_Metadata)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Start On the Fly DECryption (OTFDEC) for external flash */
      e_ret_status = SE_LL_FLASH_EXT_Decrypt_Init(p_x_se_Metadata);
      break;
    }
#endif /* OTFDEC_ENABLED */

#ifdef ENABLE_IMAGE_STATE_HANDLING
    case SE_IMG_GET_FW_STATE:
    {
      uint32_t slot_number;
      SE_FwStateTypeDef *p_fw_state;

      /* Retrieve argument(s) */
      slot_number = va_arg(arguments, uint32_t);
      p_fw_state = va_arg(arguments, SE_FwStateTypeDef *);

      /* Check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram((void *)p_fw_state, sizeof(*p_fw_state)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram((void *)p_fw_state, sizeof(*p_fw_state)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Read state */
      e_ret_status = SE_IMG_GetActiveFwState(slot_number, p_fw_state);
      break;
    }
#endif /* ENABLE_IMAGE_STATE_HANDLING */

#ifdef ENABLE_IMAGE_STATE_HANDLING
    case SE_IMG_SET_FW_STATE:
    {
      uint32_t slot_number;
      SE_FwStateTypeDef *p_fw_state;

      /* Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Retrieve argument(s) */
      slot_number = va_arg(arguments, uint32_t);
      p_fw_state = va_arg(arguments, SE_FwStateTypeDef *);

      /* Check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram((void *)p_fw_state, sizeof(*p_fw_state)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check that the Secure Engine services are not locked */
      IS_SE_LOCKED_SERVICES();

      /* Double Check to avoid basic fault injection : check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram((void *)p_fw_state, sizeof(*p_fw_state)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      e_ret_status = SE_IMG_SetActiveFwState(slot_number, p_fw_state);
      break;
    }
#endif /* ENABLE_IMAGE_STATE_HANDLING */

    /* ============================ */
    /* ===== APPLICATION PART ===== */
    /* ============================ */

    /* --------------------------------- */
    /* FIRMWARE IMAGES HANDLING SERVICES */
    /* --------------------------------- */

    /* No protected service needed for this */

    /* --------------------------------- */
    /* USER APPLICATION SERVICES         */
    /* --------------------------------- */
    case SE_APP_GET_ACTIVE_FW_INFO:
    {
      SE_APP_ActiveFwInfo_t *p_FwInfo;
      uint32_t slot_number;

      /* Retrieve argument(s) */
      slot_number = va_arg(arguments, uint32_t);
      p_FwInfo = va_arg(arguments, SE_APP_ActiveFwInfo_t *);

      /* Check structure allocation */
      if (SE_LL_Buffer_in_ram((void *)p_FwInfo, sizeof(*p_FwInfo)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_part_of_SE_ram((void *)p_FwInfo, sizeof(*p_FwInfo)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check structure allocation */
      if (SE_LL_Buffer_in_ram((void *)p_FwInfo, sizeof(*p_FwInfo)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_part_of_SE_ram((void *)p_FwInfo, sizeof(*p_FwInfo)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      e_ret_status = SE_APPLI_GetActiveFwInfo(slot_number, p_FwInfo);
      break;
    }

#ifdef ENABLE_IMAGE_STATE_HANDLING
    case SE_APP_VALIDATE_FW:
    {
      uint32_t slot_number;
      SE_FwStateTypeDef fw_state;

      /* Retrieve argument(s) */
      slot_number = va_arg(arguments, uint32_t);

      if (slot_number == VALID_ALL_SLOTS)
      {
        fw_state = FWIMG_STATE_VALID_ALL;
        e_ret_status = SE_IMG_SetActiveFwState(MASTER_SLOT, &fw_state);
      }
      else
      {
        fw_state = FWIMG_STATE_VALID;
        e_ret_status = SE_IMG_SetActiveFwState(slot_number, &fw_state);
      }
      break;
    }
#endif /* ENABLE_IMAGE_STATE_HANDLING */

#ifdef ENABLE_IMAGE_STATE_HANDLING
    case SE_APP_GET_FW_STATE:
    {
      uint32_t slot_number;
      SE_FwStateTypeDef *p_fw_state;

      /* Retrieve argument(s) */
      slot_number = va_arg(arguments, uint32_t);
      p_fw_state = va_arg(arguments, SE_FwStateTypeDef *);

      /* Check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram((void *)p_fw_state, sizeof(*p_fw_state)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      if (SE_LL_Buffer_part_of_SE_ram((void *)p_fw_state, sizeof(*p_fw_state)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : check the source buffer */
      if (SE_LL_Buffer_in_SBSFU_ram((void *)p_fw_state, sizeof(*p_fw_state)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      if (SE_LL_Buffer_part_of_SE_ram((void *)p_fw_state, sizeof(*p_fw_state)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Read state */
      e_ret_status = SE_IMG_GetActiveFwState(slot_number, p_fw_state);
      break;
    }
#endif /* ENABLE_IMAGE_STATE_HANDLING */

#if defined(SFU_ISOLATE_SE_WITH_MPU) && defined(UPDATE_IRQ_SERVICE)
    case SE_SYS_SAVE_DISABLE_IRQ:
    {
      uint32_t *pIrqState;
      uint32_t IrqStateNb;

      /* retrieve argument(s) */
      pIrqState = va_arg(arguments, uint32_t *);
      IrqStateNb = va_arg(arguments, uint32_t);

      /* Check structure allocation */
      if (SE_LL_Buffer_in_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_part_of_SE_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check structure allocation */
      if (SE_LL_Buffer_in_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_part_of_SE_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      e_ret_status = SE_LL_Save_Disable_Irq(pIrqState, IrqStateNb);
      break;
    }
#endif /* SFU_ISOLATE_SE_WITH_MPU && UPDATE_IRQ_SERVICE */

#if defined(SFU_ISOLATE_SE_WITH_MPU) && defined(UPDATE_IRQ_SERVICE)
    case SE_SYS_RESTORE_ENABLE_IRQ:
    {
      uint32_t *pIrqState;
      uint32_t IrqStateNb;

      /* retrieve argument(s) */
      pIrqState = va_arg(arguments, uint32_t *);
      IrqStateNb = va_arg(arguments, uint32_t);

      /* Check structure allocation */
      if (SE_LL_Buffer_in_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_part_of_SE_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      /* Double Check to avoid basic fault injection : Check structure allocation */
      if (SE_LL_Buffer_in_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) != SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }
      if (SE_LL_Buffer_part_of_SE_ram((void *)pIrqState, IrqStateNb * sizeof(*pIrqState)) == SE_SUCCESS)
      {
        e_ret_status = SE_ERROR;
        break;
      }

      e_ret_status = SE_LL_Restore_Enable_Irq(pIrqState, IrqStateNb);

      break;
    }
#endif /* SFU_ISOLATE_SE_WITH_MPU && UPDATE_IRQ_SERVICE */

    default:
    {
#ifdef KMS_ENABLED
      /* Is it a request to the KMS lib */
      if ((eID & SE_MW_ADDON_MSB_MASK) == SE_MW_ADDON_KMS_MSB)
      {
        CK_RV    l_rv;
        /* Clear SE MW part of the ID, add KMS calling cluster part to the ID */
        KMS_FunctionID_t kmsID = (eID & ~(SE_MW_ADDON_MSB_MASK | KMS_CLUST_MASK)) | KMS_CLUST_UNSEC;

        /* To limit the number of passed parameters, we consider that */
        /* KMS_Entry() returns the CK_RV error. */
        l_rv = KMS_Entry(kmsID, arguments);

        /*
         * SE_StatusTypeDef is used to forward the CK_RV result to upper layers
         */
        *peSE_Status = l_rv;

        if (l_rv == CKR_OK)
        {
          e_ret_status = SE_SUCCESS;
        }
        else
        {
          e_ret_status = SE_ERROR;
        }
        break;
      }
#endif /* KMS_ENABLED */
      /* Unspecified function ID -> Reset */
      NVIC_SystemReset();

      break;
    }
  }
  if ((e_ret_status == SE_ERROR) && (*peSE_Status == SE_OK))
  {
    *peSE_Status = SE_KO;
  }
  return e_ret_status;
}

/* Stop placing data in specified section*/


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

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
