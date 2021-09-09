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
  SECTION .text:CODE

  EXPORT SE_SP_SMUGGLE
  EXPORT SE_ExitHandler_Service

  IMPORT AppliActiveSp
  IMPORT AppliMsp
  IMPORT SeMsp
  IMPORT SeExcEntrySp
  IMPORT __ICFEDIT_SE_region_RAM_stack_top__
  IMPORT SE_CallGateService
  IMPORT PrimaskValue
  IMPORT AppliActiveSpMode
  IMPORT IntHand
  IMPORT SeExcReturn

; ******************************************
; Function Name  : SE_SP_SMUGGLE
; Description    : save Appli SP, call SE service, restore Appli SP
; input          : R0 : eID
;                : R1 : peSE_Status
;                : R2 : arguments
; internal       : R4, R6 : working register to update AppliMsp, AppliActiveSp, PRIMASK, CONTROL
;                : R5 : Appli Active SP
; return         : R0 : status
; ******************************************
SE_SP_SMUGGLE
; save LR
  PUSH {LR}
; save working registers
  PUSH {R4, R5, R6}
; save Appli MSP if SE_CallGate called from user appli (no interrupt)
  CMP R0, #0x00001000 ; compare to SE_EXIT_INTERRUPT
  ITTT NE
  MRSNE R5, MSP
  LDRNE R4, =AppliMsp
  STRNE R5, [R4]
; save Appli Active SP if SE_CallGate called from user appli (no interrupt)
  CMP R0, #0x00001000 ; compare to SE_EXIT_INTERRUPT
  ITTT NE
  MOVNE R5, SP
  LDRNE R4, =AppliActiveSp
  STRNE R5, [R4]
; set SP to initial Appli MSP in case of interrupt
  CMP R0, #0x00001000 ; compare to SE_EXIT_INTERRUPT
  ITT EQ
  LDREQ R4, =AppliMsp
  LDREQ SP, [R4]
; set current Active Stack Pointer to MSP (SE manages only MSP)
  MRS R4, CONTROL
  AND R4, R4, #0xFFFFFFFD
  MSR CONTROL, R4
  ISB
; set Active SP to SE Stack
  CMP R0, #0x00001000 ; compare to SE_EXIT_INTERRUPT
  ITTE EQ
  LDREQ R4, =SeMsp ; in case service = SE_EXIT_INTERRUPT
  LDREQ SP, [R4]
  LDRNE SP, =__ICFEDIT_SE_region_RAM_stack_top__ ; in case service != SE_EXIT_INTERRUPT
; restore primask in case service != SE_EXIT_INTERRUPT
  CMP R0, #0x00001000 ; compare service to SE_EXIT_INTERRUPT
  ITTT NE ; in case service != SE_EXIT_INTERRUPT
  LDRNE R4, =PrimaskValue
  LDRNE R4, [R4]
  MSRNE PRIMASK, R4
; call SE service
  BLX SE_CallGateService
; disable interrupts
  CPSID i
; reset current Active Stack Pointer to AppliActiveSpMode
  MRS R4,CONTROL
  LDR R6, =AppliActiveSpMode
  LDR R6, [R6]
  CMP R6, #0x00000000
  ITE EQ
  ANDEQ R4, R4, #0xFDFDFDFD
  ORRNE R4, R4, #0x02020202
  MSR CONTROL, R4
  ISB
; restore Appli MSP (at this step, service != SE_EXIT_INTERRUPT)
  LDR R4, =AppliMsp
  LDR R4, [R4]
  MSR MSP, R4
; restore Appli Active SP
  LDR R4, =AppliActiveSp
  LDR R4, [R4]
  MOV SP, R4
; restore saved register
  POP {R4, R5, R6}
; restore LR
  POP {LR}
; return
  BX LR

; ******************************************
; Function Name  : SE_ExitHandler_Service
; Description    : restore SP & non scratch registers & exit Handler Mode
; input          : None
; internal       : R0, R1
; return         : None: PC set with EXC_RETURN value
; ******************************************
SE_ExitHandler_Service
; reset interrupt handling flag
  MOV R0, #0x00000000
  LDR R1, =IntHand
  STR R0, [R1]
; Set Active SP to SeExcEntrySp
  LDR R0, =SeExcEntrySp
  LDR SP, [R0]
; R0 = SeExcReturn
  LDR R0, =SeExcReturn
  LDR R0, [R0]
; check if FPU non scratch register shall be restored
  TST R0, #0x10 ; test if FPCA = 1
  BNE RESTORE_CORE_REG_ONLY
; restore non-scratch FPU registers
  SUB R1, SP, #96 ; R1 = SeExcEntrySp - 24 SE stack entries (24 SE stack entries = (S15 -> S31) + (R4 -> R11))
  VLDR S16, [R1]
  VLDR S17, [R1, #4]
  VLDR S18, [R1, #8]
  VLDR S19, [R1, #12]
  VLDR S20, [R1, #16]
  VLDR S21, [R1, #20]
  VLDR S22, [R1, #24]
  VLDR S23, [R1, #28]
  VLDR S24, [R1, #32]
  VLDR S25, [R1, #36]
  VLDR S26, [R1, #40]
  VLDR S27, [R1, #44]
  VLDR S28, [R1, #48]
  VLDR S29, [R1, #52]
  VLDR S30, [R1, #56]
  VLDR S31, [R1, #60]
RESTORE_CORE_REG_ONLY
; restore non-scratch core registers
  SUB R1, SP, #32 ; R1 = SeExcEntrySp - 8 SE stack entries (8 SE stack entries = (R4 -> R11))
  LDR R4, [R1]
  LDR R5, [R1, #4]
  LDR R6, [R1, #8]
  LDR R7, [R1, #12]
  LDR R8, [R1, #16]
  LDR R9, [R1, #20]
  LDR R10, [R1, #24]
  LDR R11, [R1, #28]
; restore Primask
  LDR R1, =PrimaskValue
  LDR R1, [R1]
  MSR PRIMASK, R1
; trigger the exception return
  BX R0
; we shall not raise this point
  ; B NVIC_SystemReset

  END
