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

#define PING_SEC_INTERVAL_MS 30000 // 30 seconds

static void
ping_sec_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ping_sec_work, ping_sec_work_handler);

int
ping_sec(const void *arg, const k_timeout_t timeout)
{
#ifdef CONFIG_SHELL
    const struct shell *sh = (const struct shell *)arg;
#else
    UNUSED_PARAMETER(arg);
#endif

    int ret = k_sem_take(&ping_sem, timeout);
    if (ret != 0) {
        return RET_ERROR_BUSY;
    }

    ping_pong_reset();
    ret = ping_pong_send_mcu(NULL);
    if (ret) {
#ifdef CONFIG_SHELL
        shell_error(sh, "Failed to send ping to security MCU: %d", ret);
#else
        ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE, "failed to send: %d",
                              ret);
#endif
        k_sem_give(&ping_sem);
        return ret;
    }
    if (pong_received()) {
#ifdef CONFIG_SHELL
        shell_warn(sh, "Pong already received, unexpected");
#else
        LOG_WRN("Pong already received, unexpected");
#endif
    } else {
#ifdef CONFIG_SHELL
        shell_print(sh, "Ping sent, waiting for pong");
#else
        LOG_DBG("Ping sent, waiting for pong");
#endif
    }

    k_msleep(500);

    if (pong_received()) {
#ifdef CONFIG_SHELL
        shell_print(sh, "Received pong from security MCU");
#else
        LOG_INF("Received pong from security MCU");
        ORB_STATE_SET_CURRENT(RET_SUCCESS);
#endif
    } else {
#ifdef CONFIG_SHELL
        shell_error(sh, "No pong received from security MCU");
#else
        ORB_STATE_SET_CURRENT(RET_ERROR_TIMEOUT, "pong timed out");
#endif
    }

    k_sem_give(&ping_sem);

    return RET_SUCCESS;
}

static void
ping_sec_work_handler(struct k_work *work)
{
    UNUSED_PARAMETER(work);

    // Call ping_sec with a reasonable timeout
    ping_sec(NULL, K_MSEC(1000));

    // Reschedule for the next interval
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
