/*
*****************************************************************************
**
**  File        : LinkerScript.ld
**
**  Abstract    : Linker script for STM32G474RETx Device with
**                0KByte FLASH, 0KByte RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
**  (c)Copyright Ac6.
**  You may use this file as-is or modify it according to the needs of your
**  project. Distribution of this file (unmodified or modified) is not
**  permitted. Ac6 permit registered System Workbench for MCU users the
**  rights to distribute the assembled, compiled & linked contents of this
**  file as part of an application binary file, provided that it is built
**  using the System Workbench for MCU toolchain.
**
*****************************************************************************
*/

/* Slots must be aligned on 4096 bytes (0x1000) */

/* swap (8 kbytes) */
__ICFEDIT_SWAP_start__           = 0x08046000;
__ICFEDIT_SWAP_end__             = 0x08047FFF;

/* Active slot #1 (216 kbytes) */
__ICFEDIT_SLOT_Active_1_start__  = 0x08010000;
__ICFEDIT_SLOT_Active_1_end__    = 0x08045FFF;
__ICFEDIT_SLOT_Active_1_header__ = __ICFEDIT_SLOT_Active_1_start__;

/* Dwl slot #1 (216 kbytes) */
__ICFEDIT_SLOT_Dwl_1_start__     = 0x08048000;
__ICFEDIT_SLOT_Dwl_1_end__       = 0x0807DFFF;


/* Slots not configured */
__ICFEDIT_SLOT_Active_2_header__ = 0x00000000;
__ICFEDIT_SLOT_Active_2_start__  = 0x00000000;
__ICFEDIT_SLOT_Active_2_end__    = 0x00000000;
__ICFEDIT_SLOT_Active_3_header__ = 0x00000000;
__ICFEDIT_SLOT_Active_3_start__  = 0x00000000;
__ICFEDIT_SLOT_Active_3_end__    = 0x00000000;
__ICFEDIT_SLOT_Dwl_2_start__     = 0x00000000;
__ICFEDIT_SLOT_Dwl_2_end__       = 0x00000000;
__ICFEDIT_SLOT_Dwl_3_start__     = 0x00000000;
__ICFEDIT_SLOT_Dwl_3_end__       = 0x00000000;
