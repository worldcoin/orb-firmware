;/******************************************************************************
;* File Name          : se_interface_exception.s
;* Author             : MCD Application Team
;* Description        : This file defines function to handle user interrupts
;*                      raised in Firewall
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

        SECTION .SE_IF_Code_Entry:CODE:NOROOT(2)

        EXPORT SE_UserHandlerWrapper

        IMPORT DummyMemAccess
        IMPORT SeCallGateStatusParam
        IMPORT __ICFEDIT_SE_CallGate_region_ROM_start__

; ******************************************
; Function Name  : SE_UserHandlerWrapper
; Description    : call Appli IT Handler
; input          : R0 : @ Appli IT Handler
;                : R1 : Primask value
; internal       : R3
; output         : R0 : SE_CallGate 1st input param: Service ID
;                : R1 : SE_CallGate 2nd input param: return status var @
;                : R2 : SE_CallGate 3rd input param: Primask
; return         : None
; ******************************************
SE_UserHandlerWrapper
; restore Primask
        MSR PRIMASK, R1
; Specific B-L4S5I-IOT01A : force a SRAM data access outside FWALL protected SRAM to close correctly the FWALL
        ; See errata sheet: ES0393 - Rev 6 - October 2019
        ; DummyMemAccess variable used to force data access outside FWALL protected SRAM1
        ; and outside the 18 LSB range protected by FWALL.
        LDR R2, =DummyMemAccess
        LDR R2, [R2]
; call User IT Handler
        ; SE_UserHandlerWrapper shall be mapped at @ bit[4] = 1 in linker script file
        ; the purpose is to have LR bit [4] = 1 after executing the next instruction BLX R0
        ; So be careful not to modify code here that changes LR bit [4] after executing the next instruction BLX R0
        BLX R0
; disable IT
        CPSID i
; set input param for SE_CallGate
        MOV R0, #0x00001000 ; SE_EXIT_INTERRUPT
        LDR R1, =SeCallGateStatusParam
        MOV R2, #0xFFFFFFFF ; invalid Primask
; re-enter in firewall via SE_CallGate
        LDR R3, =__ICFEDIT_SE_CallGate_region_ROM_start__
        ADD R3, R3, #1
        BLX R3 ; LR shall be updated otherwise __IS_CALLER_SE_IF will fail in Se_CallGate
; we shall not raise this point
        ; B NVIC_SystemReset

        END
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
