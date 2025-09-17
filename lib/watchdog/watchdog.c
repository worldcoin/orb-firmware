#include "watchdog.h"
#include "orb_logs.h"
#include <app_assert.h>
#include <errors.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(watchdog, CONFIG_WATCHDOG_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(stack_area,
                             CONFIG_ORB_LIB_THREAD_STACK_SIZE_WATCHDOG);
static struct k_thread watchdog_thread_data;
static volatile int wdt_channel_id = -1;
static const struct device *const watchdog_dev =
    DEVICE_DT_GET(DT_ALIAS(watchdog0));

#ifndef WATCHDOG_RELOAD_MS
#define WATCHDOG_RELOAD_MS CONFIG_ORB_LIB_WATCHDOG_RELOAD_MS
#endif

BUILD_ASSERT(CONFIG_ORB_LIB_WATCHDOG_RELOAD_MS <
                 CONFIG_ORB_LIB_WATCHDOG_TIMEOUT_MS,
             "Watchdog reload time must be less than watchdog timeout");

static bool feed = true;

__WEAK bool
watchdog_perform_checks(void)
{
    return true;
}

void
watchdog_stop_feed(void)
{
    feed = false;
}

static void
watchdog_thread()
{
    while (wdt_channel_id >= 0) {
        // Allow null callback,
        // Don't rearrange due to short circuit rules, NULL check must be first
        if (feed && watchdog_perform_checks() == true) {
            wdt_feed(watchdog_dev, wdt_channel_id);
        }
        k_sleep(K_MSEC(WATCHDOG_RELOAD_MS));
    }

    LOG_ERR("Watchdog thread stopped");
}

int
watchdog_init(void)
{
    int err_code;

    if (!device_is_ready(watchdog_dev)) {
        ASSERT_SOFT(RET_ERROR_NOT_INITIALIZED);
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (wdt_channel_id >= 0) {
        ASSERT_SOFT(RET_ERROR_ALREADY_INITIALIZED);
        return RET_ERROR_ALREADY_INITIALIZED;
    }

    const struct wdt_timeout_cfg wdt_config = {
        /* Reset SoC when watchdog timer expires. */
        .flags = WDT_FLAG_RESET_SOC,

        /* Expire watchdog after max window */
        .window.min = 0,
        .window.max = CONFIG_ORB_LIB_WATCHDOG_TIMEOUT_MS,
    };

    wdt_channel_id = wdt_install_timeout(watchdog_dev, &wdt_config);
    if (wdt_channel_id < 0) {
        ASSERT_SOFT(RET_ERROR_NOT_INITIALIZED);
        return RET_ERROR_NOT_INITIALIZED;
    }

    uint8_t opt = 0;
#ifdef CONFIG_DEBUG
    opt |= WDT_OPT_PAUSE_HALTED_BY_DBG;
#endif

    err_code = wdt_setup(watchdog_dev, opt);
    if (err_code < 0) {
        ASSERT_SOFT(RET_ERROR_NOT_INITIALIZED);
        return RET_ERROR_NOT_INITIALIZED;
    }

    k_thread_create(&watchdog_thread_data, stack_area,
                    K_THREAD_STACK_SIZEOF(stack_area),
                    (k_thread_entry_t)watchdog_thread, NULL, NULL, NULL,
                    CONFIG_ORB_LIB_THREAD_PRIORITY_WATCHDOG, 0, K_NO_WAIT);
    k_thread_name_set(&watchdog_thread_data, "watchdog");

    return RET_SUCCESS;
}

#if CONFIG_ORB_LIB_WATCHDOG_SYS_INIT
SYS_INIT(watchdog_init, POST_KERNEL, CONFIG_ORB_LIB_WATCHDOG_INIT_PRIORITY);
#endif
