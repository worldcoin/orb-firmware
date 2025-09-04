#include "date.h"

#include <app_assert.h>
#include <time.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/logging/log.h>

static const struct device *const rtc = DEVICE_DT_GET(DT_ALIAS(rtc));
#define LOG_LEVEL LOG_LEVEL_INF
LOG_MODULE_REGISTER(date);

uint64_t
date_get(void)
{
    struct rtc_time time = {0};
    if (rtc_get_time(rtc, &time) == 0) {
        return mktime(rtc_time_to_tm(&time));
    }

    return 0;
}

void
date_print(void)
{
    struct rtc_time time = {0};
    if (rtc_get_time(rtc, &time) == 0) {
        LOG_INF("📆 %u-%u-%u, %02u:%02u:%02u", time.tm_year + 1900,
                time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min,
                time.tm_sec);
    }
}

int
date_set_time_epoch(const time_t epoch)
{
    struct tm tm_time;
    gmtime_r(&epoch, &tm_time);
    return date_set_time(&tm_time);
}

int
date_set_time(struct tm *tm_time)
{
    struct rtc_time rtc_date = {0};

    /* print clock drift if > 2 seconds */
    if (rtc_get_time(rtc, &rtc_date) == 0) {
        struct tm *tm_current = rtc_time_to_tm(&rtc_date);
        const time_t epoch_current = mktime(tm_current);
        const time_t epoch_new = mktime(tm_time);
        const int64_t time_diff = (int64_t)difftime(epoch_new, epoch_current);
        if (time_diff > 2) {
            LOG_INF(
                "Setting new date: %lld, current date: %lld; diff %lld sec.",
                epoch_new, epoch_current, time_diff);
        }
    }

    /* The `struct rtc_time` is 1-1 mapped to the `struct tm` for the members
     * \p tm_sec to \p tm_isdst making it compatible with the standard time
     * library.
     */
    memset(&rtc_date, 0, sizeof(rtc_date));
    memcpy(&rtc_date, tm_time, sizeof(struct tm));

    const int ret = rtc_set_time(rtc, &rtc_date);
    ASSERT_SOFT(ret);
    if (ret == 0) {
        date_print();
    }

    return ret;
}

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

ZTEST(hil, test_date)
{
    // test setter
    struct tm tm_time = {
        .tm_year = 2025 - 1900,
        .tm_mon = 5 - 1,
        .tm_mday = 27,
        .tm_hour = 9,
        .tm_min = 55,
    };
    int ret = date_set_time(&tm_time);
    zassert_equal(ret, 0);

    // test getter
    const uint64_t epoch_27_may_2025_9_55_00 = 1748339700;
    uint64_t date_epoch;
    date_epoch = date_get();
    zassert_equal(date_epoch, epoch_27_may_2025_9_55_00, "date_get: %llu",
                  date_epoch);

    // ensure the rtc is running
    k_msleep(10000);
    date_epoch = date_get();
    zassert_true(date_epoch >= (epoch_27_may_2025_9_55_00 + 9) &&
                 date_epoch <= (epoch_27_may_2025_9_55_00 + 11));
}
#endif
