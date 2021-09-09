/**
  ******************************************************************************
  * @file    kms_ecc.h
  * @author  MCD Application Team
  * @brief   This file contains utilities for the manipulation of elliptic curves
  *          of Key Management Services (KMS)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file in
  * the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef KMS_ECC_H
#define KMS_ECC_H

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Key_Management_Services Key Management Services (KMS)
  * @{
  */

/** @addtogroup KMS_ECC Elliptic curves utilities
  * @{
  */

/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
/** @addtogroup KMS_ECC_Exported_Functions Exported Functions
  * @{
  */
#if defined(KMS_ECDSA)
CK_RV KMS_ECC_LoadCurve(kms_attr_t *p_EC_Param, CA_EC_stt *p_EC_st);
#endif /* KMS_ECDSA */

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

#endif /* KMS_ECC_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
