/**
  ******************************************************************************
  * @file      se_stack_smuggler_GNU.s
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
  PUSH {R11,LR}
// retrieve SP value on R11
  MOV R11, SP
// CHANGE SP
  LDR SP, =__ICFEDIT_SE_region_RAM_stack_top__
// Let 4 byte to store appli vector address
  SUB SP, SP, #4
// push R11 on new stack
  PUSH {R11}
  BLX SE_CallGateService
// retrieve previous stack
  POP {R11}
// put new stack
  MOV SP, R11
  POP {R11, LR}
// return
  BX LR
  .end
