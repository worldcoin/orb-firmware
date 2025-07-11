#include "orb_state.h"

#include <errors.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/errno.h>
#include <zephyr/sys/iterable_sections.h>
#include <zephyr/sys/printk.h>

TYPE_SECTION_START_EXTERN(struct orb_state_const_data, orb_state_const);
TYPE_SECTION_END_EXTERN(struct orb_state_const_data, orb_state_const);

void
orb_state_dump(void)
{
    for (const struct orb_state_const_data *data =
             TYPE_SECTION_START(orb_state_const);
         data < TYPE_SECTION_END(orb_state_const); data++) {
        if (data->dynamic_data != NULL) {
            printk("[%s]\t%s[%s]\t%s\n", data->name,
                   (strlen(data->name) > 2
                        ? (strlen(data->name) > 5 ? "" : "\t") // Align output,
                        : "\t\t"),
                   data->dynamic_data->status < 0
                       ? strerror(data->dynamic_data->status)
                       : ret_code_to_str(data->dynamic_data->status),
                   (char *)data->dynamic_data->message);
        } else {
            printk("Err: %s: no data available\n", data->name);
        }
    }
}

int
orb_state_set(struct orb_state_dynamic_data *data, const char *fmt, ...)
{
    if (data == NULL) {
        return -ENXIO; // no data provided
    }

    if (fmt == NULL) {
        data->message[0] = '\0';
        // Clear message if no format string is provided
        return 0;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf((char *)data->message, ORB_STATE_MESSAGE_MAX_LENGTH, fmt, args);
    va_end(args);

    return 0; // Assuming success
}
