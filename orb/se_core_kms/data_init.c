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

#include "stdint-gcc.h"

#ifndef vu32
#	define vu32 volatile uint32_t
#endif

void LoopCopyDataInit(void) {
	extern char _sidata asm("_sidata");
	extern char _sdata asm("_sdata");
	extern char _edata asm("_edata");

	vu32* src = (vu32*) &_sidata;
	vu32* dst = (vu32*) &_sdata;

	vu32 len = (&_edata - &_sdata) / 4;

	for(vu32 i=0; i < len; i++)
		dst[i] = src[i];
}

void LoopFillZerobss(void) {
	extern char _sbss asm("_sbss");
	extern char _ebss asm("_ebss");

	vu32* dst = (vu32*) &_sbss;
	vu32 len = (&_ebss - &_sbss) / 4;

	for(vu32 i=0; i < len; i++)
		dst[i] = 0;
}

void __gcc_data_init(void) {
	LoopFillZerobss();
	LoopCopyDataInit();
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
