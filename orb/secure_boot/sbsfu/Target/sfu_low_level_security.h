/**
  ******************************************************************************
  * @file    sfu_low_level_security.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Secure Firmware Update security
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SFU_LOW_LEVEL_SECURITY_H
#define SFU_LOW_LEVEL_SECURITY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "sfu_fwimg_regions.h"
#include "sfu_def.h"

/* Exported constants --------------------------------------------------------*/
/* Secure User Memory end : from FLASH_BASE to end of active slot header */
#define SFU_SEC_MEM_AREA_ADDR_END SFU_ROM_ADDR_END

#define SFU_NB_PAGE_SEC_USER_MEM ((uint32_t)(((SFU_SEC_MEM_AREA_ADDR_END - FLASH_BASE) / FLASH_PAGE_SIZE_128_BITS)\
                                             + 1U))


/*!< Bank 1, Area A  used to protect Vector Table*/
#define SFU_PROTECT_WRP_AREA_1          (OB_WRPAREA_BANK1_AREAA)

/*!< First page including the Vector Table: 0 based */
#define SFU_PROTECT_WRP_PAGE_START_1    ((uint32_t)((SFU_BOOT_BASE_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE_128_BITS))

/*!< Last page: (code_size-1)/page_size because the page
     indexes start from 0 */
#define SFU_PROTECT_WRP_PAGE_END_1      ((uint32_t)((SFU_ROM_ADDR_END - FLASH_BASE) / FLASH_PAGE_SIZE_128_BITS))


#define SFU_PROTECT_PCROP_AREA          (FLASH_BANK_1)                            /*!< PCROP Area */
#define SFU_PROTECT_PCROP_ADDR_START    ((uint32_t)SFU_KEYS_ROM_ADDR_START)       /*!< PCROP Start Address (included) */
#define SFU_PROTECT_PCROP_ADDR_END      ((uint32_t)SFU_KEYS_ROM_ADDR_END)         /*!< PCROP End Address*/


/**
  * @brief The regions can overlap, and can be nested. The region 7 has the highest priority
  *    and the region 0 has the lowest one and this governs how overlapping the regions behave.
  *    The priorities are fixed, and cannot be changed.
  */



#define SFU_PROTECT_MPU_MAX_NB_SUBREG           (8U)

/**
  * @brief Region 0 - Enable the read/write operations for full peripheral area in unprivileged mode.
  *                   Execution capability disabled
  */
#define SFU_PROTECT_MPU_PERIPH_1_RGNV  MPU_REGION_NUMBER0
#define SFU_PROTECT_MPU_PERIPH_1_START PERIPH_BASE        /*!< Peripheral memory area */
#define SFU_PROTECT_MPU_PERIPH_1_SIZE  MPU_REGION_SIZE_512MB
#define SFU_PROTECT_MPU_PERIPH_1_SREG  0x00U              /*!< All subregions activated */
#define SFU_PROTECT_MPU_PERIPH_1_PERM  MPU_REGION_FULL_ACCESS
#define SFU_PROTECT_MPU_PERIPH_1_EXECV MPU_INSTRUCTION_ACCESS_DISABLE
#define SFU_PROTECT_MPU_PERIPH_1_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_PERIPH_1_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_PERIPH_1_C     MPU_ACCESS_NOT_CACHEABLE

/**
  * @brief Region 1 - Enable the read/write operations for RCC peripheral area in privileged mode.
  *                   Execution capability disabled
  *                   Inner region inside the Region 0
  */
#define SFU_PROTECT_MPU_PERIPH_2_RGNV  MPU_REGION_NUMBER1
#define SFU_PROTECT_MPU_PERIPH_2_START RCC_BASE
#define SFU_PROTECT_MPU_PERIPH_2_SIZE  MPU_REGION_SIZE_1KB
#define SFU_PROTECT_MPU_PERIPH_2_SREG  0x00U               /*!< All subregions activated */
#define SFU_PROTECT_MPU_PERIPH_2_PERM  MPU_REGION_PRIV_RW
#define SFU_PROTECT_MPU_PERIPH_2_EXECV MPU_INSTRUCTION_ACCESS_DISABLE
#define SFU_PROTECT_MPU_PERIPH_2_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_PERIPH_2_B     MPU_ACCESS_BUFFERABLE
#define SFU_PROTECT_MPU_PERIPH_2_C     MPU_ACCESS_NOT_CACHEABLE

/**
  * @brief Region 2 - Enable the read/write operations for full flash area in unprivileged mode.
  *                   Execution capability disabled
  */
#define SFU_PROTECT_MPU_FLASHACC_RGNV  MPU_REGION_NUMBER2
#define SFU_PROTECT_MPU_FLASHACC_START FLASH_BASE          /*!< FLASH memory area */
#define SFU_PROTECT_MPU_FLASHACC_SIZE  MPU_REGION_SIZE_512KB
#define SFU_PROTECT_MPU_FLASHACC_SREG  0x00U               /*!< All subregions activated */
#define SFU_PROTECT_MPU_FLASHACC_PERM  MPU_REGION_FULL_ACCESS
#define SFU_PROTECT_MPU_FLASHACC_EXECV MPU_INSTRUCTION_ACCESS_DISABLE
#define SFU_PROTECT_MPU_FLASHACC_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_FLASHACC_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_FLASHACC_C     MPU_ACCESS_CACHEABLE

/**
  * @brief Region 3 - Enable the execution for SB/SFU Full area (SBSFU + SE + Keys) in unprivileged mode.
  *                   Read only capability configured
  *                   Inner region inside the Region 2
  */
#define SFU_PROTECT_MPU_FLASHEXE_RGNV  MPU_REGION_NUMBER3
#define SFU_PROTECT_MPU_FLASHEXE_START FLASH_BASE          /*!< FLASH memory area */
#define SFU_PROTECT_MPU_FLASHEXE_SIZE  MPU_REGION_SIZE_256KB
#define SFU_PROTECT_MPU_FLASHEXE_SREG  0x00U               /*!< All subregions activated */
#define SFU_PROTECT_MPU_FLASHEXE_PERM  MPU_REGION_PRIV_RO_URO
#define SFU_PROTECT_MPU_FLASHEXE_EXECV MPU_INSTRUCTION_ACCESS_ENABLE
#define SFU_PROTECT_MPU_FLASHEXE_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_FLASHEXE_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_FLASHEXE_C     MPU_ACCESS_CACHEABLE

/**
  * @brief Region 4 - Enable the read/write operation in privileged mode for Header of active slots
  *                   Execution capability disabled
  *                   Inner region inside the Region 2
  */
#define SFU_PROTECT_MPU_HEADER_RGNV  MPU_REGION_NUMBER4
#define SFU_PROTECT_MPU_HEADER_START SLOT_ACTIVE_1_HEADER
#define SFU_PROTECT_MPU_HEADER_SREG  0x00U                 /*!< All subregions activated */
#define SFU_PROTECT_MPU_HEADER_SIZE  MPU_REGION_SIZE_4KB
#define SFU_PROTECT_MPU_HEADER_PERM  MPU_REGION_PRIV_RW
#define SFU_PROTECT_MPU_HEADER_EXECV MPU_INSTRUCTION_ACCESS_DISABLE
#define SFU_PROTECT_MPU_HEADER_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_HEADER_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_HEADER_C     MPU_ACCESS_CACHEABLE

/**
  * @brief Region 5 - Enable the read/write operation in unprivileged mode for RAM area.
  *                   Execution capability disabled
  */
#define SFU_PROTECT_MPU_SRAMACC_RGNV  MPU_REGION_NUMBER5
#define SFU_PROTECT_MPU_SRAMACC_START SRAM_BASE            /*!< RAM memory area */
#define SFU_PROTECT_MPU_SRAMACC_SIZE  MPU_REGION_SIZE_128KB
#define SFU_PROTECT_MPU_SRAMACC_SREG  0x00U                /*!< All subregions activated */
#define SFU_PROTECT_MPU_SRAMACC_PERM  MPU_REGION_FULL_ACCESS
#define SFU_PROTECT_MPU_SRAMACC_EXECV MPU_INSTRUCTION_ACCESS_DISABLE
#define SFU_PROTECT_MPU_SRAMACC_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_SRAMACC_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_SRAMACC_C     MPU_ACCESS_CACHEABLE

/**
  * @brief Region 6 - Enable the read/write operation in privileged mode for Secure Engine RAM area.
  *                   Execution capability disabled
  *                   Inner region inside the Region 5
  *                   Address must be aligned on 4KB as size is 4KB
  */
#define SFU_PROTECT_MPU_SRAM_SE_RGNV  MPU_REGION_NUMBER6
#define SFU_PROTECT_MPU_SRAM_SE_START SFU_SENG_RAM_ADDR_START  /*!< SE RAM memory area */
#define SFU_PROTECT_MPU_SRAM_SE_SIZE  MPU_REGION_SIZE_4KB
#define SFU_PROTECT_MPU_SRAM_SE_SREG  0x00U                /*!< All subregions activated */
#define SFU_PROTECT_MPU_SRAM_SE_PERM  MPU_REGION_PRIV_RW
#define SFU_PROTECT_MPU_SRAM_SE_EXECV MPU_INSTRUCTION_ACCESS_DISABLE
#define SFU_PROTECT_MPU_SRAM_SE_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_SRAM_SE_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_SRAM_SE_C     MPU_ACCESS_CACHEABLE

/**
  * @brief Region 6 - Enable execution in privileged mode for Secure Memory RAM code area.
  *                   Read only capability configured
  *                   Execution capability enabled
  *                   Inner region inside the Region 5
  *                   Address must be aligned on 8KB as size is 8KB
  */
#define SFU_PROTECT_MPU_SRAM_HDP_RGNV  MPU_REGION_NUMBER6
#define SFU_PROTECT_MPU_SRAM_HDP_START SB_HDP_CODE_REGION_RAM_START  /*!< HDP activation code area */
#define SFU_PROTECT_MPU_SRAM_HDP_SIZE  MPU_REGION_SIZE_256B
#define SFU_PROTECT_MPU_SRAM_HDP_SREG  0x00U                /*!< All subregions activated */
#define SFU_PROTECT_MPU_SRAM_HDP_PERM  MPU_REGION_PRIV_RO
#define SFU_PROTECT_MPU_SRAM_HDP_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_SRAM_HDP_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_SRAM_HDP_C     MPU_ACCESS_CACHEABLE

/**
  * @brief Region 7 - Enable the execution for Secure Engine flash area in privileged mode.
  *                   Read only capability configured
  *                   Inner region inside the Region 3
  */
#define SFU_PROTECT_MPU_EXEC_SE_RGNV  MPU_REGION_NUMBER7
#define SFU_PROTECT_MPU_EXEC_SE_START FLASH_BASE           /*!< Flash memory area */
#define SFU_PROTECT_MPU_EXEC_SE_SIZE  MPU_REGION_SIZE_128KB
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define SFU_PROTECT_MPU_EXEC_SE_SREG  0x80U                /*!< 32 Kbytes / 8 * 7 ==> 28 Kbytes */
#elif defined (__GNUC__)
#define SFU_PROTECT_MPU_EXEC_SE_SREG  0xC0U                /*!< 32 Kbytes / 8 * 6 ==> 24 Kbytes */
#else
#define SFU_PROTECT_MPU_EXEC_SE_SREG  0xC0U                /*!< 32 Kbytes / 8 * 6 ==> 24 Kbytes */
#endif /* (__CC_ARM) || (__ARMCC_VERSION) */
#define SFU_PROTECT_MPU_EXEC_SE_PERM  MPU_REGION_PRIV_RO
#define SFU_PROTECT_MPU_EXEC_SE_EXECV MPU_INSTRUCTION_ACCESS_ENABLE
#define SFU_PROTECT_MPU_EXEC_SE_TEXV  MPU_TEX_LEVEL0
#define SFU_PROTECT_MPU_EXEC_SE_B     MPU_ACCESS_NOT_BUFFERABLE
#define SFU_PROTECT_MPU_EXEC_SE_C     MPU_ACCESS_CACHEABLE



/**
  * @}
  */

/** @defgroup SFU_CONFIG_TAMPER Tamper Configuration
  * @{
  */
#define TAMPER_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()
#define RTC_TAMPER_ID                      RTC_TAMPER_2
#define RTC_TAMPER_ID_INTERRUPT            RTC_TAMPER2_INTERRUPT

/**
  * @}
  */

/** @defgroup SFU_CONFIG_DBG Debug Port Configuration
  * @{
  */
#define SFU_DBG_PORT            GPIOA
#define SFU_DBG_CLK_ENABLE()    __HAL_RCC_GPIOA_CLK_ENABLE()
#define SFU_DBG_SWDIO_PIN       GPIO_PIN_13
#define SFU_DBG_SWCLK_PIN       GPIO_PIN_14


/**
  * @}
  */

/**
  * @}
  */

/** @defgroup SFU_SECURITY_Exported_Constants Exported Constants
  * @{
  */

/** @defgroup SFU_SECURITY_Exported_Constants_Wakeup FU WAKEUP ID Type definition
  * @{
  */
typedef enum
{
    SFU_RESET_UNKNOWN = 0x00U,
    SFU_RESET_WDG_RESET,
    SFU_RESET_LOW_POWER,
    SFU_RESET_HW_RESET,
    SFU_RESET_BOR_RESET,
    SFU_RESET_SW_RESET,
    SFU_RESET_OB_LOADER,
} SFU_RESET_IdTypeDef;

/**
  * @}
  */

/** @defgroup SFU_SECURITY_Exported_Constants_Protections FU SECURITY Protections_Constants
  * @{
  */
#define SFU_PROTECTIONS_NONE                 ((uint32_t)0x00000000U)   /*!< Protection configuration unchanged */
#define SFU_STATIC_PROTECTION_RDP            ((uint32_t)0x00000001U)   /*!< RDP protection level 1 is applied */
#define SFU_STATIC_PROTECTION_WRP            ((uint32_t)0x00000002U)   /*!< Constants section in Flash. Needed as
                                                                            separate section to support PCRoP */
#define SFU_STATIC_PROTECTION_PCROP          ((uint32_t)0x00000004U)   /*!< SFU App section in Flash */
#define SFU_STATIC_PROTECTION_LOCKED         ((uint32_t)0x00000008U)   /*!< RDP Level2 is applied. The device is Locked!
                                                                            Std Protections cannot be
                                                                            added/removed/modified */
#define SFU_STATIC_PROTECTION_BFB2           ((uint32_t)0x00000010U)   /*!< BFB2 is disabled. The device shall always
                                                                            boot in bank1! */

#define SFU_RUNTIME_PROTECTION_MPU           ((uint32_t)0x00000100U)   /*!< Shared Info section in Flash */
#define SFU_RUNTIME_PROTECTION_IWDG          ((uint32_t)0x00000400U)   /*!< Independent Watchdog */
#define SFU_RUNTIME_PROTECTION_DAP           ((uint32_t)0x00000800U)   /*!< Debug Access Port control */
#define SFU_RUNTIME_PROTECTION_DMA           ((uint32_t)0x00001000U)   /*!< DMA protection, disable DMAs */
#define SFU_RUNTIME_PROTECTION_ANTI_TAMPER   ((uint32_t)0x00002000U)   /*!< Anti-Tampering protections */
#define SFU_RUNTIME_PROTECTION_CLOCK_MONITOR ((uint32_t)0x00004000U)   /*!< Activate a clock monitoring */
#define SFU_RUNTIME_PROTECTION_TEMP_MONITOR  ((uint32_t)0x00008000U)   /*!< Activate a Temperature monitoring */

#define SFU_STATIC_PROTECTION_ALL           (SFU_STATIC_PROTECTION_RDP   | SFU_STATIC_PROTECTION_WRP   | \
                                             SFU_STATIC_PROTECTION_PCROP | SFU_STATIC_PROTECTION_LOCKED)
/*!< All the static protections */

#define SFU_RUNTIME_PROTECTION_ALL          (SFU_RUNTIME_PROTECTION_MPU  | SFU_RUNTIME_PROTECTION_FWALL | \
                                             SFU_RUNTIME_PROTECTION_IWDG | SFU_RUNTIME_PROTECTION_DAP   | \
                                             SFU_RUNTIME_PROTECTION_DMA  | SFU_RUNTIME_PROTECTION_ANTI_TAMPER  | \
                                             SFU_RUNTIME_PROTECTION_CLOCK_MONITOR | SFU_RUNTIME_PROTECTION_TEMP_MONITOR)
/*!< All the run-time protections */

#define SFU_INITIAL_CONFIGURATION           (0x00U)     /*!< Initial configuration */
#define SFU_SECOND_CONFIGURATION            (0x01U)     /*!< Second configuration */
#define SFU_THIRD_CONFIGURATION             (0x02U)     /*!< Third configuration */

/* Exported functions ------------------------------------------------------- */
#define SFU_CALLBACK_ANTITAMPER HAL_RTCEx_Tamper2EventCallback
/*!<SFU Redirect of RTC Tamper Event Callback*/
#define SFU_CALLBACK_MEMORYFAULT(void) MemManage_Handler(void)  /*!<SFU Redirect of Mem Manage Callback*/

SFU_ErrorStatus    SFU_LL_SECU_IWDG_Refresh(void);
SFU_ErrorStatus    SFU_LL_SECU_CheckApplyStaticProtections(void);
SFU_ErrorStatus    SFU_LL_SECU_CheckApplyRuntimeProtections(uint8_t uStep);
void               SFU_LL_SECU_GetResetSources(SFU_RESET_IdTypeDef *peResetpSourceId);
void               SFU_LL_SECU_ClearResetSources(void);
#ifdef SFU_MPU_PROTECT_ENABLE
SFU_ErrorStatus    SFU_LL_SECU_SetProtectionMPU(uint8_t uStep);
#endif /*SFU_MPU_PROTECT_ENABLE*/
#ifdef SFU_DMA_PROTECT_ENABLE
SFU_ErrorStatus    SFU_LL_SECU_SetProtectionDMA(void);
#endif /*SFU_DMA_PROTECT_ENABLE*/
#ifdef SFU_DAP_PROTECT_ENABLE
SFU_ErrorStatus    SFU_LL_SECU_SetProtectionDAP(void);
#endif /*SFU_DAP_PROTECT_ENABLE*/
#ifdef SFU_TAMPER_PROTECT_ENABLE
SFU_ErrorStatus    SFU_LL_SECU_SetProtectionANTI_TAMPER(void);
#endif /*SFU_DAP_PROTECT_ENABLE*/
SFU_ErrorStatus    SFU_LL_SECU_SetProtectionMPU_SecUser(uint8_t Exec_Property);
void               SFU_LL_SECU_ActivateSecUser(uint32_t Address);

#ifdef __cplusplus
}
#endif

#endif /* SFU_LOW_LEVEL_SECURITY_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
