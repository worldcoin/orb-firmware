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

#ifndef __must_be_array
#define __must_be_array(...)
#warning Checks not performed on arrays with this compiler
#endif

#define GET_SP() __current_sp()

#ifndef ASSERT_CONST_ARRAY_VALUE
#warning ASSERT_CONST_ARRAY_VALUE is not defined if using the ARM compiler
#endif

#elif defined(__GNUC__)

#include "cmsis_gcc.h"

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

/// usage of this function will lead to a compile-time error with the passed
/// message
__attribute((error("const value from within the array is incorrect"))) void
assert_pointer_value_check(char *details);

/// This macro helps testing a `const` array value at compile-time.
///
/// The compiler should remove the function call to
/// `assert_pointer_value_check` if the `expected` value matches `array_val`.
/// If not, the function is used, leading to a compile-time error.
/// This macro must be called from within a function.
///
/// Usage of `static_assert` doesn't work because an array value is a pointer
/// which is not considered const
#define ASSERT_CONST_ARRAY_VALUE(array_val, expected)                          \
    if (array_val != expected) {                                               \
        assert_pointer_value_check("");                                        \
    }

#ifndef __must_be_array
/*
 * Based on Linux kernel's include/linux/build_bug.h
 * https://github.com/torvalds/linux/blob/master/include/linux/build_bug.h:
 * Force a compilation error if condition is true, but also produce a result (of
 * value 0 and type int), so the expression can be used e.g. in a structure
 * initializer (or where-ever else comma expressions aren't permitted).
 */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#define __same_type(a, b)    __builtin_types_compatible_p(typeof(a), typeof(b))
// Based on
// https://github.com/torvalds/linux/blob/master/tools/include/linux/compiler-gcc.h
#define __must_be_array(a)   BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#endif

#endif

#define UNUSED_VARIABLE(X)     ((void)(X))
#define UNUSED_PARAMETER(X)    UNUSED_VARIABLE(X)
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
