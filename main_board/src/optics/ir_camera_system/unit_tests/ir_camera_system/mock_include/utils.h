#include <zephyr/toolchain.h>

#define STATIC_OR_EXTERN

#define CRITICAL_SECTION_ENTER(k)                                              \
    {                                                                          \
        __unused int k;

#define CRITICAL_SECTION_EXIT(k) }

// Size(in bytes) of the member 'field' of structure 'type'
#define STRUCT_MEMBER_SIZE_BYTES(type, field) (sizeof(((type *)0)->field))
