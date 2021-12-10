//
// Created by Cyril on 17/06/2021.
//

#ifndef FIRMWARE_ORB_COMMON_INCLUDE_COMPILERS_H
#define FIRMWARE_ORB_COMMON_INCLUDE_COMPILERS_H

#include <stdint.h>

#if defined(__CC_ARM)

#include <cmsis_armcc.h>

#ifndef __ASM
#define __ASM __asm
#endif

#ifndef __INLINE
#define __INLINE __inline
#endif

#ifndef __WEAK
#define __WEAK __weak
#endif

#ifndef __ALIGN
#define __ALIGN(n) __align(n)
#endif

#ifndef __PACKED
#define __PACKED __packed
#endif

#define GET_SP() __current_sp()

#elif defined(__GNUC__)

#ifndef __ASM
#define __ASM __asm
#endif

#ifndef __INLINE
#define __INLINE inline
#endif

#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

#ifndef __ALIGN
#define __ALIGN(n) __attribute__((aligned(n)))
#endif

#ifndef __PACKED
#define __PACKED __attribute__((packed))
#endif

#ifndef __BKPT
#define __BKPT(value) __ASM volatile("bkpt " #value)
#endif

#endif

#define UNUSED_VARIABLE(X) ((void)(X))
#define UNUSED_PARAMETER(X) UNUSED_VARIABLE(X)
#define UNUSED_RETURN_VALUE(X) UNUSED_VARIABLE(X)

// NOTE: If you are using CMSIS, the registers can also be
// accessed through CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk
#define HALT_IF_DEBUGGING()                                                    \
    do {                                                                       \
        if ((*(volatile uint32_t *)0xE000EDF0) & (1 << 0)) {                   \
            __BKPT(1);                                                         \
        }                                                                      \
    } while (0)

#endif // FIRMWARE_ORB_COMMON_INCLUDE_COMPILERS_H
