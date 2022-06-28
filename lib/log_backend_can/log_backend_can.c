#include "log_backend_can.h"
#include "can_messaging.h"
#include "mcu_messaging.pb.h"
#include <logging/log.h>
#include <logging/log_backend.h>
LOG_MODULE_REGISTER(log_can);

static bool panic_mode = false;

// Max log string length per message, without NULL termination character
#define LOG_MAX_CHAR_COUNT (sizeof(Log) - 1)

static int
can_message_out(uint8_t *data, size_t length, void *ctx)
{
    (void)ctx;

    McuMessage log = {.which_message = McuMessage_m_message_tag,
                      .message.m_message.which_payload = McuToJetson_log_tag,
                      .message.m_message.payload.log.log = {0}};
    // keep NULL termination character at the end so subtract one to array size
    memcpy(log.message.m_message.payload.log.log, data,
           MIN(length, LOG_MAX_CHAR_COUNT));

    if (!panic_mode) {
        can_messaging_async_tx(&log);
    } else {
        // ⚠️ block until message is sent
        can_messaging_blocking_tx(&log);
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
    return panic_mode;
}

/// Send message to physical channel
/// Process is for LOG2
/// \param backend
/// \param msg
static void
process(const struct log_backend *const backend, union log_msg2_generic *msg)
{
    (void)backend;

    if (log_msg2_get_level(&msg->log) > CONFIG_ORB_LIB_LOG_BACKEND_LEVEL ||
        log_msg2_get_level(&msg->log) == LOG_LEVEL_NONE) {
        return;
    }

    // override flags :
    // - no color
    // - print level <wrn> or <err>
    log_output_msg2_process(&log_output_can, &msg->log, LOG_OUTPUT_FLAG_LEVEL);
}

static void
log_backend_can_init(struct log_backend const *const backend)
{
    (void)backend;

    panic_mode = false;
}

static void
panic(struct log_backend const *const backend)
{
    (void)backend;

    // On that call backend should switch to synchronous, interrupt-less
    // operation or shut down itself if that is not supported.
    panic_mode = true;
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
    .put = NULL,
    .put_sync_string = NULL,
    .put_sync_hexdump = NULL,
    .panic = panic,
    .init = log_backend_can_init,
    .dropped = dropped,
};

/// Definition of the backend
/// Enabled automatically during startup, autostart: true
LOG_BACKEND_DEFINE(log_backend_can, log_backend_can_api, true);
