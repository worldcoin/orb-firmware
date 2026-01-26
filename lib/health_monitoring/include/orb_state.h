#ifndef ORB_STATE_H
#define ORB_STATE_H

#include <stdint.h>
#include <string.h>
#include <zephyr/sys/util.h>

#include "errors.h"

/*
 * Principle of this module is to provide a way to register
 * states of the system, which can be used for health monitoring and
 * diagnostics.
 * The states are stored in specifics ROM and RAM section, and can be iterated
 * over. Each state has a name (string), a status (int), and a message (string).
 * The status and message can be updated at runtime.
 *
 * Architecture has been inspired by the Zephyr's logging subsystem.
 *
 * Helpers are defined to register states, set their status and message:
 * - multiple states can be registered in one file using
 * `ORB_STATE_REGISTER_MULTIPLE`
 * - single state can be registered using `ORB_STATE_REGISTER`
 * - to set the current state, `ORB_STATE_SET` or `ORB_STATE_SET_CURRENT`
 * (single state)
 */

#define ORB_STATE_NAME_MAX_LENGTH    12 // 11 characters + null terminator
#define ORB_STATE_MESSAGE_MAX_LENGTH 36 // 35 characters + null terminator

struct orb_state_const_data {
    const char *name; // Name of the state
    const struct orb_state_dynamic_data
        *dynamic_data; // Pointer to dynamic data
};

struct orb_state_dynamic_data {
    ret_code_t status;
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
        .dynamic_data = &ORB_STATE_ITEM_DYNAMIC_DATA(_name)};                  \
    BUILD_ASSERT((sizeof(STRINGIFY(_name)) <= ORB_STATE_NAME_MAX_LENGTH),      \
                 "State name too long (11 chars max) for orb state: " #_name)

#define ORB_STATE_MODULE_DECLARE(_name)                                        \
    extern const struct orb_state_const_data ORB_STATE_ITEM_CONST_DATA(_name); \
    extern struct orb_state_dynamic_data ORB_STATE_ITEM_DYNAMIC_DATA(_name);   \
    static const struct orb_state_const_data *__orb_state_const_data           \
        __unused = &ORB_STATE_ITEM_CONST_DATA(_name);                          \
    static struct orb_state_dynamic_data *__orb_state_dynamic_data __unused =  \
        &ORB_STATE_ITEM_DYNAMIC_DATA(_name)

// Use ORB_STATE_REGISTER_MULTIPLE when registering multiple states in one file
// dynamic first to keep pointer to dyn in const data
#define ORB_STATE_REGISTER(name)                                               \
    _ORB_STATE_DYNAMIC_DATA_CREATE(name);                                      \
    _ORB_STATE_CONST_DATA_CREATE(name);                                        \
    ORB_STATE_MODULE_DECLARE(name)

// dynamic first to keep pointer to dyn in const data
// when defining multiple states in one file, use ORB_STATE_SET to set a state
// with the state name as the first argument
#define ORB_STATE_REGISTER_MULTIPLE(...)                                       \
    FOR_EACH(_ORB_STATE_DYNAMIC_DATA_CREATE, (;), __VA_ARGS__);                \
    FOR_EACH(_ORB_STATE_CONST_DATA_CREATE, (;), __VA_ARGS__);

#define ORB_STATE_CURRENT_DATA() (__orb_state_dynamic_data)

#define COPY_MESSAGE(_new_message)                                             \
    BUILD_ASSERT(sizeof(_new_message) <= ORB_STATE_MESSAGE_MAX_LENGTH,         \
                 "Message too long for orb state");                            \
    strncpy(ORB_STATE_CURRENT_DATA()->message, _new_message,                   \
            ORB_STATE_MESSAGE_MAX_LENGTH);

// to be used when setting the only registered state in the current file
#define ORB_STATE_SET_CURRENT(state, ...)                                      \
    orb_state_set(ORB_STATE_CURRENT_DATA(), state,                             \
                  COND_CODE_1(IS_EMPTY(__VA_ARGS__), (NULL), (__VA_ARGS__)))

// to be used when multiple states are registered in one file
#define ORB_STATE_SET(name, state, ...)                                        \
    orb_state_set(&ORB_STATE_ITEM_DYNAMIC_DATA(name), state,                   \
                  COND_CODE_1(IS_EMPTY(__VA_ARGS__), (NULL), (__VA_ARGS__)))

#define ORB_STATE_GET(name) (ORB_STATE_ITEM_DYNAMIC_DATA(name).status)

#define orb_state_set(data, state, ...)                                        \
    ({                                                                         \
        BUILD_ASSERT(state >= RET_SUCCESS && state <= RET_ERROR_UNSAFE,        \
                     "state must be of type ret_code_t");                      \
        orb_state_set_impl(data, state, __VA_ARGS__);                          \
    })

/**
 * @param data Pointer to the dynamic data structure for the state.
 * @param state ret_code_t value representing the state.
 * @param fmt format string for the message.
 * @param ... optional arguments for the format string.
 * @return 0 on success, negative error code on failure.
 */
int
orb_state_set_impl(struct orb_state_dynamic_data *data, const ret_code_t state,
                   const char *fmt, ...);

/**
 * Iterate over the registered states.
 *
 * @param data_ptr Pointer to a pointer to the current data item. Pointing to
 * NULL for initialization.
 * @return true if iterator has more data, false if it has reached the end.
 */
bool
orb_state_iter(struct orb_state_const_data **data_ptr);

#if defined(DEBUG) || defined(CONFIG_SHELL)
#include <zephyr/shell/shell.h>

/**
 * Dump the current state of the system to the log (printk)
 * This is useful for debugging and diagnostics.
 */
void
orb_state_dump(const struct shell *sh);

#endif

#endif
