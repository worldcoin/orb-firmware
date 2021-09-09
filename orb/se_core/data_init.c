/**
  ******************************************************************************
  * @file    data_init.c
  * @author  MCD Application Team
  * @brief   Data section (RW + ZI) initialization.
  *          This file provides set of firmware functions to manage SE low level
  *          interface functionalities.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright(c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "stdint-gcc.h"

#ifndef vu32
#	define vu32 volatile uint32_t
#endif

/**
  * @brief  Copy initialized data from ROM to RAM.
  * @param  None.
  * @retval None.
  */

void
LoopCopyDataInit(void)
{
    extern uint8_t _sidata asm("_sidata");
    extern uint8_t _sdata asm("_sdata");
    extern uint8_t _edata asm("_edata");

    vu32 *src = (vu32 *) &_sidata;
    vu32 *dst = (vu32 *) &_sdata;

    vu32 len = (&_edata - &_sdata) / 4;

    for (vu32 i = 0; i < len; i++)
        dst[i] = src[i];
}

/**
  * @brief  Clear the zero-initialized data section.
  * @param  None.
  * @retval None.
  */
void
LoopFillZerobss(void)
{
    extern uint8_t _sbss asm("_sbss");
    extern uint8_t _ebss asm("_ebss");

    vu32 *dst = (vu32 *) &_sbss;
    vu32 len = (&_ebss - &_sbss) / 4;

    for (vu32 i = 0; i < len; i++)
        dst[i] = 0;
}

/**
  * @brief  Data section initialization.
  * @param  None.
  * @retval None.
  */
void
__gcc_data_init(void)
{
    LoopFillZerobss();
    LoopCopyDataInit();
}

/**
  * @brief  Clear the data initialized section.
  * @param  None.
  * @retval None.
  */
void
LoopCleanDataInit(void)
{
    extern uint8_t _sdata asm("_sdata");
    extern uint8_t _edata asm("_edata");
    vu32 *dst = (vu32 *) &_sdata;

    vu32 len = (&_edata - &_sdata) / 4;

    for (vu32 i = 0; i < len; i++)
        dst[i] = 0;
}

/**
  * @brief  Clear Data section.
  * @param  None.
  * @retval None.
  */
void
__gcc_clean_data(void)
{
    LoopFillZerobss();
    LoopCleanDataInit();
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
