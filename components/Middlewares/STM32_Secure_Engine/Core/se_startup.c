/**
  ******************************************************************************
  * @file    se_startup.c
  * @author  MCD Application Team
  * @brief   Secure Engine STARTUP module.
  *          This file provides set of firmware functions to manage SE Startup
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

/* Place code in a specific section*/
#if defined(__ICCARM__)
#pragma default_function_attributes = @ ".SE_Startup_Code"
#endif /* __ICCARM__ */

/* Includes ------------------------------------------------------------------*/
#include "se_def.h"
#include "se_startup.h"
#include "se_intrinsics.h"

/** @addtogroup SE Secure Engine
  * @{
  */

/** @addtogroup SE_CORE SE Core
  * @{
  */

/** @defgroup  SE_STARTUP SE Startup
  * @brief Init of the SE handled by the bootloader (via se_interface_bootloader) before the isolation mechanism is
  *        enabled.
  * @{
  */
#if defined(__ICCARM__)
void __iar_data_init3(void); /*!< Compiler-specific data initialization function*/
#elif defined (__ARMCC_VERSION)
void __arm_data_init(void);
#elif defined(__GNUC__)
void __gcc_data_init(void);
#endif /* __ICCARM__ */

/** @defgroup SE_STARTUP_Exported_Functions Exported Functions
  * @{
  */

/**
  * @brief Secure Engine Core Startup function.
  *        It is the initialization function for Secure Engine Core binary.
  *        Initialize all variables defined in the binary.
  * @note  It has to be called before the isolation mechanism is activated!!
  * @param  None.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
#if defined(__ARMCC_VERSION)
__attribute__((section(".SE_Startup_Code")))
#endif /* __ARMCC_VERSION */
__root SE_ErrorStatus SE_CORE_Startup(void)
{

#if defined(__ICCARM__)
  /* Data initialization function*/
  __iar_data_init3();
#elif defined (__ARMCC_VERSION)
  __arm_data_init();
#elif defined(__GNUC__)
  __gcc_data_init();
#endif /* __ICCARM__ */

  /*NOTE : other initialization may be added here */
  return SE_SUCCESS;
}

/* Stop placing data in specified section*/
#if defined(__ICCARM__)
#pragma default_function_attributes =
#elif defined(__CC_ARM)
#pragma arm section code
#endif /* __ICCARM__ */

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
