#ifndef FIRMWARE_UTILS_H
#define FIRMWARE_UTILS_H

#include "zephyr/sys/util.h"

// Number of elements in member 'field' of structure 'type'
#define STRUCT_MEMBER_ARRAY_SIZE(type, field) ARRAY_SIZE(((type *)0)->field)

// Size(in bytes) of the member 'field' of structure 'type'
#define STRUCT_MEMBER_SIZE_BYTES(type, field) (sizeof(((type *)0)->field))

// Prefer to use macro below to make sure we exit the critical section
// and re-enable interrupts
#define CRITICAL_SECTION_ENTER(k)                                              \
    {                                                                          \
        int k = irq_lock();

#define CRITICAL_SECTION_EXIT(k)                                               \
    irq_unlock(k);                                                             \
    }

#endif // FIRMWARE_UTILS_H
