;/******************************************************************************
;* File Name          : se_stack_smuggler_ARM.s
;* Author             : MCD Application Team
;* Description        : Switch SP from SB to SE RAM region.
;*******************************************************************************
;* @attention
;*
;* Copyright (c) 2020 STMicroelectronics.
;* All rights reserved.
;*
;* This software is licensed under terms that can be found in the LICENSE file in
;* the root directory of this software component.
;* If no LICENSE file comes with this software, it is provided AS-IS.
;*
;*******************************************************************************
;
  AREA |.text|, CODE
  EXPORT SE_SP_SMUGGLE
;SE_SP_SMUGGLE(SE_FunctionIDTypeDef eID, SE_StatusTypeDef *peSE_Status, ...)
;R0 and R1 are used to call with new stack SE_CallGateService
  PRESERVE8    {TRUE}
  IMPORT |Image$$SE_region_RAM$$Base|
  IMPORT SE_CallGateService
SE_SP_SMUGGLE
; SP - 8
  PUSH {R11,LR}
; retrieve SP value on R11
  MOV R11, SP
; CHANGE SP
  LDR SP, =|Image$$SE_region_RAM$$Base|
; Let 4 bytes to store appli vector
  SUB SP, SP, #4
; push R11 on new stack
  PUSH {R11}
  BL SE_CallGateService
; retrieve previous stack
  POP {R11}
; put new stack
  MOV SP, R11
  POP {R11, LR}
; return
  BX LR
  ALIGN   4      ; now aligned on 4-byte boundary
  END
