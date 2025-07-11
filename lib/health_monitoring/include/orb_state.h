#ifndef ORB_STATE_H
#define ORB_STATE_H

#include <stdint.h>
#include <string.h>
#include <zephyr/sys/util.h>

#define ORB_STATE_MESSAGE_MAX_LENGTH 64

struct orb_state_const_data {
    const char *name; // Name of the state
    const struct orb_state_dynamic_data
        *dynamic_data; // Pointer to dynamic data
};

struct orb_state_dynamic_data {
    int status;
    char message[ORB_STATE_MESSAGE_MAX_LENGTH];
};

#define ORB_STATE_ITEM_CONST_DATA(_name)   UTIL_CAT(orb_state_const_, _name)
#define ORB_STATE_ITEM_DYNAMIC_DATA(_name) UTIL_CAT(orb_state_dynamic_, _name)

#define _ORB_STATE_DYNAMIC_DATA_CREATE(_name)                                  \
    STRUCT_SECTION_ITERABLE_ALTERNATE(orb_state_dynamic,                       \
                                      orb_state_dynamic_data,                  \
                                      ORB_STATE_ITEM_DYNAMIC_DATA(_name))

#define _ORB_STATE_CONST_DATA_CREATE(_name)                                    \
    static const char UTIL_CAT(_name, _str)[] __in_section(                    \
        _orb_state_strings, static, _CONCAT(_name, _)) __used __noasan =       \
        STRINGIFY(_name);                                                      \
    const STRUCT_SECTION_ITERABLE_ALTERNATE(                                   \
        orb_state_const, orb_state_const_data,                                 \
        ORB_STATE_ITEM_CONST_DATA(_name)) = {                                  \
        .name = STRINGIFY(_name),                                              \
        .dynamic_data = &ORB_STATE_ITEM_DYNAMIC_DATA(_name)}

#define ORB_STATE_MODULE_DECLARE(_name)                                        \
    extern const struct orb_state_const_data ORB_STATE_ITEM_CONST_DATA(_name); \
    extern struct orb_state_dynamic_data ORB_STATE_ITEM_DYNAMIC_DATA(_name);   \
    static const struct orb_state_const_data *__orb_state_const_data           \
        __unused = &ORB_STATE_ITEM_CONST_DATA(_name);                          \
    static struct orb_state_dynamic_data *__orb_state_dynamic_data __unused =  \
        &ORB_STATE_ITEM_DYNAMIC_DATA(_name)

// dynamic first to keep pointer to dyn in const data
#define ORB_STATE_REGISTER(name)                                               \
    _ORB_STATE_DYNAMIC_DATA_CREATE(name);                                      \
    _ORB_STATE_CONST_DATA_CREATE(name);                                        \
    ORB_STATE_MODULE_DECLARE(name)

#define ORB_STATE_CURRENT_DATA() (__orb_state_dynamic_data)

#define COPY_MESSAGE(_new_message)                                             \
    BUILD_ASSERT(sizeof(_new_message) <= ORB_STATE_MESSAGE_MAX_LENGTH,         \
                 "Message too long for orb state");                            \
    strncpy(ORB_STATE_CURRENT_DATA()->message, _new_message,                   \
            ORB_STATE_MESSAGE_MAX_LENGTH);

#define ORB_STATE_SET(new_status, ...)                                         \
    ORB_STATE_CURRENT_DATA()->status = new_status;                             \
    orb_state_set(ORB_STATE_CURRENT_DATA(),                                    \
                  COND_CODE_1(IS_EMPTY(__VA_ARGS__), (NULL), (__VA_ARGS__)))

int
orb_state_set(struct orb_state_dynamic_data *data, const char *fmt, ...);

void
orb_state_dump(void);

#endif
