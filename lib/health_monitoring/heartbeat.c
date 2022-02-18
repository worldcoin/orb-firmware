//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#include "heartbeat.h"
#include "errors.h"
#include "zephyr.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(heartbeat);

#define THREAD_PRIORITY_HEARTBEAT   8
#define THREAD_STACK_SIZE_HEARTBEAT 512
static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_HEARTBEAT);
static struct k_thread thread_data;
static k_tid_t thread_id;

static void (*heartbeat_timeout_cb)(void) = NULL;
static volatile uint32_t global_delay_s;

static void
timeout_default_handler(void)
{
    // ☠️
    APP_ASSERT(RET_ERROR_TIMEOUT);
}

static void
thread_entry_point()
{
    for (;;) {
        // k_sleep returns the time in milliseconds left to sleep
        // if 0, it means the delay before the next heartbeat is
        // over, and the timeout handler has to be called.
        // Otherwise, it means the thread has been waked up via `k_wakeup()`
        if (k_sleep(K_SECONDS(global_delay_s)) == 0) {
            // turn off heartbeat monitoring
            global_delay_s = 0;
            if (heartbeat_timeout_cb != NULL) {
                heartbeat_timeout_cb();
            }
        }

        // turn off heartbeat if global_delay_s set to 0
        if (global_delay_s == 0) {
            break;
        }
    }

    thread_id = NULL;
}

int
heartbeat_boom(uint32_t delay_s)
{
    global_delay_s = delay_s;

    if (thread_id == NULL && global_delay_s != 0) {
        thread_id = k_thread_create(&thread_data, stack_area,
                                    K_THREAD_STACK_SIZEOF(stack_area),
                                    thread_entry_point, NULL, NULL, NULL,
                                    THREAD_PRIORITY_HEARTBEAT, 0, K_NO_WAIT);
        if (thread_id == NULL) {
            return RET_ERROR_INTERNAL;
        }

        // make sure timeout handler is init
        if (heartbeat_timeout_cb == NULL) {
            heartbeat_timeout_cb = timeout_default_handler;
        }
    } else if (thread_id != NULL) {
        k_wakeup(thread_id);
    }

    return RET_SUCCESS;
}

void
heartbeat_register_cb(void (*timeout_cb)(void))
{
    if (timeout_cb != NULL) {
        heartbeat_timeout_cb = timeout_cb;
    } else {
        heartbeat_timeout_cb = timeout_default_handler;
    }
}
