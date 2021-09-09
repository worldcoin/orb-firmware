/**
  ******************************************************************************
  * @file    sfu_boot.c
  * @author  MCD Application Team
  * @brief   SFU BOOT module
  *          This file provides firmware functions to manage the following
  *          functionalities of the Secure Boot:
  *           + Initialization and de-initialization functions
  *           + Secure Boot Control functions
  *           + Secure Boot State functions
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

#define SFU_BOOT_C

/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "sfu_boot.h"
#include "sfu_loader.h"
#include "sfu_low_level_security.h"
#include "sfu_low_level_flash_int.h"
#include "sfu_low_level.h"
#include "sfu_fsm_states.h"
#include "sfu_error.h"
#include "stm32g4xx_it.h" /* required for the HAL Cube callbacks */
/*
 * The sfu_com init is provided by the sfu_com_trace module by default.
 * If not, then it is taken from the sfu_com_loader module.
 */
#if defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION)
#include "sfu_trace.h"
#else
#include "sfu_trace.h"      /* needed anyhow even if the defines will be empty */
#include "sfu_com_loader.h" /* needed only for the COM init/de-init */
#endif /* SFU_DEBUG_MODE  || SFU_TEST_PROTECTION */
#include "se_def.h"
#include "se_interface_bootloader.h"  /* sfu_boot is the bootloader core part */
#include "sfu_new_image.h"            /* the local loader is a kind of "application" running in SB_SFU so it needs the
                                         services to install a FW image */
#include "sfu_fwimg_services.h"       /* sfu_boot uses the services of the FWIMG module */
#include "sfu_test.h"                 /* auto tests */
#include "sfu_fwimg_internal.h"
#include "sfu_standalone_loader.h"
#include "sfu_mpu_isolation.h"        /* Secure Engine isolated thanks to MPU */


/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  SFU_BOOT_StateMachineTypeDef  PrevState;      /*!< The previous state of the State Machine */
  SFU_BOOT_StateMachineTypeDef  CurrState;      /*!< The current state of the State Machine */
} SFU_BOOT_StateMachineContextTypeDef;          /*!< Specifies a structure containing the State Machine context
                                                     information using during the SM evolution. */

/* Private defines -----------------------------------------------------------*/
#define EXEC_ID_SECURE_BOOT     0U       /*!< ID for Secure Boot */
#define EXEC_ID_USER_APP        1U       /*!< ID for User App */
#define IS_VALID_EXEC_ID(EXEC_ID)        (((EXEC_ID) == EXEC_ID_SECURE_BOOT) || \
                                          ((EXEC_ID) == EXEC_ID_USER_APP)) /*!< Check for valid ID */

#define RESERVED_VALUE (0xFEU) /*!< Reserved value. The reserved field used inside the LastExecStatus of the BootInfo is
                                    maintained for future customization/expansion of the field itself */

#define SFU_STATE_INITIAL     SFU_STATE_CHECK_STATUS_ON_RESET  /*!< Define the initial state*/

/* Private macros ------------------------------------------------------------*/
#define SFU_SET_SM_IF_CURR_STATE(Status, SM_STATE_OK, SM_STATE_FAILURE) \
  do{                                                                   \
    m_StateMachineContext.PrevState = m_StateMachineContext.CurrState;  \
    if (Status == SFU_SUCCESS){                                         \
      m_StateMachineContext.CurrState = SM_STATE_OK;                    \
    }                                                                   \
    else {                                                              \
      m_StateMachineContext.CurrState = SM_STATE_FAILURE;               \
    }                                                                   \
  }while(0) /*!< Set a State Machine state according to the 'Status' value*/


#define SFU_SET_SM_CURR_STATE(NewState)                                 \
  do{                                                                   \
    m_StateMachineContext.PrevState = m_StateMachineContext.CurrState;  \
    m_StateMachineContext.CurrState = NewState;                         \
  }while(0) /*!< Set a State Machine state*/

/* Private variables ---------------------------------------------------------*/
/*!< Static member variables representing the StateMachine context used during the StateMachine evolution. */
static __IO SFU_BOOT_StateMachineContextTypeDef m_StateMachineContext = {SFU_STATE_INITIAL,
                                                                         SFU_STATE_INITIAL
                                                                        };

/*!< Static member variables identifyng the slots to be processed by secure firmware update . */
static uint32_t m_DwlSlotToInstall = SLOT_INACTIVE;
static uint32_t m_ActiveSlotToResume = SLOT_INACTIVE;
static uint32_t m_ActiveSlotToRollback = SLOT_INACTIVE;
static uint32_t m_ActiveSlotToExecute = SLOT_INACTIVE;

/* Global variables ----------------------------------------------------------*/
/**
  * This variable indicates if at boot-up the FW presence check has already been performed or not:
  * \li It is reset when the FW status has already been checked once and no FW is present
  * \li (see @ref SFU_BOOT_SM_CheckUserFwStatus)
  * \li It is used to display some messages only once in the teraterm console
  * \li It is used to determine if the user button must be pressed before waiting for a local download to start
  * \li This is used at LocalDownload and CheckUserFwStatus stages.
  */
uint8_t initialDeviceStatusCheck;

/**
  *  Flow control initial values.
  *   - Security protections flow (static + dynamic)
  *   - Crypto operations flow (authentication, integrity)
  */
uint32_t uFlowProtectValue = FLOW_CTRL_INIT_VALUE;
uint32_t uFlowCryptoValue = FLOW_CTRL_INIT_VALUE;

/* Private function prototypes -----------------------------------------------*/
static SFU_ErrorStatus SFU_BOOT_Init(void);
static SFU_ErrorStatus SFU_BOOT_DeInit(void);
static void SFU_BOOT_BspConfiguration(void);
static SFU_ErrorStatus SFU_BOOT_SM_Run(void);
static void SFU_BOOT_SM_CheckStatusOnReset(void);
#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
static void SFU_BOOT_SM_CheckNewFwToDownload(void);
static void SFU_BOOT_SM_DownloadNewUserFw(void);
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
static void SFU_BOOT_SM_CheckUserFwStatus(void);
static void SFU_BOOT_SM_VerifyUserFwSignature(void);
static void SFU_BOOT_SM_ExecuteUserFw(void);
static void SFU_BOOT_SM_HandleCriticalFailure(void);
static void SFU_BOOT_SM_RebootStateMachine(void);
static void SFU_BOOT_SM_InstallNewUserFw(void);
static void SFU_BOOT_SM_ResumeInstallNewUserFw(void);
static void SFU_BOOT_SM_RollbackInstallPrevUserFw(void);
#if (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
static SFU_ErrorStatus SFU_BOOT_LaunchStandaloneLoader(void);
#endif /* (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
static SFU_ErrorStatus SFU_BOOT_SystemSecurity_Config(void);
static SFU_ErrorStatus SFU_BOOT_CheckApplySecurityProtections(void);
static SFU_ErrorStatus SFU_BOOT_SecuritySafetyCheck(void);
static void SFU_BOOT_ManageResetSources(void);

/* Functions Definition ------------------------------------------------------*/
/**
  * @brief This function starts the secure boot service and returns only if a configuration issue occurs.
  *        In the nominal case, the bootloader service runs until the user application is launched.
  *        When no valid user application can be run (after installing a new image or not),
  *        if the local loader feature is not enabled then the execution stops,
  *        otherwise a local download will be awaited.
  *        If the state machine encounters a major issue then a reboot is triggered.
  * @param None.
  * @note Please note that this service initializes all the required sub-services and rely on them to perform its tasks.
  * @note Constraints
  *       1. The system initialization must be completed (HAL, clocks, peripherals...) before calling this function.
  *       2. This function also takes care of BSP initialization after enabling the secure mode.
  *          The BSP init code can be added in @ref SFU_BOOT_BspConfiguration().
  *       3. No other entity should handle the initialization of the Secure Engine.
  *       4. The other SB_SFU services should NOT be configured by other entities if this service is used (the previous
  *          configurations will be overwritten).
  *       5. The other SB_SFU services should NOT be used by any other entity if this service is running.
  *       6. When returning from this function a reboot should be triggered (NVIC_SystemReset) after processing the
  *          error cause.
  *       7. The caller must be prepared to never get the hand back after calling this function (jumping in user
  *          application by default or entering local loader state if local loader is enabled or rebooting to install a
  *          new image).
  * @note Settings are handled at compilation time:
  *       1. See compiler switches in main.h for secure IPs settings
  *       2. The trace system is configured in the sfu_trace.h file
  * @retval SFU_BOOT_InitErrorTypeDef error code as the function returns only if a critical failure occurs at init
  *          stage.
  */
SFU_BOOT_InitErrorTypeDef SFU_BOOT_RunSecureBootService()
{
  SFU_BOOT_InitErrorTypeDef e_ret_code = SFU_BOOT_INIT_ERROR;

  /*
   * initialize Secure Engine variable as secure Engine is managed as a completely separate binary - not
   * "automatically" managed by SBSFU compiler command
   */
  if (SE_Startup() == SE_SUCCESS)
  {
    /* Security Configuration */
    if (SFU_BOOT_SystemSecurity_Config() == SFU_SUCCESS)
    {
      /* Board BSP  Configuration */
      SFU_BOOT_BspConfiguration();

      /* Configure the Secure Boot and start the State machine */
      if (SFU_BOOT_Init() == SFU_SUCCESS)
      {
        /* Start the Secure Boot State Machine */
        (void) SFU_BOOT_SM_Run();
      }
      else
      {
        /* failure when initializing the secure boot service */
        e_ret_code = SFU_BOOT_INIT_FAIL;
      }
    }
    else
    {
      /* failure when configuring the security IPs  */
      e_ret_code = SFU_BOOT_SECIPS_CFG_FAIL;
    }
  }
  else
  {
    /* failure at secure engine initialization stage */
    e_ret_code = SFU_BOOT_SECENG_INIT_FAIL;
  }

  /*
   * This point should not be reached unless a critical init failure occurred
   * Return the error code
   */
  return (e_ret_code);
}

/**
  * @brief  Force System Reboot
  * @param  None
  * @retval None
  */
void SFU_BOOT_ForceReboot(void)
{
  /*
   * WARNING: The follow TRACEs are for debug only. This function could be called
   * inside an IRQ so the below printf could not be executed or could generate a fault!
   */
  TRACE("\r\n========= End of Execution ==========");
  TRACE("\r\n\r\n\r\n");

  /*
   * This is the last operation executed. Force a System Reset.
   * The check below is needed in case we reboot before setting the Unpriviledged mode.
   * This can happen in a factory reset use-case typically.
   */
  if (0U != SFU_MPU_IsUnprivileged())
  {
    SFU_MPU_SysCall((uint32_t)SB_SYSCALL_RESET);
  }
  else
  {
    NVIC_SystemReset();
  }
}

/**
  * @brief  Initialize the Secure Boot State machine.
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus SFU_BOOT_Init(void)
{
  SFU_ErrorStatus  e_ret_status = SFU_ERROR;
  SE_StatusTypeDef e_se_status;

  /*
   * We start the execution at boot-up (display all messages in teraterm console, check the trigger to force a local
   * download)
   */
  initialDeviceStatusCheck = 1U;

  /* Call the Hardware Abstraction Layer Init implemented for the specific MCU */
  if (SFU_LL_Init() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }

  /* Flash interface initialization */
  if (SFU_LL_FLASH_Init() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }

  /* The COM modules is required only if the trace or the local download is enabled */
#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION)
  /* Call the COM module Init (already handled in SFU_BOOT_SystemSecurity_Config) */
  if (SFU_COM_Init() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }
#endif /* SECBOOT_USE_LOCAL_LOADER || SFU_DEBUG_MODE || SFU_TEST_PROTECTION*/

#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER)
  /* Call the SFU_LOADER module Init */
  if (SFU_LOADER_Init() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }
#endif /* SECBOOT_USE_LOCAL_LOADER */

  /* Call the Exception module Init */
  if (SFU_EXCPT_Init() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }

  /* Call the image handling Init  */
  if (SFU_IMG_InitImageHandling() != SFU_IMG_INIT_OK)
  {
    return SFU_ERROR;
  } /* else continue */


  TRACE("\r\n\r\n");
  TRACE("\r\n======================================================================");
  TRACE("\r\n=              (C) COPYRIGHT 2017 STMicroelectronics                 =");
  TRACE("\r\n=                                                                    =");
  TRACE("\r\n=              Secure Boot and Secure Firmware Update                =");
  TRACE("\r\n======================================================================");
  TRACE("\r\n\r\n");

  /* Initialize the Secure Engine that will be used for all the most critical operations */
  if (SE_Init(&e_se_status, SystemCoreClock) != SE_SUCCESS)
  {
    TRACE("\r\n= [SBOOT] SECURE ENGINE INITIALIZATION CRITICAL FAILURE!");
  }
  else
  {
    e_ret_status = SFU_SUCCESS;
    TRACE("\r\n= [SBOOT] SECURE ENGINE INITIALIZATION SUCCESSFUL");
  }

  return e_ret_status;
}

/**
  * @brief  DeInitialize the Secure Boot State machine.
  * @param  None
  * @note   Please note that in this example the de-init function is used only once to avoid a compiler warning.
  *         The bootloader can terminate:
  *         1. with an init failure : no de-init needed
  *         2. with a critical failure leading to a reboot: no de-init needed as long as no persistent info is stored
  *            by this function.
  *         3. when launching the user app: de-init may be called here if required as long as it does not disengage the
  *            required security mechanisms.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus SFU_BOOT_DeInit(void)
{
  if (SFU_EXCPT_DeInit() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }

#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER)
  if (SFU_LOADER_DeInit() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }
#endif /* SECBOOT_USE_LOCAL_LOADER */

#if (((SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION)) && (!defined(SFU_MPU_PROTECT_ENABLE)))
  if (SFU_COM_DeInit() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }
#endif /* ((SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION)) && (!defined(SFU_MPU_PROTECT_ENABLE)) */

  if (SFU_LL_DeInit() != SFU_SUCCESS)
  {
    return SFU_ERROR;
  }

  return SFU_SUCCESS;
}

/**
  * @brief  BSP Initialization.
  *         Called when the secure mode is enabled.
  * @note   The BSP configuration should be handled only in this function.
  * @param  None
  * @retval None
  */
static void SFU_BOOT_BspConfiguration()
{
  /* LED Init*/
  (void) BSP_LED_Init(SFU_STATUS_LED);

#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
  /* User Button */
  BUTTON_INIT();
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
}

/**
  * @brief  Execute the Secure Boot state machine.
  * @param  None
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus SFU_BOOT_SM_Run(void)
{
  SFU_ErrorStatus  e_ret_status = SFU_SUCCESS;
  void (*fnStateMachineFunction)(void);
  static void (* fnStateMachineTable[])(void) = {SFU_BOOT_SM_CheckStatusOnReset,
#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
                                                 SFU_BOOT_SM_CheckNewFwToDownload,
                                                 SFU_BOOT_SM_DownloadNewUserFw,
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
                                                 SFU_BOOT_SM_CheckUserFwStatus,
                                                 SFU_BOOT_SM_InstallNewUserFw,
                                                 SFU_BOOT_SM_VerifyUserFwSignature,
                                                 SFU_BOOT_SM_ExecuteUserFw,
                                                 SFU_BOOT_SM_ResumeInstallNewUserFw,
                                                 SFU_BOOT_SM_RollbackInstallPrevUserFw,
                                                 SFU_BOOT_SM_HandleCriticalFailure,
                                                 SFU_BOOT_SM_RebootStateMachine
                                                };

  /* Start the State Machine loop/evolution */
  while (e_ret_status == SFU_SUCCESS)
  {
    /* Always execute a security/safety check before moving to the next state */
    if (SFU_BOOT_SecuritySafetyCheck() == SFU_SUCCESS)
    {
      /* Get the right StateMachine function according to the current state */
      fnStateMachineFunction = fnStateMachineTable[(uint8_t)m_StateMachineContext.CurrState];

      /* Call the State Machine function associated to the current state */
      fnStateMachineFunction();
    }
    else
    {
      e_ret_status = SFU_ERROR;
    }
  }

  /* If the State Machine cannot evolve anymore, reboot is the only option */

  /* Set the error before forcing a reboot */
  SFU_EXCPT_SetError(SFU_EXCPT_UNKNOWN);

  /* This is the last operation executed. Force a System Reset */
  SFU_BOOT_ForceReboot();

  return e_ret_status;
}

/** @brief  Check the Reset status in order to understand the last cause of Reset
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_CheckStatusOnReset(void)
{
  TRACE("\r\n= [SBOOT] STATE: CHECK STATUS ON RESET");

  /* Check the wakeup sources */
  SFU_BOOT_ManageResetSources();

  /* RCC access done to identify the wakeup sources, we can switch to unpriviledged */
  SFU_MPU_EnterUnprivilegedMode();
#ifdef SFU_TEST_PROTECTION
  /*
   * All protections are installed and the correct execution mode is set (unprivileged).
   * This is the good time to run the auto-tests.
   */
  SFU_TEST_Init();
#endif /* SFU_TEST_PROTECTION */

#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
  /* When the local loader feature is supported we need to check if a local download is requested */
  SFU_SET_SM_CURR_STATE(SFU_STATE_CHECK_NEW_FW_TO_DOWNLOAD);
#else
  /* When the local loader feature is disabled go directly to the check of the FW status */
  SFU_SET_SM_CURR_STATE(SFU_STATE_VERIFY_USER_FW_STATUS);
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
}

#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
/**
  * @brief  Check if a new UserApp firmware is available for downloading.
  *         When entering this state from SFU_STATE_CHECK_STATUS_ON_RESET (initialDeviceStatusCheck=1) it is required
  *         to press the user button to force the local download.
  *         When entering this state from SFU_STATE_VERIFY_USER_FW_STATUS the local download is awaited automatically
  *         (because there is no other action to do).
  * @param  None
  * @note   This function must set the next State Machine State.
  * @retval None
  */
static void SFU_BOOT_SM_CheckNewFwToDownload(void)
{
  SFU_ErrorStatus  e_ret_status = SFU_ERROR;
#if (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)

  if (STANDALONE_LOADER_STATE == STANDALONE_LOADER_DWL_REQ)
  {
    TRACE("\r\n= [SBOOT] STATE: execution standalone loader");
    e_ret_status = SFU_BOOT_LaunchStandaloneLoader();

    /* This is unreachable code (dead code) in principle...
    At this point we should not be able to reach the following instructions.
    If we can execute them a critical issue has occurred.. So set the next State Machine accordingly */
    SFU_SET_SM_IF_CURR_STATE(e_ret_status, SFU_STATE_HANDLE_CRITICAL_FAILURE, SFU_STATE_HANDLE_CRITICAL_FAILURE);
    return;
  }
#endif /* (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */

  if (initialDeviceStatusCheck == 1U)
  {
    /* At boot-up, before checking the FW status, a local download can be forced thanks to the user button */
    TRACE("\r\n= [SBOOT] STATE: CHECK NEW FIRMWARE TO DOWNLOAD");
    if (0U != BUTTON_PUSHED())
    {
      /* Download requested */
      e_ret_status = SFU_SUCCESS;
    }
    else
    {
      e_ret_status = SFU_ERROR;
    }
  }
  else
  {
    /*
     * The FW status has already been checked and no FW can be launched: no need to check the trigger, wait for a local
     * download to start
     */
    e_ret_status = SFU_SUCCESS;
  }

  /* Set the next State Machine state according to the success of the failure of e_ret_status */
  SFU_SET_SM_IF_CURR_STATE(e_ret_status, SFU_STATE_DOWNLOAD_NEW_USER_FW, SFU_STATE_VERIFY_USER_FW_STATUS);

}
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */


/**
  * @brief  Check the Status of the Fw Image to work on  in order to set the next
  *         State Machine state accordingly
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_CheckUserFwStatus(void)
{
#if (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
#endif /* (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
  SFU_IMG_ImgInstallStateTypeDef e_PendingInstallStatus;
  uint32_t i;


  if (initialDeviceStatusCheck == 1U)
  {
    TRACE("\r\n= [SBOOT] STATE: CHECK USER FW STATUS");
  }

#if (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
  if (STANDALONE_LOADER_STATE == STANDALONE_LOADER_INSTALL_REQ)
  {
    /* Read header in dwl slot */
    e_ret_status = SFU_LL_FLASH_Read((uint8_t *) &fw_image_header_validated, (uint8_t *) SlotStartAdd[SLOT_DWL_1],
                                     sizeof(SE_FwRawHeaderTypeDef));
    if (e_ret_status == SFU_SUCCESS)
    {
      /*
       * Notify the Secure Boot that a new image has been downloaded
       */
      e_ret_status = SFU_IMG_InstallAtNextReset((uint8_t *) &fw_image_header_validated);
    }
#if defined(SFU_VERBOSE_DEBUG_MODE)
    if (e_ret_status == SFU_ERROR)
    {
      TRACE("\r\n\t  Cannot memorize that a new image has been downloaded.");
    }
#endif /* SFU_VERBOSE_DEBUG_MODE */

    /* In all cases, standard SBSFU start-up at next reset */
    STANDALONE_LOADER_STATE = STANDALONE_LOADER_NO_REQ;
  }
#endif /* SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER */

  /* Check if there is a pending action related to a FW update procedure */
  e_PendingInstallStatus = SFU_IMG_CheckPendingInstallation(&m_DwlSlotToInstall, &m_ActiveSlotToResume,
                                                            &m_ActiveSlotToRollback);

  switch (e_PendingInstallStatus)
  {
    case SFU_IMG_FWIMAGE_ROLLBACK:
      /*
       * The new FW image has not been validated : rollback requested @ next reset
       * Rollback is done assuming the previous image was backed-up in corresponding dwl slot
       * As example : new image installed in SLOT_ACTIVE_2. Previous image was backed-up in SLOT_DWL_2
       */
      /* Identify corresponding backed-up slot */
      m_DwlSlotToInstall = m_ActiveSlotToRollback - SLOT_ACTIVE_1 + SLOT_DWL_1;
      TRACE("\r\n\t  Installation not validated: rollback procedure initiated (SLOT_ACTIVE_%d / SLOT_DWL_%d)",
            m_ActiveSlotToRollback, m_DwlSlotToInstall - SLOT_DWL_1 + 1U);
      SFU_SET_SM_CURR_STATE(SFU_STATE_ROLLBACK_PREV_USER_FW);
      break;

    case SFU_IMG_FWUPDATE_STOPPED:
      /* The installation of a downloaded FW has been interrupted */
      /* We perform a resume of the installation */
      TRACE("\r\n\t  Installation Failed: resume installation procedure initiated (SLOT_ACTIVE_%d / SLOT_DWL_%d)",
            m_ActiveSlotToResume, m_DwlSlotToInstall - SLOT_DWL_1 + 1U);
      SFU_SET_SM_CURR_STATE(SFU_STATE_RESUME_INSTALL_NEW_USER_FW);
      break;

    case SFU_IMG_FWIMAGE_TO_INSTALL:
      /*
       * A new FW is detected in the dwl slot and ready to be installed
       */
      TRACE("\r\n\t  New Fw to be installed from slot SLOT_DWL_%d", m_DwlSlotToInstall - SLOT_DWL_1 + 1U);
      SFU_SET_SM_CURR_STATE(SFU_STATE_INSTALL_NEW_USER_FW);
      break;

    case SFU_IMG_NO_FWUPDATE:
      /*
       * No FW image installation pending :
       *   1- Priority to MASTER_SLOT : check if a firmware is detected
       *   2- No firmware identified : verify other slots and start execution on the last detected firmware
       *   3- No active firmware candidate for execution ==> Local download
       *
       * This strategy can be adapted by removing for example execution of step2 ==> focus only on MASTER_SLOT.
       */
      m_ActiveSlotToExecute = 0U;

      /* 1- Priority to MASTER_SLOT : check if a firmware is detected */
      if (MASTER_SLOT != 0xFFU)
      {
        if (SFU_SUCCESS == SFU_IMG_DetectFW(MASTER_SLOT))
        {
          m_ActiveSlotToExecute = MASTER_SLOT;
          TRACE("\r\n\t  A FW is detected in the slot SLOT_ACTIVE_%d", m_ActiveSlotToExecute);
          SFU_SET_SM_CURR_STATE(SFU_STATE_VERIFY_USER_FW_SIGNATURE);
        }
      }

      /* 2- No firmware identified : verify other slots and start execution on the last detected firmware */
      if (m_ActiveSlotToExecute == 0U)
      {
        for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
        {
          if (SlotStartAdd[SLOT_ACTIVE_1 + i] != 0U)         /* Slot configured ? */
          {
            if (SFU_SUCCESS == SFU_IMG_DetectFW(SLOT_ACTIVE_1 + i))
            {
              m_ActiveSlotToExecute = SLOT_ACTIVE_1 + i;
              TRACE("\r\n\t  A FW is detected in the slot SLOT_ACTIVE_%d", m_ActiveSlotToExecute);
              SFU_SET_SM_CURR_STATE(SFU_STATE_VERIFY_USER_FW_SIGNATURE);
            }
          }
        }
      }

      /* 3- No active firmware candidate for execution ==> Local download */
      if (m_ActiveSlotToExecute == 0U)
      {
        /* Control if all active slot are empty */
        for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
        {
          if (SlotStartAdd[SLOT_ACTIVE_1 + i] != 0U)       /* Slot configured ? */
          {
            if (SFU_IMG_VerifyEmptyActiveSlot(SLOT_ACTIVE_1 + i) != SFU_SUCCESS)
            {
              /*
               * We should never reach this code.
               * Could come from an attack ==> as an example we invalidate current firmware.
               */
              TRACE("\r\n\t  Slot SLOT_ACTIVE_%d not empty : erasing ...", SLOT_ACTIVE_1 + i);
              (void)SFU_IMG_InvalidateCurrentFirmware(SLOT_ACTIVE_1 + i); /* If this fails we continue anyhow */
            }
          }
        }

        /*
         * No valid FW is present in the active slot
         * and there is no FW to be installed in UserApp download area: local download (when possible)
         */
        if (1U == initialDeviceStatusCheck)
        {
          TRACE("\r\n\t  No valid FW found in the active slots nor new FW to be installed");
#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
          /* Waiting for a local download is automatic, no trigger required. */
          TRACE("\r\n\t  Waiting for the local download to start... ");
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
          initialDeviceStatusCheck = 0U;
#ifdef SFU_TEST_PROTECTION
          SFU_TEST_Reset();
#endif /* SFU_TEST_PROTECTION */
        }
#if defined(SFU_VERBOSE_DEBUG_MODE)
        else
        {
          /*
           * No ELSE branch (except for verbose debug), because the FW status is checked only once per boot:
           *   If a FW is present in the active slot => it is checked then launched.
           *   If there is a FW to install => the installation procedure starts.
           *   If no FW is present and no installation is pending:
           *      - if the local loader feature is enabled we enter the local download state
           *      - if the local loader feature is disabled, the execution is stopped.
           */
          TRACE("\r\n\t  Abnormal case: SFU_STATE_VERIFY_USER_FW_STATUS should not be entered more than once per boot.");
        }
#endif /* SFU_VERBOSE_DEBUG_MODE */
#if (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
        SFU_SET_SM_CURR_STATE(SFU_STATE_CHECK_NEW_FW_TO_DOWNLOAD);
#else
        /*
         * When the local loader feature is disabled it is not possible to enter the local download state.
         * Rebooting automatically or keeping on checking the FW status would not necessarily be better.
         * So we end up waiting for the user to reboot (or the IWDG to expire).
         */
        TRACE("No valid FW and no local loader: execution stopped.\r\n");
        while (1 == 1)
        {
          (void) BSP_LED_Toggle(SFU_STATUS_LED);
          HAL_Delay(SFU_STOP_NO_FW_BLINK_DELAY);
        }
#endif /* (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER) || (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */
      }
      break;

    default:
      TRACE("\r\n\t  Flash State Unknown, Critical failure");
      /* If not in one of the previous state, something bad occurred */
      SFU_SET_SM_CURR_STATE(SFU_STATE_HANDLE_CRITICAL_FAILURE);
      break;
  }
}


#if (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
/**
  * @brief  A new UserApp Fw was available. Start to download it
  * @param  None
  * @note   Reset is generated at by standalone loader when FW is downloaded.
  * @retval None
  */
static void SFU_BOOT_SM_DownloadNewUserFw(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  TRACE("\r\n= [SBOOT] STATE: DOWNLOAD NEW USER FIRMWARE");

  /* Jump into standalone loader */
  STANDALONE_LOADER_STATE = STANDALONE_LOADER_DWL_REQ;
  e_ret_status = SFU_BOOT_LaunchStandaloneLoader();

  /* This is unreachable code (dead code) in principle...
  At this point we should not be able to reach the following instructions.
  If we can execute them a critical issue has occurred.. So set the next State Machine accordingly */
  SFU_SET_SM_IF_CURR_STATE(e_ret_status, SFU_STATE_HANDLE_CRITICAL_FAILURE, SFU_STATE_HANDLE_CRITICAL_FAILURE);
}
#elif (SECBOOT_LOADER == SECBOOT_USE_LOCAL_LOADER)
/**
  * @brief  A new UserApp Fw was available. Start to download it
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_DownloadNewUserFw(void)
{
  SFU_ErrorStatus           e_ret_status = SFU_ERROR;
  SFU_LOADER_StatusTypeDef  e_ret_status_app = SFU_LOADER_ERR_COM;
  uint32_t                  dwl_slot;
  uint32_t                  u_size = 0;

  TRACE("\r\n= [SBOOT] STATE: DOWNLOAD NEW USER FIRMWARE");

  /*
   * Download area will be chosen as following. After header analysis :
   *   - same DWL slot number as SFU magic number, if the slot is configured.
   *     As example, we will choose SLOT_DWL_2 for SFU2 magic in the FW header
   *   - by default : SLOT_DWL_1
   */
  e_ret_status = SFU_LOADER_DownloadNewUserFw(&e_ret_status_app, &dwl_slot, &u_size);
  if (e_ret_status == SFU_SUCCESS)
  {
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\r\n\t  FwSize=%d | PartialFwSize=%d | PartialFwOffset=%d | %d bytes received",
          fw_image_header_validated.FwSize, fw_image_header_validated.PartialFwSize,
          fw_image_header_validated.PartialFwOffset, u_size);
#endif /* SFU_VERBOSE_DEBUG_MODE */

    /* Read header in dwl slot */
    e_ret_status = SFU_LL_FLASH_Read((uint8_t *) &fw_image_header_validated, (uint8_t *) SlotStartAdd[dwl_slot],
                                     sizeof(SE_FwRawHeaderTypeDef));
  }

  if (e_ret_status == SFU_SUCCESS)
  {
    /*
     * Notify the Secure Boot that a new image has been downloaded
     * by calling the SE interface function to trigger the installation procedure at next reboot.
     */
    if (SFU_IMG_InstallAtNextReset((uint8_t *) &fw_image_header_validated) != SFU_SUCCESS)
    {
      /* Erase downloaded image */
      (void) SFU_IMG_EraseDownloadedImg(dwl_slot);

      /* no specific error cause set */
#if defined(SFU_VERBOSE_DEBUG_MODE)
      TRACE("\r\n\t  Cannot memorize that a new image has been downloaded.");
#endif /* SFU_VERBOSE_DEBUG_MODE */
    }
  }
  else
  {
    /* Erase downloaded image */
    (void) SFU_IMG_EraseDownloadedImg(dwl_slot);

    /* Memorize the specific error cause if any before handling this critical failure */
    switch (e_ret_status_app)
    {
      case SFU_LOADER_ERR_COM:
        SFU_EXCPT_SetError(SFU_EXCPT_COM_ERR);
        break;
      case SFU_LOADER_ERR_FW_VERSION:
        SFU_EXCPT_SetError(SFU_EXCPT_VERSION_ERR);
        break;
      case SFU_LOADER_ERR_FW_LENGTH:
        SFU_EXCPT_SetError(SFU_EXCPT_FW_TOO_BIG);
        break;
      case SFU_LOADER_ERR_AUTH_FAILED:
        SFU_EXCPT_SetError(SFU_EXCPT_HEADER_AUTH_ERR);
        break;
      case SFU_LOADER_ERR_FLASH:
        SFU_EXCPT_SetError(SFU_EXCPT_FLASH_ERR);
        break;
      case SFU_LOADER_ERR_CRYPTO:
        SFU_EXCPT_SetError(SFU_EXCPT_DECRYPT_ERR);
        break;
      default:
        /* no specific error cause */
        break;
    }
  } /* else error with no specific error cause */
  /* Set the next State Machine state according to the success of the failure of e_ret_status */
  SFU_SET_SM_IF_CURR_STATE(e_ret_status, SFU_STATE_REBOOT_STATE_MACHINE, SFU_STATE_HANDLE_CRITICAL_FAILURE);
}
#endif /* SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER */

/**
  * @brief  Install the new UserApp Fw
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_InstallNewUserFw(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  TRACE("\r\n= [SBOOT] STATE: INSTALL NEW USER FIRMWARE ");

  /* Check the candidate version vs current active version */
  e_ret_status = SFU_IMG_CheckCandidateVersion(m_DwlSlotToInstall);

  if (SFU_SUCCESS != e_ret_status)
  {
    /* Erase downloaded FW in case of authentication/integrity error */
    (void) SFU_IMG_EraseDownloadedImg(m_DwlSlotToInstall);
  }
  else
  {
    /* Secure coding : double check the candidate version vs current active version */
    e_ret_status = SFU_IMG_CheckCandidateVersion(m_DwlSlotToInstall);
  }

  if (SFU_SUCCESS == e_ret_status)
  {
    /* Launch the Firmware Installation procedure */
    e_ret_status = SFU_IMG_TriggerImageInstallation(m_DwlSlotToInstall);
  }

  if (SFU_SUCCESS == e_ret_status)
  {
    /*
     * The FW installation succeeded: the previous FW is now backed up in the dwl slot.
     * Nothing more to do, in the next FSM state we are going to verify the FW again and execute it if possible.
     */
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\r\n\t  FW installation succeeded.");
#endif /* SFU_VERBOSE_DEBUG_MODE */
  }
  else
  {
    /*
     * The FW installation failed: we need to reboot and the resume procedure will be triggered at next boot.
     * Nothing more to do, the next FSM state (HANDLE_CRITICAL_FAILURE) will deal with it.
     */
#if defined(SFU_VERBOSE_DEBUG_MODE)
    TRACE("\r\n\t  FW installation failed!");
#endif /* SFU_VERBOSE_DEBUG_MODE */
  }

  /* Installation successful : reboot for next operations on other slots
       - installation
       - resume
       - rollback */
  if (SFU_SUCCESS == e_ret_status)
  {
    SFU_BOOT_ForceReboot();
  }
  else
  {
    SFU_SET_SM_CURR_STATE(SFU_STATE_HANDLE_CRITICAL_FAILURE);
  }
}

/**
  * @brief  Execute a UserApp Fw installation resume
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_ResumeInstallNewUserFw(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  TRACE("\r\n= [SBOOT] STATE: RESUME INSTALLATION OF NEW USER FIRMWARE");

  /*
   * This resume installation procedure continue installation of new User FW in the active slot
   */
  e_ret_status = SFU_IMG_TriggerResumeInstallation(m_ActiveSlotToResume, m_DwlSlotToInstall);

  /* Installation successful : reboot for next operations on other slots
       - installation
       - resume
       - rollback */
  if (SFU_SUCCESS == e_ret_status)
  {
    SFU_BOOT_ForceReboot();
  }
  else
  {
    /* No specific error cause managed here because the FSM state already provides the information. */
    SFU_SET_SM_CURR_STATE(SFU_STATE_HANDLE_CRITICAL_FAILURE);
  }
}

/**
  * @brief  Execute a rollback to the previous UserApp Fw
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_RollbackInstallPrevUserFw(void)
{
#if defined(ENABLE_IMAGE_STATE_HANDLING) && !defined(SFU_NO_SWAP)
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  TRACE("\r\n= [SBOOT] STATE: ROLLBACK INSTALLATION TO PREVIOUS USER FIRMWARE");

  /*
   * Rollack installation to the previous User FW from Dwl_Slot
   */
  e_ret_status = SFU_IMG_TriggerRollbackInstallation(m_ActiveSlotToRollback, m_DwlSlotToInstall);

  /* Installation successful : reboot for next operations on other slots
       - installation
       - resume
       - rollback */
  if (SFU_SUCCESS == e_ret_status)
  {
    SFU_BOOT_ForceReboot();
  }
  else
  {
    /* No specific error cause managed here because the FSM state already provides the information. */
    SFU_SET_SM_CURR_STATE(SFU_STATE_HANDLE_CRITICAL_FAILURE);
  }
#else
  TRACE("\r\n= [SBOOT] STATE: ROLLBACK NOT SUPPORTED");

  /* No specific error cause managed here because the FSM state already provides the information. */
  SFU_SET_SM_CURR_STATE(SFU_STATE_HANDLE_CRITICAL_FAILURE);
#endif /* ENABLE_IMAGE_STATE_HANDLING && !(SFU_NO_SWAP) */
}


/**
  * @brief  Verify the UserApp Fw signature before executing it
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_VerifyUserFwSignature(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
  uint32_t i;

  TRACE("\r\n= [SBOOT] STATE: VERIFY USER FW SIGNATURE");


  /*
   * With the 2 images handling:
   *       1. the header signature is verified  when installing a new firmware
   *       2. the firmware signature is checked when installing a new firmware
   *       3. remaining part of active slot is kept "clean" during installation procedure
   * So following checks should never fail.
   */
  /* Check all active slots configured */
  for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
  {
    /* Slot configured ? */
    if (SlotStartAdd[SLOT_ACTIVE_1 + i] != 0U)
    {
      /* FW installed ? */
      if (SFU_SUCCESS == SFU_IMG_DetectFW(SLOT_ACTIVE_1 + i))
      {
        /* Initialize Flow control */
        FLOW_CONTROL_INIT(uFlowCryptoValue, FLOW_CTRL_INIT_VALUE);

        /* Check the header signature */
        e_ret_status = SFU_IMG_VerifyActiveImgMetadata(SLOT_ACTIVE_1 + i);
        if (SFU_SUCCESS == e_ret_status)
        {
          /* Check the FW signature */
          e_ret_status = SFU_IMG_VerifyActiveImg(SLOT_ACTIVE_1 + i);
          if (SFU_SUCCESS == e_ret_status)
          {
            /* Verify that there is no additional code beyond firmware image */
            e_ret_status = SFU_IMG_VerifyActiveSlot(SLOT_ACTIVE_1 + i);
            if (SFU_SUCCESS != e_ret_status)
            {
#if defined(SFU_VERBOSE_DEBUG_MODE)
              TRACE("\r\n\t  Unexpected code beyond FW image in slot SLOT_ACTIVE_%d", SLOT_ACTIVE_1 + i);
#endif /* SFU_VERBOSE_DEBUG_MODE */
            }
          }
          else
          {
#if defined(SFU_VERBOSE_DEBUG_MODE)
            TRACE("\r\n\t  Firmware signature verification failure in slot SLOT_ACTIVE_%d", SLOT_ACTIVE_1 + i);
#endif /* SFU_VERBOSE_DEBUG_MODE */
          }
        }
        else
        {
#if defined(SFU_VERBOSE_DEBUG_MODE)
          TRACE("\r\n\t  Header signature verification failure in slot SLOT_ACTIVE_%d", SLOT_ACTIVE_1 + i);
#endif /* SFU_VERBOSE_DEBUG_MODE */
        }
        /*
         * One of the checks fails :
         *  - Header signature
         *  - FW signature
         *  - No malicious code
         *  ==> Active slot should be invalidated
         */
        if (SFU_SUCCESS != e_ret_status)
        {
          /*
           * We should never reach this code.
           * Could come from an attack ==> as an example we invalidate current firmware.
           */
#if defined(SFU_VERBOSE_DEBUG_MODE)
          TRACE("\r\n\t  Erasing slot SLOT_ACTIVE_%d", SLOT_ACTIVE_1 + i);
#endif /* SFU_VERBOSE_DEBUG_MODE */
          (void)SFU_IMG_InvalidateCurrentFirmware(SLOT_ACTIVE_1 + i); /* If this fails we continue anyhow */
        }
        else
        {
          /* Verify if authentication and integrity controls performed */
          FLOW_CONTROL_CHECK(uFlowCryptoValue, FLOW_CTRL_INTEGRITY);
        }
      }
      else
      {
        if (SFU_IMG_VerifyEmptyActiveSlot(SLOT_ACTIVE_1 + i) != SFU_SUCCESS)
        {
          /*
           * We should never reach this code.
           * Could come from an attack ==> as an example we invalidate current firmware.
           */
#if defined(SFU_VERBOSE_DEBUG_MODE)
          TRACE("\r\n\t  Slot SLOT_ACTIVE_%d not empty : erasing ...", SLOT_ACTIVE_1 + i);
#endif /* SFU_VERBOSE_DEBUG_MODE */
          (void)SFU_IMG_InvalidateCurrentFirmware(SLOT_ACTIVE_1 + i); /* If this fails we continue anyhow */
        }
      }
    }
  }

  /* Set the next State Machine state according to the success of the failure of e_ret_status */
  SFU_SET_SM_IF_CURR_STATE(e_ret_status, SFU_STATE_EXECUTE_USER_FW, SFU_STATE_HANDLE_CRITICAL_FAILURE);
}

/**
  * @brief  Exit from the SB/SFU State Machine and try to execute the UserApp Fw
  * @param  None
  * @note   This function must set the next State Machine State
  * @retval None
  */
static void SFU_BOOT_SM_ExecuteUserFw(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
  SE_StatusTypeDef e_se_status = SE_KO;
  uint32_t i;

  TRACE("\r\n= [SBOOT] STATE: EXECUTE USER FIRMWARE");

  /* Reload Watchdog */
  (void) SFU_LL_SECU_IWDG_Refresh();

  /* Verify if authentication and integrity controls performed at SFU_STATE_VERIFY_USER_FW_SIGNATURE */
  FLOW_CONTROL_CHECK(uFlowCryptoValue, FLOW_CTRL_INTEGRITY);

  /* Double security check for all active slots :
     - Control twice the Header signature will avoid basic hardware attack
     - Control twice the FW signature will avoid basic hardware attack */

  /* Check all active slots configured */
  for (i = 0U; i < SFU_NB_MAX_ACTIVE_IMAGE; i++)
  {
    /* Slot configured ? */
    if (SlotStartAdd[SLOT_ACTIVE_1 + i] != 0U)
    {
      /* FW installed ? */
      if (SFU_SUCCESS == SFU_IMG_DetectFW(SLOT_ACTIVE_1 + i))
      {
        /* Initialize Flow control */
        FLOW_CONTROL_INIT(uFlowCryptoValue, FLOW_CTRL_INIT_VALUE);

        /* Check the header signature */
        if (SFU_IMG_VerifyActiveImgMetadata(SLOT_ACTIVE_1 + i) != SFU_SUCCESS)
        {
          /* Security issue : execution stopped ! */
          SFU_EXCPT_Security_Error();
        }

        /* Check the FW signature */
        if (SFU_IMG_ControlActiveImgTag(SLOT_ACTIVE_1 + i) != SFU_SUCCESS)
        {
          /* Security issue : execution stopped ! */
          SFU_EXCPT_Security_Error();
        }

#if defined(ENABLE_IMAGE_STATE_HANDLING) && !defined(SFU_NO_SWAP)
        /* Move the state to SELFTEST for the new images */
        if (SFU_IMG_UpdateImageState(SLOT_ACTIVE_1 + i) != SFU_SUCCESS)
        {
          /* Image state cannot be changed : What to do ?
             ==> decision to continue execution */
          TRACE("\r\n= [FWIMG] WARNING: IMAGE STATE CANNOT BE CHANGED!");
        }
#endif /* ENABLE_IMAGE_STATE_HANDLING  && !(SFU_NO_SWAP) */

        /* Verify if authentication and integrity controls performed */
        FLOW_CONTROL_CHECK(uFlowCryptoValue, FLOW_CTRL_INTEGRITY);
      }
    }
  }

  /*
   * You may decide to implement additional checks before running the Firmware.
   * For the time being we launch the FW present in the active slot.
   *
   * The bootloader must also take care of the security aspects:
   *   A.configure (if any) the external flash in execution mode with On The Fly DECryption (OTFDEC)
   *   B.lock the SE services the UserApp is not allowed to call
   *   C.leave secure boot mode
   */

  /* Configure active slot in execution mode with OTFDEC (if any) : required in case of external flash */
  e_ret_status = SFU_LL_FLASH_Config_Exe(m_ActiveSlotToExecute);
  if (e_ret_status != SFU_SUCCESS)
  {
    /* Set the error before forcing a reboot, don't care of return value as followed by reboot */
    SFU_EXCPT_SetError(SFU_EXCPT_FLASH_CFG_ERR);

    /* This is the last operation executed. Force a System Reset */
    SFU_BOOT_ForceReboot();
  }

  if (e_ret_status == SFU_SUCCESS)
  {
    /* Lock part of Secure Engine services */
    if (SE_LockRestrictServices(&e_se_status) == SE_SUCCESS)
    {
      /* Double instruction to avoid basic fault injection */
      if (SE_LockRestrictServices(&e_se_status) == SE_SUCCESS)
      {
        /* De-initialize the SB_SFU bootloader before launching the UserApp */
        (void)SFU_BOOT_DeInit(); /* the return value is not checked, we will always try launching the UserApp */

        /* Last flow control : lock service */
        FLOW_CONTROL_STEP(uFlowCryptoValue, FLOW_STEP_LOCK_SERVICE, FLOW_CTRL_LOCK_SERVICE);

        /* This function should not return */
        e_ret_status = SFU_IMG_LaunchActiveImg(m_ActiveSlotToExecute);

        /* This point should not be reached */
#if defined(SFU_VERBOSE_DEBUG_MODE)
        /* We do not memorize any specific error, the FSM state is already providing the info */
        TRACE("\r\n\t  SFU_IMG_LaunchActiveImg(SLOT_ACTIVE_%d) failure!", m_ActiveSlotToExecute);
#endif /* SFU_VERBOSE_DEBUG_MODE */
        while (1 == 1)
        {
          ;    /* wait for watchdog */
        }
      }
      else
      {
        /* Set the error before forcing a reboot, don't care of return value as followed by reboot */
        SFU_EXCPT_SetError(SFU_EXCPT_LOCK_SE_SERVICES_ERR);

        /* This is the last operation executed. Force a System Reset */
        SFU_BOOT_ForceReboot();
      }
    }
    else
    {
      /* Set the error before forcing a reboot, don't care of return value as followed by reboot */
      SFU_EXCPT_SetError(SFU_EXCPT_LOCK_SE_SERVICES_ERR);

      /* This is the last operation executed. Force a System Reset */
      SFU_BOOT_ForceReboot();
    }
  }

  /* This is unreachable code (dead code) in principle...
  At this point we should not be able to reach the following instructions.
  If we can execute them a critical issue has occurred.. So set the next State Machine accordingly */
  SFU_SET_SM_IF_CURR_STATE(e_ret_status, SFU_STATE_HANDLE_CRITICAL_FAILURE, SFU_STATE_HANDLE_CRITICAL_FAILURE);
}

/**
  * @brief  Manage a Critical Failure occurred during the evolution of the State Machine
  * @param  None
  * @note   After a Critical Failure a Reboot will be called.
  * @retval None
  */
static void SFU_BOOT_SM_HandleCriticalFailure(void)
{
  TRACE("\r\n= [SBOOT] STATE: HANDLE CRITICAL FAILURE");

  /* It's not possible to continue without compromising the stability or the security of the solution.
     The State Machine needs to be aborted and a Reset must be triggered */
  SFU_SET_SM_IF_CURR_STATE(SFU_ERROR, SFU_STATE_REBOOT_STATE_MACHINE, SFU_STATE_REBOOT_STATE_MACHINE);
}

/**
  * @brief  The state machine is aborted and a Reset is triggered
  * @param  None
  * @note   You are in this condition because it's not possible to continue without
            compromising the stability or the security of the solution.
  * @retval None
  */
static void SFU_BOOT_SM_RebootStateMachine(void)
{
  TRACE("\r\n= [SBOOT] STATE: REBOOT STATE MACHINE");

  /*
   * In case some clean-up must be done before resetting.
   * Please note that at the moment this function does not clean-up the RAM used by SB_SFU.
   */
  (void)SFU_BOOT_DeInit();

  /* This is the last operation executed. Force a System Reset */
  SFU_BOOT_ForceReboot();
}

#if (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER)
/**
  * @brief  Jump into standalone loader
  * @param  None
  * @retval None
  */
static SFU_ErrorStatus SFU_BOOT_LaunchStandaloneLoader(void)
{
  SE_StatusTypeDef e_se_status = SE_KO;


  /* Lock part of Secure Engine services */
  if (SE_LockRestrictServices(&e_se_status) == SE_SUCCESS)
  {
    /* Double instruction to avoid basic fault injection */
    if (SE_LockRestrictServices(&e_se_status) == SE_SUCCESS)
    {
      /* this function does not return */
      SFU_MPU_SysCall((uint32_t)SB_SYSCALL_LAUNCH_APP, LOADER_REGION_ROM_START);

      /* The point below should NOT be reached */
      return SFU_SUCCESS;
    }
    else
    {
      return SFU_ERROR;
    }
  }
  else
  {
    return SFU_ERROR;
  }
}
#endif /* (SECBOOT_LOADER == SECBOOT_USE_STANDALONE_LOADER) */

/**
  * @brief  Check (and Apply when possible) the security/safety/integrity protections.
  *         The "Apply" part depends on @ref SECBOOT_OB_DEV_MODE and @ref SFU_PROTECT_RDP_LEVEL.
  * @param  None
  * @note   This operation should be done as soon as possible after a reboot.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus SFU_BOOT_CheckApplySecurityProtections(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Apply Static protections involving Option Bytes */
  if (SFU_LL_SECU_CheckApplyStaticProtections() == SFU_SUCCESS)
  {
    /* Apply runtime protections needed to be enabled after each Reset */
    e_ret_status = SFU_LL_SECU_CheckApplyRuntimeProtections(SFU_INITIAL_CONFIGURATION);
  }

  return e_ret_status;
}

/**
  * @brief  System security configuration
  * @param  None
  * @note   Check and apply the security protections. This has to be done as soon
  *         as possible after a reset
  * @retval None
  */
static SFU_ErrorStatus SFU_BOOT_SystemSecurity_Config(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;
  /* WARNING: The following CheckApplySecurityProtection function must be called
     as soon as possible after a Reset in order to be sure the system is secured
     before starting any other operation. The drawback is that the ErrorManagement
     is not initialized yet, so in case of failure of this function, there will not be
     any error stored into the BootInfo or other visible effects.  */

  /* Very few things are already initialized at this stage. Need additional initialization
     to show a message that is added as below only in Debug/Test mode */
#if defined(SFU_DEBUG_MODE) || defined(SFU_TEST_PROTECTION)
  (void) SFU_COM_Init();
#endif /* SFU_DEBUG_MODE || SFU_TEST_PROTECTION */

  if (SFU_BOOT_CheckApplySecurityProtections() != SFU_SUCCESS)
  {
    /* WARNING: This might be generated by an attempted attack or a bug of your code!
       Add your code here in order to implement a custom action for this event,
       e.g. trigger a mass erase or take any other action in order to
       protect your system, or simply discard it if this is expected.
       ...
       ...
    */
    TRACE("\r\n= [SBOOT] System Security Check failed! Rebooting...");
  }
  else
  {
    TRACE("\r\n= [SBOOT] System Security Check successfully passed. Starting...");
    e_ret_status = SFU_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief  Periodic verification of applied protection mechanisms, in order to prevent
  *         a malicious code removing some of the applied security/integrity features.
  *         The IWDG is also refreshed in this function.
  * @param  None
  * @note   This function must be called with a frequency greater than 0.25Hz!
  *         Otherwise a Reset will occur. Once enabled the IWDG cannot be disabled
  *         So the User App should continue to refresh the IWDG counter.
  * @retval SFU_ErrorStatus SFU_SUCCESS if successful, SFU_ERROR otherwise.
  */
static SFU_ErrorStatus SFU_BOOT_SecuritySafetyCheck(void)
{
  SFU_ErrorStatus e_ret_status = SFU_ERROR;

  /* Refresh the IWDG */
  e_ret_status = SFU_LL_SECU_IWDG_Refresh();


  /* Add your code here for customization:
     e.g. for additional security or safety periodic check.
     ...
     ...
  */
  return e_ret_status;
}

/**
  * @brief  Manage the  the Reset sources, and if the case store the error for the next steps
  * @param  None
  * @retval None
  */
static void SFU_BOOT_ManageResetSources(void)
{
  SFU_RESET_IdTypeDef e_wakeup_source_id = SFU_RESET_UNKNOWN;

  /* Check the wakeup sources */
  SFU_LL_SECU_GetResetSources(&e_wakeup_source_id);
  switch (e_wakeup_source_id)
  {
      /*
       * Please note that the example of reset causes handling below is only a basic example to illustrate the way the
       * RCC_CSR flags can be used to do so.
       */

    case SFU_RESET_WDG_RESET:
      TRACE("\r\n\t  WARNING: A Reboot has been triggered by a Watchdog reset!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;

    case SFU_RESET_LOW_POWER:
      TRACE("\r\n\t  INFO: A Reboot has been triggered by a LowPower reset!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;

    case SFU_RESET_HW_RESET:
      TRACE("\r\n\t  INFO: A Reboot has been triggered by a Hardware reset!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;

    case SFU_RESET_BOR_RESET:
      TRACE("\r\n\t  INFO: A Reboot has been triggered by a BOR reset!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;

    case SFU_RESET_SW_RESET:
      TRACE("\r\n\t  INFO: A Reboot has been triggered by a Software reset!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;

    case SFU_RESET_OB_LOADER:
      TRACE("\r\n\t  WARNING: A Reboot has been triggered by an Option Bytes reload!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;

    default:
      TRACE("\r\n\t  WARNING: A Reboot has been triggered by an Unknown reset source!");
      /* WARNING: This might be generated by an attempted attack, a bug or your code!
         Add your code here in order to implement a custom action for this event,
         e.g. trigger a mass erase or take any other  action in order to
         protect your system, or simply discard it if this is expected.
         ...
         ...
      */
      break;
  }

  /* Once the reset sources has been managed and a possible error has been set, clear the reset sources */
  SFU_LL_SECU_ClearResetSources();
}

/**
  * @brief  Implement the Cube_Hal Callback generated on the Tamper IRQ.
  * @param  None
  * @retval None
  */
void SFU_CALLBACK_ANTITAMPER(RTC_HandleTypeDef *hrtc)
{
  UNUSED(hrtc);
  SFU_EXCPT_IrqExceptionHandler(SFU_EXCPT_TAMPERING_FAULT);
}

/**
  * @brief  Implement the Cube_Hal Callback generated on the Memory Fault.
  * @note  After a Memory Fault could not be possible to execute additional code
  * @param  None
  * @retval None
  */
void SFU_CALLBACK_MEMORYFAULT(void)
{
  SFU_EXCPT_IrqExceptionHandler(SFU_EXCPT_MEMORY_FAULT);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
