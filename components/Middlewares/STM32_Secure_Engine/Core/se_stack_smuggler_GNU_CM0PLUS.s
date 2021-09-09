/**
  ******************************************************************************
  * @file      se_stack_smuggler_GNU_CM0PLUS.s
  * @author    MCD Application Team
  * @brief     Switch SP from SB to SE RAM region.
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
  .section .text
  .global SE_SP_SMUGGLE
  .syntax unified
  .thumb
//SE_SP_SMUGGLE(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *peSE_Status, ...)
//R0 and R1 are used to call with new stack SE_CallGateService
  .global __ICFEDIT_SE_region_RAM_stack_top__
  .global SE_CallGateService
SE_SP_SMUGGLE:
// SP - 8
  PUSH {R6, R7, LR}
// retrieve SP value on R7
  MOV R7, SP
// CHANGE SP
  LDR R6,  =__ICFEDIT_SE_region_RAM_stack_top__
  MOV SP, R6
// Let 4 bytes to store appli vector
  SUB SP, SP, #4
// push R7 on new stack
  PUSH {R7}
  BL SE_CallGateService
// retrieve previous stack
  POP {R7}
// put new stack
  MOV SP, R7
// return
  POP {R6, R7, PC}
  .end
