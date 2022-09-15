#include "heartbeat.h"
#include "errors.h"
#include "zephyr.h"
#include <app_assert.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(heartbeat, CONFIG_HEARTBEAT_LOG_LEVEL);

#define THREAD_PRIORITY_HEARTBEAT   8
#define THREAD_STACK_SIZE_HEARTBEAT 512
static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_HEARTBEAT);
static struct k_thread health_monitoring_thread_data;
static k_tid_t thread_id;

static int (*heartbeat_timeout_cb)(void) = NULL;
static volatile uint32_t global_delay_s;

static int
timeout_default_handler(void)
{
    // ☠️
    ASSERT_HARD(RET_ERROR_TIMEOUT);
    return RET_ERROR_ASSERT_FAILS;
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
            heartbeat_timeout_cb();
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
        k_tid_t tid = k_thread_create(
            &health_monitoring_thread_data, stack_area,
            K_THREAD_STACK_SIZEOF(stack_area), thread_entry_point, NULL, NULL,
            NULL, THREAD_PRIORITY_HEARTBEAT, 0, K_NO_WAIT);
        k_thread_name_set(tid, "heartbeat");

        // make sure timeout handler is initialized
        if (heartbeat_timeout_cb == NULL) {
            heartbeat_timeout_cb = timeout_default_handler;
        }
    } else if (thread_id != NULL) {
        k_wakeup(thread_id);
    }

    return RET_SUCCESS;
}

void
heartbeat_register_cb(int (*timeout_cb)(void))
{
    if (timeout_cb != NULL) {
        heartbeat_timeout_cb = timeout_cb;
    } else {
        heartbeat_timeout_cb = timeout_default_handler;
    }
}
