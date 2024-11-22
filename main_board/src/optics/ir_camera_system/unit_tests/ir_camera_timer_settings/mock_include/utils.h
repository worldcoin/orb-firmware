#include <zephyr/toolchain.h>

#define CRITICAL_SECTION_ENTER(k)                                              \
    {                                                                          \
        __unused int k;

#define CRITICAL_SECTION_EXIT(k) }
