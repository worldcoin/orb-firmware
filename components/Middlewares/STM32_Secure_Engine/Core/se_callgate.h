/**
  ******************************************************************************
  * @file    se_callgate.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Engine CALLGATE module
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SE_CALLGATE_H
#define SE_CALLGATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"
#include "se_low_level.h"


/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @addtogroup SE_CALLGATE SE CallGate
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup SE_CALLGATE_Exported_Constants Exported Constants
  * @{
  */

/** @defgroup SE_CALLGATE_ID_Functions Secure Engine CallGate Function ID structure definition
  * @{
  */

/*Generic functions*/
#define SE_INIT_ID                                (0x00UL)    /*!< Secure Engine Init     */

/* CRYPTO Low level functions for bootloader only */
#define SE_CRYPTO_LL_DECRYPT_INIT_ID              (0x04UL)    /*!< CRYPTO Low level Decrypt_Init */
#define SE_CRYPTO_LL_DECRYPT_APPEND_ID            (0x05UL)    /*!< CRYPTO Low level Decrypt_Append */
#define SE_CRYPTO_LL_DECRYPT_FINISH_ID            (0x06UL)    /*!< CRYPTO Low level Decrypt_Finish */
#define SE_CRYPTO_LL_AUTHENTICATE_FW_INIT_ID      (0x07UL)    /*!< CRYPTO Low level Authenticate_FW_Init */
#define SE_CRYPTO_LL_AUTHENTICATE_FW_APPEND_ID    (0x08UL)    /*!< CRYPTO Low level Authenticate_FW_Append */
#define SE_CRYPTO_LL_AUTHENTICATE_FW_FINISH_ID    (0x09UL)    /*!< CRYPTO Low level Authenticate_FW_Finish */

/* CRYPTO High level functions for bootloader only */
#define SE_CRYPTO_HL_AUTHENTICATE_METADATA        (0x10UL)    /*!< CRYPTO High level Authenticate Metadata */

/* Next ranges are kept for future use (additional crypto schemes, additional user code) */
#define SE_APP_GET_ACTIVE_FW_INFO                 (0x20UL)    /*!< User Application retrieves an Active Firmware Info */
#define SE_APP_VALIDATE_FW                        (0x21UL)    /*!< User Application validates an Active Firmware */
#define SE_APP_GET_FW_STATE                       (0x22UL)    /*!< User Application retreives an Active Firmware state */

/* System configuration access (NVIC...) */
#define SE_SYS_SAVE_DISABLE_IRQ                   (0x60UL)    /*!< System command to disable IRQ, returning to caller IRQ configuration */
#define SE_SYS_RESTORE_ENABLE_IRQ                 (0x61UL)    /*!< System command to enable IRQ with given configuration */

/* SE IMG interface (bootloader only) */
#define SE_IMG_READ                               (0x92UL)    /*!< SFU reads a Flash protected area (bootloader only) */
#define SE_IMG_WRITE                              (0x93UL)    /*!< SFU write a Flash protected area (bootloader only) */
#define SE_IMG_ERASE                              (0x94UL)    /*!< SFU erase a Flash protected area (bootloader only) */
#define SE_IMG_GET_FW_STATE                       (0x95UL)    /*!< SFU Get Active Image State (bootloader only) */
#define SE_IMG_SET_FW_STATE                       (0x96UL)    /*!< SFU Set Active Image State (bootloader only) */

/* LOCK service to be used by the bootloader only */
#define SE_LOCK_RESTRICT_SERVICES                 (0x100UL)   /*!< SFU lock part of SE services (bootloader only) */

/* Configure "On The Fly DECryption" mechanism (OTFDEC) for external FLASH */
#define SE_EXTFLASH_DECRYPT_INIT                  (0x110UL)   /*!< SFU lock part of SE services (bootloader only) */

/* CM0 stack or FUS update process */
#define SE_CM0_UPDATE                             (0x120UL)   /*!< Wireless stack or FUS update managed by CM0 */

/* Secure Engine add-on middle wares */
#define SE_MW_ADDON_MSB_MASK     (0x70000000U)         /*!< SE add-ons MSB bits reserved for add-on middlewares IDs */
#define SE_MW_ADDON_KMS_MSB      (0x10000000U)         /*!< KMS services ID range begin */

/* Secure Engine Interrupts management */
#define SE_EXIT_INTERRUPT        (0x0001000UL)        /* !< Exit interrupt */

typedef uint32_t SE_FunctionIDTypeDef;                 /*!< Secure Engine CallGate Function ID structure definition */

/**
  * @}
  */

/**
  * @}
  */

/* External variables --------------------------------------------------------*/
/** @defgroup SE_CALLGATE_External_Variables External Variables
  * @{
  */

#if defined(SFU_ISOLATE_SE_WITH_FIREWALL) || defined(CKS_ENABLED)
/* Variable used to store Appli vector position */
extern uint32_t AppliVectorsAddr;
#endif /* (SFU_ISOLATE_SE_WITH_FIREWALL) || (CKS_ENABLED) */

#if defined(SFU_ISOLATE_SE_WITH_FIREWALL) && defined(IT_MANAGEMENT)
/* Variable used to store User Primask value */
extern uint32_t PrimaskValue;
/* Interrupt being handled by SE */
extern uint32_t IntHand;
/* Application Active Stack Pointer: 0 means MSP, 1 means PSP */
extern uint32_t AppliActiveSpMode;
/* Application Main Stack Pointer & Active Stack pointer */
extern uint32_t AppliMsp;
extern uint32_t AppliActiveSp;
/* SE Main Stack Pointer */
extern uint32_t SeMsp;
/* SP when entering SE IT Handler */
extern uint32_t SeExcEntrySp;
/* EXC_RETURN value in SE Handler Mode */
extern uint32_t SeExcReturn;
/* miscelleaneous register address */
extern uint32_t ScbVtorAddr;
extern uint32_t FirewallCrAddr;
/* set address of SE_UserHandlerWrapper: it is placed in the first entry of SE_IF_REGION_ROM_START */
extern uint32_t SE_UserHandlerWrapperAddr;
#endif /* SFU_ISOLATE_SE_WITH_FIREWALL && IT_MANAGEMENT*/
/* status to re-enter into SE_CallGate */
extern uint32_t SeCallGateStatusParam;
/**
  * @}
  */

/* Exported functions ------------------------------------------------------- */
/** @addtogroup SE_CALLGATE_Exported_Functions
  * @{
  */
SE_ErrorStatus SE_CallGate(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *const peSE_Status, uint32_t PrimaskValue, ...);

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

#ifdef __cplusplus
}
#endif

#endif /* SE_CALLGATE_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
