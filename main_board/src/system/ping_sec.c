#include "ping_sec.h"

#include "compilers.h"
#include "errors.h"
#include "mcu_ping.h"
#include "orb_logs.h"
#include "orb_state.h"
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(ping_sec);
ORB_STATE_REGISTER(canbus_sec);
static K_SEM_DEFINE(ping_sem, 1, 1);

#define PING_SEC_INTERVAL_MS 60000 // 1 minute
#define PING_PONG_TIMEOUT_MS                                                   \
    500 // 500 milliseconds, gives plenty of time to exchange ping while still
        // being fast for human

static void
ping_sec_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ping_sec_work, ping_sec_work_handler);

int
ping_sec(const void *arg, const k_timeout_t sem_timeout)
{
#ifdef CONFIG_SHELL
    const struct shell *sh = (const struct shell *)arg;
#else
    UNUSED_PARAMETER(arg);
#endif

    int ret = k_sem_take(&ping_sem, sem_timeout);
    if (ret != 0) {
        if (ret == -EAGAIN || ret == -ETIMEDOUT) {
            LOG_WRN("Failed to acquire ping semaphore: %d", ret);
        }
        return RET_ERROR_BUSY;
    }

    ping_pong_reset();
    ret = ping_pong_send_mcu(NULL);
    if (ret) {
#ifdef CONFIG_SHELL
        if (sh) {
            shell_error(sh, "Failed to send ping to security MCU: %d", ret);
        }
#endif
        ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE, "failed to send: %d",
                              ret);
        k_sem_give(&ping_sem);
        return ret;
    }

    if (pong_received()) {
#ifdef CONFIG_SHELL
        if (sh) {
            shell_warn(sh, "Pong already received, unexpected");
        }
#else
        LOG_WRN("Pong already received, unexpected");
#endif
    } else {
#ifdef CONFIG_SHELL
        if (sh) {
            shell_print(sh, "Ping sent, waiting for pong");
        }
#else
        LOG_DBG("Ping sent, waiting for pong");
#endif
    }

    k_msleep(PING_PONG_TIMEOUT_MS);

    if (pong_received()) {
#ifdef CONFIG_SHELL
        if (sh) {
            shell_print(sh, "Received pong from security MCU");
        }
#else
        LOG_INF("Received pong from security MCU");
#endif
        ORB_STATE_SET_CURRENT(RET_SUCCESS);
    } else {
#ifdef CONFIG_SHELL
        if (sh) {
            shell_error(sh, "No pong received from security MCU");
        }
#endif
        ORB_STATE_SET_CURRENT(RET_ERROR_TIMEOUT, "pong timed out");
        k_sem_give(&ping_sem);
        return RET_ERROR_TIMEOUT;
    }

    k_sem_give(&ping_sem);

    return RET_SUCCESS;
}

static void
ping_sec_work_handler(struct k_work *work)
{
    UNUSED_PARAMETER(work);

    // Call ping_sec with a timeout longer than PING_PONG_TIMEOUT_MS
    // to ensure we can acquire the semaphore even if a previous ping
    // is still in progress
    int ret = ping_sec(NULL, K_MSEC(PING_PONG_TIMEOUT_MS + 1000));
    if (ret != RET_SUCCESS && ret != RET_ERROR_BUSY) {
        LOG_WRN("ping_sec failed: %d", ret);
    }

    // Reschedule for the next interval regardless of success/failure
    // to maintain periodic health checks
    k_work_reschedule(&ping_sec_work, K_MSEC(PING_SEC_INTERVAL_MS));
}

int
ping_sec_init(void)
{
    // Schedule the first ping immediately
    return k_work_reschedule(&ping_sec_work, K_NO_WAIT);
}

int
ping_sec_cancel(void)
{
    return k_work_cancel_delayable(&ping_sec_work);
}
