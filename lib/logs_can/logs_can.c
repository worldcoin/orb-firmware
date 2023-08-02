#include "logs_can.h"
#include <pb_encode.h>

#ifdef CONFIG_ORB_LIB_LOG_BACKEND_CAN
#include <log_backend_can.h>
#endif

static void (*send_log_over_can)(const char *log, size_t size,
                                 bool blocking) = NULL;

#if defined(CONFIG_PRINTK) && !defined(DEBUG)
extern void
__printk_hook_install(int (*fn)(int c));

#ifndef CONFIG_PRINTK_SYNC
#error                                                                         \
    "CONFIG_PRINTK_SYNC must be enabled to prevent race conditions over buffer"
#endif
static char buf[256] = {0};
static size_t buf_idx = 0;

static int
printk_hook(int c)
{
    if (send_log_over_can == NULL) {
        return c;
    }

    if ((c == '\n' || buf_idx >= sizeof(buf) - 1) && buf_idx > 0) {
        buf[buf_idx] = '\0';
        send_log_over_can(buf, buf_idx, false);
        buf_idx = 0;
    } else {
        buf[buf_idx++] = c;
    }

    return c;
}
#endif

int
logs_init(void (*print)(const char *log, size_t size, bool blocking))
{
    if (print == NULL) {
        return -1;
    }

    send_log_over_can = print;

#ifdef CONFIG_ORB_LIB_LOG_BACKEND_CAN
    log_backend_can_register_print(send_log_over_can);
#elif defined(CONFIG_PRINTK) && !defined(DEBUG)
    __printk_hook_install(printk_hook);
#endif

    return 0;
}
