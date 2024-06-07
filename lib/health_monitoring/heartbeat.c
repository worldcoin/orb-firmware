#include "heartbeat.h"
#include "errors.h"
#include "orb_logs.h"
#include "zephyr/kernel.h"
#include <app_assert.h>
LOG_MODULE_REGISTER(heartbeat, CONFIG_HEARTBEAT_LOG_LEVEL);

K_THREAD_STACK_DEFINE(heartbeat_stack_area, THREAD_STACK_SIZE_HEARTBEAT);
static struct k_thread health_monitoring_thread_data;
static k_tid_t thread_id;

static int (*heartbeat_timeout_cb)(void) = NULL;
static volatile uint32_t global_delay_s;
static K_SEM_DEFINE(heartbeat_sem, 0, 1);

static int
timeout_default_handler(void)
{
    // ☠️
    ASSERT_HARD(RET_ERROR_TIMEOUT);
    return RET_ERROR_ASSERT_FAILS;
}

static void
heartbeat_thread()
{
    for (;;) {
        if (k_sem_take(&heartbeat_sem, K_SECONDS(global_delay_s)) == -EAGAIN) {
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
    if (delay_s == 0 && global_delay_s != 0) {
        LOG_INF("stopped");
    } else if (delay_s) {
        LOG_INF("boom [%us.]", delay_s);
    }

    global_delay_s = delay_s;

    if (thread_id == NULL && global_delay_s != 0) {
        thread_id = k_thread_create(
            &health_monitoring_thread_data, heartbeat_stack_area,
            K_THREAD_STACK_SIZEOF(heartbeat_stack_area), heartbeat_thread, NULL,
            NULL, NULL, THREAD_PRIORITY_HEARTBEAT, 0, K_NO_WAIT);
        k_thread_name_set(thread_id, "heartbeat");

        // make sure timeout handler is initialized
        if (heartbeat_timeout_cb == NULL) {
            heartbeat_timeout_cb = timeout_default_handler;
        }
    }

    k_sem_give(&heartbeat_sem);

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
