/**
  ******************************************************************************
  * @file    se_interface_bootloader.c
  * @author  MCD Application Team
  * @brief   Secure Engine Interface module.
  *          This file provides set of firmware functions to manage SE Interface
  *          functionalities. These services are used by the bootloader.
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
#pragma default_function_attributes = @ ".SE_IF_Code"
#elif defined(__CC_ARM)
#pragma arm section code = ".SE_IF_Code"
#endif /* __ICCARM__ */

/* Includes ------------------------------------------------------------------*/
#include "main.h"                       /* se_interface_bootloader.c is compiled in SBSFU project using main.h from
                                         * this project
                                         */
#include "se_low_level.h"               /* This file is part of SE_CoreBin and adapts the Secure Engine
                                         *(and its interface) to the STM32 board specificities
                                         */
#include "se_interface_common.h"
#include "se_callgate.h"
#include "se_interface_bootloader.h"
#include "se_intrinsics.h"


/** @addtogroup SE Secure Engine
  * @{
  */

/** @defgroup  SE_INTERFACE Secure Engine Interface
  * @{
  */

/** @defgroup  SE_INTERFACE_BOOTLOADER Secure Engine Interface for Bootloader
  * @brief Interface functions the Bootloader calls to use SE services.
  *        These APIs must remain crypto-agnostic.
  *  @note Please note that this code is compiled in the SB_SFU project (not in SE_CoreBin).
  *        Nevertheless, this file also includes "se_low_level.h" from SE_CoreBin to make sure the SE interface is in
  *        sync with the SE code.
  * @{
  *
  * "se_interface" is built and set in the context of the SB_SFU project.
  * Then some symbols are exported to the User Application for the User Application
  * to call some Secure Engine services through "se_interface"
  * (SB_SFU and the User Application are exclusive, they do not run in parallel).
  * As "se_interface" is set by SB_SFU but can be used by the User Application, it is important
  * not to introduce global variables if you do not want to create dependencies between the SB_SFU RAM mapping (SRAM1)
  * and the User Application RAM mapping.
  */

/*
 * DO NOT ADD ANY VARIABLE HERE, SEE EXPLANATIONS ABOVE
 * This file is dedicated to the bootloader so the problem should not occur.
 * Nevertheless, we keep the same rule for consistency.
 */

/** @defgroup SE_INTERFACE_BOOTLOADER_Exported_Functions Exported Functions
  * @brief These are the functions the bootloader can call to use the SE services.
  *        These functions must remain crypto-agnostic.
  * @{
  */

/**
  * @brief Secure Engine initialization function. This function initialize all the
  *        internals module of the Secure Engine.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param uSystemCoreClock System clock value.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_Init(SE_StatusTypeDef *peSE_Status, uint32_t uSystemCoreClock)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

  /* Check the pointers allocation */
  if (peSE_Status == NULL)
  {
    return SE_ERROR;
  }

  /* Set the CallGate function pointer */
  SET_CALLGATE();

  /* Enter Secure Mode */
  SE_EnterSecureMode(&primask_bit);

  /* Secure Engine Call */
  e_ret_status = (*SE_CallGatePtr)(SE_INIT_ID, peSE_Status, primask_bit, uSystemCoreClock);

  /* Exit Secure Mode */
  SE_ExitSecureMode(primask_bit);


  return e_ret_status;
}

/**
  * @brief Secure Engine Startup function. This function initializes the SE Core binary.
  *        This function must be called only one time so an internal check is executed.
  * @param None.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_Startup(void)
{
  SE_ErrorStatus e_ret_status;
  static uint8_t b_startup_already_done = 0U;
  static SE_ErrorStatus(*SE_StartupPtr)(void);            /*!< Secure Engine STARTUP  pointer function*/

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

  /* Need to be sure to execute this initialization only one time! */
  if (b_startup_already_done == 0U)
  {
    /* Secure Core Initialization */
    SE_StartupPtr = (SE_ErrorStatus(*)(void))((uint32_t) SE_STARTUP_REGION_ROM_START + 1U);
    e_ret_status = (*SE_StartupPtr)();
    if (e_ret_status == SE_SUCCESS)
    {
      b_startup_already_done = 1U;
    }
  }
  else
  {
    /* This function has been already successfully called */
    e_ret_status = SE_SUCCESS;
  }

  return e_ret_status;
}

/**
  * @brief call by SFU to lock part of Secure Engine services
  * @param  pSE_Status Secure Engine Status.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_LockRestrictServices(SE_StatusTypeDef *pSE_Status)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();
#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    SE_SysCall(&e_ret_status, SE_LOCK_RESTRICT_SERVICES, pSE_Status, NULL);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    SE_EnterSecureMode(&primask_bit);
    e_ret_status = (*SE_CallGatePtr)(SE_LOCK_RESTRICT_SERVICES, pSE_Status, primask_bit);
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief call by SFU to trigg FUS or wireless stack update process managed by CM0
  * @param  pSE_Status Secure Engine Status.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_CM0_Update(SE_StatusTypeDef *pSE_Status)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();
#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    SE_SysCall(&e_ret_status, SE_CM0_UPDATE, pSE_Status, NULL);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    SE_EnterSecureMode(&primask_bit);
    e_ret_status = (*SE_CallGatePtr)(SE_CM0_UPDATE, pSE_Status, primask_bit);
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief Configure "On The Fly DECryption" mechanism (OTFDEC) for external FLASH
  * @param pSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pxSE_Metadata Metadata that will be used to fill the Crypto Init structure.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_ExtFlash_Decrypt_Init(SE_StatusTypeDef *pSE_Status, SE_FwRawHeaderTypeDef *pxSE_Metadata)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();
#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[1] = {(uint32_t)pxSE_Metadata};
    SE_SysCall(&e_ret_status, SE_EXTFLASH_DECRYPT_INIT, pSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    SE_EnterSecureMode(&primask_bit);
    e_ret_status = (*SE_CallGatePtr)(SE_EXTFLASH_DECRYPT_INIT, pSE_Status, primask_bit, pxSE_Metadata);
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief call by SFU to read flash protected area
  * @param  pSE_Status: Secure Engine Status.
  * @param  pDestination: pointer to output buffer.
  * @param  pSource: pointer to the flash area to read.
  * @param  Length: number of bytes to read from flash
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_SFU_IMG_Read(SE_StatusTypeDef *pSE_Status, uint8_t *pDestination, const uint8_t *pSource,
                               uint32_t Length)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();
#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {

    uint32_t params[3] = {(uint32_t)pDestination, (uint32_t)pSource, (uint32_t)Length};
    SE_SysCall(&e_ret_status, SE_IMG_READ, pSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_IMG_READ, pSE_Status, primask_bit, pDestination, pSource, Length);;

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief call by SFU to write a flash protected area
  * @param  pSE_Status: Secure Engine Status.
  * @param  pDestination: pointer to flash area to write.
  * @param  pSource: pointer to input buffer.
  * @param  Length: number of bytes to write to flash
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_SFU_IMG_Write(SE_StatusTypeDef *pSE_Status, uint8_t *pDestination, const uint8_t *pSource,
                                uint32_t Length)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[3] = {(uint32_t)pDestination, (uint32_t)pSource, (uint32_t)Length};
    SE_SysCall(&e_ret_status, SE_IMG_WRITE, pSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    SE_EnterSecureMode(&primask_bit);
    e_ret_status = (*SE_CallGatePtr)(SE_IMG_WRITE, pSE_Status, primask_bit, pDestination, pSource, Length);
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief call by SFU to erase a flash protected area
  * @param  pSE_Status: Secure Engine Status.
  * @param  pDestination: pointer to flash area to erase.
  * @param  Length: number of bytes to erase in flash
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_SFU_IMG_Erase(SE_StatusTypeDef *pSE_Status, uint8_t *pDestination, uint32_t Length)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pDestination, (uint32_t)Length};
    SE_SysCall(&e_ret_status, SE_IMG_ERASE, pSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    SE_EnterSecureMode(&primask_bit);
    e_ret_status = (*SE_CallGatePtr)(SE_IMG_ERASE, pSE_Status, primask_bit, pDestination, Length);
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

#if !(SECBOOT_CRYPTO_SCHEME == SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256)

/**
  * @brief Secure Engine Decrypt Init function.
  *        It is a wrapper of Decrypt_Init function included in the protected area.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pxSE_Metadata Metadata that will be used to fill the Crypto Init structure.
  * @param SE_FwType Type of Fw Image.
  *        This parameter can be SE_FW_IMAGE_COMPLETE or SE_FW_IMAGE_PARTIAL.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_Decrypt_Init(SE_StatusTypeDef *peSE_Status, SE_FwRawHeaderTypeDef *pxSE_Metadata, uint32_t SE_FwType)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

  /* Check the parameter */
  if ((SE_FwType != SE_FW_IMAGE_COMPLETE) && (SE_FwType != SE_FW_IMAGE_PARTIAL))
  {
    return SE_ERROR;
  }

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pxSE_Metadata, (uint32_t)SE_FwType};
    SE_SysCall(&e_ret_status, SE_CRYPTO_LL_DECRYPT_INIT_ID, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_LL_DECRYPT_INIT_ID, peSE_Status, primask_bit, pxSE_Metadata, SE_FwType);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief Secure Engine Decrypt Append function.
  *        It is a wrapper of Decrypt_Append function included in the protected area.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pInputBuffer pointer to Input Buffer.
  * @param InputSize Input Size (bytes).
  * @param pOutputBuffer pointer to Output Buffer.
  * @param pOutputSize pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_Decrypt_Append(SE_StatusTypeDef *peSE_Status, const uint8_t *pInputBuffer, int32_t InputSize,
                                 uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[4] = {(uint32_t)pInputBuffer, (uint32_t)InputSize, (uint32_t)pOutputBuffer, (uint32_t)pOutputSize};
    SE_SysCall(&e_ret_status, SE_CRYPTO_LL_DECRYPT_APPEND_ID, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_LL_DECRYPT_APPEND_ID, peSE_Status, primask_bit, pInputBuffer, InputSize,
                                     pOutputBuffer, pOutputSize);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief Secure Engine Decrypt Finish function.
  *        It is a wrapper of Decrypt_Finish function included in the protected area.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pOutputBuffer pointer to Output Buffer.
  * @param pOutputSize pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_Decrypt_Finish(SE_StatusTypeDef *peSE_Status, uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pOutputBuffer, (uint32_t)pOutputSize};
    SE_SysCall(&e_ret_status, SE_CRYPTO_LL_DECRYPT_FINISH_ID, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_LL_DECRYPT_FINISH_ID, peSE_Status, primask_bit, pOutputBuffer,
                                     pOutputSize);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */
  return e_ret_status;
}

/**
  * @brief Secure Engine Firmware Authentication Init function.
  *        It is a wrapper of AuthenticateFW_Init function included in the Firewall.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pxSE_Metadata Metadata that will be used to fill the Crypto Init structure.
  * @param SE_FwType: Type of Fw Image.
  *        This parameter can be SE_FW_IMAGE_COMPLETE or SE_FW_IMAGE_PARTIAL.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_AuthenticateFW_Init(SE_StatusTypeDef *peSE_Status, SE_FwRawHeaderTypeDef *pxSE_Metadata,
                                      uint32_t SE_FwType)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

  /* Check the parameter */
  if ((SE_FwType != SE_FW_IMAGE_COMPLETE) && (SE_FwType != SE_FW_IMAGE_PARTIAL))
  {
    return SE_ERROR;
  }

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pxSE_Metadata, (uint32_t)SE_FwType};
    SE_SysCall(&e_ret_status, SE_CRYPTO_LL_AUTHENTICATE_FW_INIT_ID, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_LL_AUTHENTICATE_FW_INIT_ID, peSE_Status, primask_bit, pxSE_Metadata,
                                     SE_FwType);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */

  return e_ret_status;
}

/**
  * @brief Secure Engine Firmware Authentication Append function.
  *        It is a wrapper of AuthenticateFW_Append function included in the Firewall.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pInputBuffer pointer to Input Buffer.
  * @param InputSize Input Size (bytes).
  * @param pOutputBuffer pointer to Output Buffer.
  * @param pOutputSize pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_AuthenticateFW_Append(SE_StatusTypeDef *peSE_Status, const uint8_t *pInputBuffer, int32_t InputSize,
                                        uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[4] = {(uint32_t)pInputBuffer, (uint32_t)InputSize, (uint32_t)pOutputBuffer, (uint32_t)pOutputSize};
    SE_SysCall(&e_ret_status, SE_CRYPTO_LL_AUTHENTICATE_FW_APPEND_ID, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_LL_AUTHENTICATE_FW_APPEND_ID, peSE_Status, primask_bit, pInputBuffer,
                                     InputSize, pOutputBuffer, pOutputSize);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */

  return e_ret_status;
}

/**
  * @brief Secure Engine Firmware Authentication Finish function.
  *        It is a wrapper of AuthenticateFW_Finish function included in the Firewall.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pOutputBuffer pointer to Output Buffer.
  * @param pOutputSize pointer to Output Size (bytes).
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_AuthenticateFW_Finish(SE_StatusTypeDef *peSE_Status, uint8_t *pOutputBuffer, int32_t *pOutputSize)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {(uint32_t)pOutputBuffer, (uint32_t)pOutputSize};
    SE_SysCall(&e_ret_status, SE_CRYPTO_LL_AUTHENTICATE_FW_FINISH_ID, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    /* Secure Engine Call */
    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_LL_AUTHENTICATE_FW_FINISH_ID, peSE_Status, primask_bit, pOutputBuffer,
                                     pOutputSize);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */

  return e_ret_status;
}


/**
  * @brief Secure Engine Verify Fw Header signature.
  *        It verifies the signature of a raw header file.
  * @param peSE_Status Secure Engine Status.
  *        This parameter can be a value of @ref SE_Status_Structure_definition.
  * @param pxFwRawHeader pointer to RawHeader Buffer.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
SE_ErrorStatus SE_VerifyHeaderSignature(SE_StatusTypeDef *peSE_Status, SE_FwRawHeaderTypeDef *pxFwRawHeader)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[1] = {(uint32_t)pxFwRawHeader};
    SE_SysCall(&e_ret_status, SE_CRYPTO_HL_AUTHENTICATE_METADATA, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    e_ret_status = (*SE_CallGatePtr)(SE_CRYPTO_HL_AUTHENTICATE_METADATA, peSE_Status, primask_bit, pxFwRawHeader);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */

  return e_ret_status;

}

#endif /* SECBOOT_CRYPTO_SCHEME */

#ifdef ENABLE_IMAGE_STATE_HANDLING
/**
  * @brief Service called by the Bootloader to set the Active Firmware State.
  * @param peSE_Status Secure Engine Status.
  * @param SlotNumber index of the slot in the list
  * @param FwState Active Firmware State that should be set.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
__root SE_ErrorStatus SE_SFU_IMG_SetActiveFwState(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                                  SE_FwStateTypeDef *pFwState)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {SlotNumber, (uint32_t)pFwState};
    SE_SysCall(&e_ret_status, SE_IMG_SET_FW_STATE, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    e_ret_status = (*SE_CallGatePtr)(SE_IMG_SET_FW_STATE, peSE_Status, primask_bit, SlotNumber, pFwState);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */

  return e_ret_status;
}

/**
  * @brief Service called by the Bootloader to retrieve the Active Firmware State.
  * @param peSE_Status Secure Engine Status.
  * @param SlotNumber index of the slot in the list
  * @param p_FwState Active Firmware State structure that will be filled.
  * @retval SE_ErrorStatus SE_SUCCESS if successful, SE_ERROR otherwise.
  */
__root SE_ErrorStatus SE_SFU_IMG_GetActiveFwState(SE_StatusTypeDef *peSE_Status, uint32_t SlotNumber,
                                                  SE_FwStateTypeDef *pFwState)
{
  SE_ErrorStatus e_ret_status;
  uint32_t primask_bit; /*!< interruption mask saved when disabling ITs then restore when re-enabling ITs */

  /* Check if the call is coming from SFU code */
  __IS_SFU_RESERVED();

#ifdef SFU_ISOLATE_SE_WITH_MPU
  if (0U != SE_IsUnprivileged())
  {
    uint32_t params[2] = {SlotNumber, (uint32_t)pFwState};
    SE_SysCall(&e_ret_status, SE_IMG_GET_FW_STATE, peSE_Status, &params);
  }
  else
  {
#endif /* SFU_ISOLATE_SE_WITH_MPU */

    /* Set the CallGate function pointer */
    SET_CALLGATE();

    /* Enter Secure Mode */
    SE_EnterSecureMode(&primask_bit);

    e_ret_status = (*SE_CallGatePtr)(SE_IMG_GET_FW_STATE, peSE_Status, primask_bit, SlotNumber, pFwState);

    /* Exit Secure Mode */
    SE_ExitSecureMode(primask_bit);
#ifdef SFU_ISOLATE_SE_WITH_MPU
  }
#endif /* SFU_ISOLATE_SE_WITH_MPU */

  return e_ret_status;
}

#endif /* ENABLE_IMAGE_STATE_HANDLING */

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


/* Stop placing data in specified section */
#if defined(__ICCARM__)
#pragma default_function_attributes =
#elif defined(__CC_ARM)
#pragma arm section code
#endif /* __ICCARM__ */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
