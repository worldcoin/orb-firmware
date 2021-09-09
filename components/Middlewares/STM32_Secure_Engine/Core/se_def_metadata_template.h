/**
  ******************************************************************************
  * @file    se_def_metadata.h
  * @author  MCD Application Team
  * @brief   This file contains metadata definitions for SE functionalities.
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
#ifndef SE_DEF_METADATA_H
#define SE_DEF_METADATA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "se_crypto_config.h"

/** @addtogroup  SE Secure Engine
  * @{
  */

/** @addtogroup  SE_CORE SE Core
  * @{
  */

/** @addtogroup  SE_CORE_DEF SE Definitions
  * @{
  */

/** @defgroup SE_DEF_METADATA SE Metadata Definitions
  *  @brief definitions related to FW metadata (FW header) which are crypto dependent.
  *  @note This file is a template file and must be adapted for each cryptographic scheme.
  *  The se_def_metadata.h file declared in the SE_CoreBin project must be included by the SB_SFU project.
  * @{
  */

/** @defgroup SE_DEF_METADATA_Exported_Constants Exported Constants
  * @{
  */

/**
  * @brief  Firmware Image Header (FW metadata) constants
  */

#define SE_FW_HEADER_TOT_LEN    ((int32_t) sizeof(SE_FwRawHeaderTypeDef))   /*!< FW INFO header Total Length*/
#define SE_FW_HEADER_METADATA_LEN    ((int32_t) sizeof(SE_FwRawHeaderTypeDef))   /*!< FW Metadata INFO header Length*/

/**
  * @}
  */

/** @defgroup SE_DEF_METADATA_Exported_Types Exported Types
  * @{
  */

/**
  * @brief  Secure Engine crypto structure definition
  * There must be a Firmware Header structure definition
  * This structure MUST be called SE_FwRawHeaderTypeDef
  * This structure MUST contain a field named 'FwVersion'
  * This structure MUST contain a field named 'FwSize'
  */

/**
  * @brief  Firmware Header structure definition
  * @note This structure MUST be called SE_FwRawHeaderTypeDef
  * @note This structure MUST contain a field named 'FwVersion'
  * @note This structure MUST contain a field named 'FwSize'
  */
typedef struct
{
  uint16_t FwVersion;              /*!< Firmware version*/
  uint32_t FwSize;                 /*!< Firmware size (bytes)*/
  /* If you want to support Firmware Authentication then the field below is also required */
  /* uint8_t  FwTag[SE_TAG_LEN]; */     /*!< Firmware Tag*/
  /* If you want to authenticate the Firmware metadata the the field below is also required */
  /* uint8_t  HeaderMAC[SE_TAG_LEN]; */  /*!< MAC of the full header message */
} SE_FwRawHeaderTypeDef;

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

#endif /* SE_DEF_METADATA_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

