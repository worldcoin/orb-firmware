//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#include "log_backend_can.h"
#include "can_messaging.h"
#include "mcu_messaging.pb.h"
#include <logging/log.h>
#include <logging/log_backend.h>
#include <logging/log_backend_std.h>
LOG_MODULE_REGISTER(log_can);

static bool panic_mode = false;

static int
can_message_out(uint8_t *data, size_t length, void *ctx)
{
    McuMessage log = {.which_message = McuMessage_m_message_tag,
                      .message.m_message.which_payload = McuToJetson_log_tag,
                      .message.m_message.payload.log.log = {0}};
    memcpy(log.message.m_message.payload.log.log, data,
           MIN(length, sizeof(log.message.m_message.payload.log) - 1));

    can_messaging_push_tx(&log);

    // no matter if the bytes have been sent
    // we consider them as processed
    return (int)length;
}

static uint8_t log_output_buf[Log_size - 1];
LOG_OUTPUT_DEFINE(log_output_can, can_message_out, log_output_buf,
                  Log_size - 1);

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
    if (log_msg2_get_level(&msg->log) == LOG_LEVEL_WRN ||
        log_msg2_get_level(&msg->log) == LOG_LEVEL_ERR) {
        log_output_msg2_process(&log_output_can, &msg->log, 0);
    }
}

static void
log_backend_can_init(struct log_backend const *const backend)
{
    panic_mode = false;
}

static void
panic(struct log_backend const *const backend)
{
    // On that call backend should switch to synchronous, interrupt-less
    // operation or shut down itself if that is not supported.
    panic_mode = true;
}

static void
dropped(const struct log_backend *const backend, uint32_t cnt)
{
    // TODO add specific error message sent to Jetson when CAN tx queue is full
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
