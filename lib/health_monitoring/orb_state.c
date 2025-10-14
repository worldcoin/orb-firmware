#include "orb_state.h"

#include <errors.h>
#include <orb_logs.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/iterable_sections.h>

LOG_MODULE_REGISTER(orb_state, LOG_LEVEL_INF);

TYPE_SECTION_START_EXTERN(struct orb_state_const_data, orb_state_const);
TYPE_SECTION_END_EXTERN(struct orb_state_const_data, orb_state_const);

#if defined(CONFIG_SHELL) || defined(DEBUG)
#include <zephyr/shell/shell.h>

void
orb_state_dump(const struct shell *sh)
{
    struct orb_state_const_data *data = NULL;

    if (IS_ENABLED(CONFIG_SHELL) && sh != NULL) {
        while (orb_state_iter(&data)) {
            if (data->dynamic_data != NULL) {
                shell_print(sh, "[%-20s]\t[%-19s]\t%s", data->name,
                            data->dynamic_data->status < 0
                                ? strerror(data->dynamic_data->status)
                                : ret_code_to_str(data->dynamic_data->status),
                            (char *)data->dynamic_data->message);
            } else {
                shell_print(sh, "Err: %s: no data available\n", data->name);
            }
            k_msleep(1);
        }
    } else {
        while (orb_state_iter(&data)) {
            if (data->dynamic_data != NULL) {
                LOG_INF("[%-20s]\t[%-19s]\t%s", data->name,
                        data->dynamic_data->status < 0
                            ? strerror(data->dynamic_data->status)
                            : ret_code_to_str(data->dynamic_data->status),
                        (char *)data->dynamic_data->message);
            } else {
                LOG_ERR("Err: %s: no data available", data->name);
            }
            k_msleep(1);
        }
    }
}
#endif // CONFIG_SHELL

bool
orb_state_iter(struct orb_state_const_data **data_ptr)
{
    if (*data_ptr == NULL) {
        *data_ptr = TYPE_SECTION_START(orb_state_const);
    } else {
        // push the pointer to the next item in the section
        // if we reach the end of the section, reset to NULL, indicating the end
        if (++*data_ptr >= TYPE_SECTION_END(orb_state_const)) {
            data_ptr = NULL; // Reset to NULL to indicate end of section
        }
    }

    return (data_ptr != NULL);
}

int
orb_state_set(struct orb_state_dynamic_data *data, const int state,
              const char *fmt, ...)
{
    if (data == NULL) {
        return -ENXIO; // no data provided
    }

    if (data->status != state) {
        data->status = state;
    }

    if (fmt == NULL) {
        data->message[0] = '\0';
        // Clear message if no format string is provided
        return 0;
    }

    // '\0' is gonna be added to an eventually truncated message by vsnprintf
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf((char *)data->message, ORB_STATE_MESSAGE_MAX_LENGTH,
                        fmt, args);
    va_end(args);

    if (ret < 0) {
        LOG_ERR("vsnprintf failed: %d", ret);
        data->message[0] = '\0'; // Clear message on error
        return -EIO;             // Input/output error
    }

    if (ret > ORB_STATE_MESSAGE_MAX_LENGTH) {
        LOG_WRN("Message truncated: %s", data->message);
    }

    return 0;
}

int
orb_state_init(void)
{
    struct orb_state_const_data *data = NULL;
    while (orb_state_iter(&data)) {
        ((struct orb_state_dynamic_data *)data->dynamic_data)->status =
            RET_ERROR_NOT_INITIALIZED;
        memset(((struct orb_state_dynamic_data *)data->dynamic_data)->message,
               0, ORB_STATE_MESSAGE_MAX_LENGTH);
    }

    return 0;
}

SYS_INIT(orb_state_init, POST_KERNEL, CONFIG_ORB_LIB_SYS_INIT_STATE_PRIORITY);
