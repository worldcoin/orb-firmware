/**
  ******************************************************************************
  * @file    kms_mem_pool_def.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for Key Management Services (KMS)
  *          memory pool definition
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

/* Recursive inclusion autorised ---------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 * POOL DESCRIPTION
 */

/*
 * In the below example, the described memory is made of 3 pools
 * - Pool #1 is a 256 Bytes pool
 * - Pool #2 is a 256 Bytes pool
 * - Pool #3 is a 512 Bytes pool
 */
/*
KMS_MEM_DECLARE_POOL_START()
KMS_MEM_DECLARE_POOL_ENTRY(1,256)
KMS_MEM_DECLARE_POOL_ENTRY(2,256)
KMS_MEM_DECLARE_POOL_ENTRY(3,512)
KMS_MEM_DECLARE_POOL_END()
*/

#ifdef __cplusplus
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

