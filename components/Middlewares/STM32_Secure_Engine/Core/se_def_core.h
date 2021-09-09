/**
  ******************************************************************************
  * @file    se_def_core.h
  * @author  MCD Application Team
  * @brief   This file contains core definitions for SE functionalities.
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
#ifndef SE_DEF_CORE_H
#define SE_DEF_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/** @addtogroup  SE Secure Engine
  * @{
  */

/** @addtogroup  SE_CORE SE Core
  * @{
  */

/** @addtogroup  SE_CORE_DEF SE Definitions
  * @{
  */

/** @addtogroup SE_DEF_CORE SE Core Definitions
  * @brief core definitions (constants, error codes, image handling) which are NOT crypto dependent
  * @{
  */

/** @defgroup SE_DEF_CORE_Exported_Constants Exported Constants
  * @{
  */

/**
  * @brief  Secure Engine Error definition
  */
typedef enum
{
  SE_ERROR = 0x00018799U,
  SE_SUCCESS = 0x0012310FU
} SE_ErrorStatus;

/** @defgroup  SE_Status_Structure_definition Secure Engine Status definition
  * @{
  */

#define SE_OK                            (0x0012e223UL)      /*!< Secure Engine OK */
#define SE_KO                            (0x0013023dUL)      /*!< Secure Engine KO */
#define SE_SIGNATURE_ERR                 (0x00146ac1UL)      /*!< An error occurred when checking FW signature (Tag) */
#define SE_ERR_FLASH_READ                (0x0014b817UL)      /*!< An error occurred trying to read the Flash */
#define SE_BUSY                          (0x1200e578UL)      /*!< SE is managing an interrupt: it cannot managed another service till the end of the interrupt */

typedef uint32_t SE_StatusTypeDef;                           /*!< Secure Engine status definition */

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

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* SE_DEF_CORE_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

