#include "log_backend_can.h"
#include "mcu_messaging.pb.h"
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
LOG_MODULE_REGISTER(log_can, CONFIG_LOG_CAN_LOG_LEVEL);

static int panic_count = 0;

// Max log string length per message, without NULL termination character
#define LOG_MAX_CHAR_COUNT (sizeof(Log) - 1)

static void (*print_log)(const char *log, size_t size, bool blocking) = NULL;

void
log_backend_can_register_print(void (*print)(const char *log, size_t size,
                                             bool blocking))
{
    print_log = print;
}

static int
can_message_out(uint8_t *data, size_t length, void *ctx)
{
    (void)ctx;

    // ⚠️ if panic has been called in panic mode, do not send any message
    if (print_log != NULL && panic_count <= 1) {
        // ⚠️ if panic, block until message is sent
        print_log(data, MIN(length, LOG_MAX_CHAR_COUNT), (panic_count != 0));
    }

    // no matter if the bytes have been sent
    // we consider them as processed
    return (int)length;
}

// Copy log into log_output_buf which has the same size as the Log message
// We define a log_output with `sizeof(Log) - 1` to keep room for the NULL
// termination character
static uint8_t log_output_buf[sizeof(Log)];
LOG_OUTPUT_DEFINE(log_output_can, can_message_out, log_output_buf,
                  LOG_MAX_CHAR_COUNT);

static inline bool
is_panic_mode(void)
{
    return (panic_count != 0);
}

/// Send message to physical channel
/// Process is for LOG2
/// \param backend
/// \param msg
static void
process(const struct log_backend *const backend, union log_msg_generic *msg)
{
    (void)backend;

    if (print_log == NULL) {
        // print_log function not registered, do not process
        return;
    }

#ifndef CONFIG_LOG_PRINTK
    if (log_msg_get_level(&msg->log) == LOG_LEVEL_NONE) {
        return;
    }
#endif

    if (log_msg_get_level(&msg->log) > CONFIG_ORB_LIB_LOG_BACKEND_LEVEL) {
        return;
    }

    // override flags :
    // - no color
    // - print level <wrn> or <err>
    log_output_msg_process(&log_output_can, &msg->log, LOG_OUTPUT_FLAG_LEVEL);
}

static void
log_backend_can_init(struct log_backend const *const backend)
{
    (void)backend;

    panic_count = 0;
}

static void
panic(struct log_backend const *const backend)
{
    (void)backend;

    // On that call backend should switch to synchronous, interrupt-less
    // operation or shut down itself if that is not supported.
    // panic() function could be called in panic mode, but should stop backend
    // operation, so we count the number of panic() calls
    panic_count++;
}

static void
dropped(const struct log_backend *const backend, uint32_t cnt)
{
    (void)backend;
    (void)cnt;

#if CONFIG_ORB_LIB_LOG_BACKEND_LEVEL < LOG_LEVEL_WRN
#warning printing info and debug logs can lead to dropped messages, please consider implementing this handler
#endif
}

const struct log_backend_api log_backend_can_api = {
    .process = process,
    .panic = panic,
    .init = log_backend_can_init,
    .dropped = dropped,
};

/// Definition of the backend
/// Enabled automatically during startup, autostart: true
LOG_BACKEND_DEFINE(log_backend_can, log_backend_can_api, true);
