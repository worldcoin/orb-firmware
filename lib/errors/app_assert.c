#include "app_assert.h"
#include "compilers.h"
#include "orb_logs.h"
#include "string.h"
#include "zephyr/kernel.h"
#include <zephyr/arch/cpu.h>
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(assert, CONFIG_ASSERT_LOG_LEVEL);

#if defined(CONFIG_MEMFAULT)
#include <memfault/core/trace_event.h>
#endif

static uint32_t error_count = 0;
// user-registered callback that can be called before hard reset
static void (*assert_hard_user_cb)(fatal_error_info_t *err_info) = NULL;

struct fatal_error_work {
    struct k_work work;
    fatal_error_info_t error_info;
};

static struct fatal_error_work fatal_error;

static void
fatal(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    // If thread priority is below < 0, the thread is cooperative, so it won't
    // be preempted until reset,
    // but if not, locking the thread will prevent it from being preempted
    k_sched_lock();

    // Logs pushed in blocking mode, without thread switch
    LOG_PANIC();
    LOG_ERR("FATAL %s:%d, error %d", fatal_error.error_info.filename,
            fatal_error.error_info.line_num, fatal_error.error_info.err_code);

    /* user callback at the end, in case of stack overflow... */
    if (assert_hard_user_cb) {
        assert_hard_user_cb(&fatal_error.error_info);
    }

    HALT_IF_DEBUGGING();

    NVIC_SystemReset();
}

__WEAK void
app_assert_hard_handler(int32_t error_code, uint32_t line_num,
                        const uint8_t *p_file_name)
{
    fatal_error.error_info.err_code = error_code;
    fatal_error.error_info.line_num = line_num;

    // destination is padded with zeros if null-character is found,
    // if not, make sure the last character is null
    strncpy(fatal_error.error_info.filename, p_file_name,
            sizeof(fatal_error.error_info.filename));
    fatal_error.error_info
        .filename[sizeof(fatal_error.error_info.filename) - 1] = '\0';

    if (!k_is_in_isr()) {
        // thread mode
        // won't return
        fatal(NULL);
    } else {
        // handler mode
        // schedule fatal error handling in high-priority system queue
        k_work_submit(&fatal_error.work);
    }
}

__WEAK void
app_assert_soft_handler(int32_t error_code, uint32_t line_num,
                        const uint8_t *p_file_name, const uint8_t *opt_message)
{
#if defined(CONFIG_MEMFAULT)
    // trace, no need to use line_num & p_file_name, cause the backtrace
    // is collected
    UNUSED_PARAMETER(line_num);
    UNUSED_PARAMETER(p_file_name);
    if (opt_message) {
        MEMFAULT_TRACE_EVENT_WITH_LOG(assert, "err %d: %s", error_code,
                                      opt_message);
    } else {
        MEMFAULT_TRACE_EVENT_WITH_LOG(assert, "err %d", error_code);
    }
#else
    if (opt_message != NULL) {
        LOG_ERR("%s:%d, error %d: %s", p_file_name, line_num, error_code,
                opt_message);
    } else {
        LOG_ERR("%s:%d, error %d", p_file_name, line_num, error_code);
    }
#endif

    ++error_count;

    // SeggerRTT sets the CPU as being debugged, but we don't want to halt/block
    // in this assert
#if !CONFIG_LOG_BACKEND_RTT
    // stop if debugging
    HALT_IF_DEBUGGING();
#endif
}

bool
app_assert_range(const char *name, const int32_t value, const int32_t min,
                 const int32_t max, const int32_t range_min,
                 const int32_t range_max, const bool verbose, const char *unity)
{
    if (range_min != 0 || range_max != 0) {
        if (value < range_min || value > range_max) {
            if (verbose) {
                LOG_WRN("%s = %d; NOT in range: [%d, %d] (unity: %s)", name,
                        value, range_min, range_max, unity ? unity : "N/A");
            }
            return false;
        } else {
            if (verbose) {
                if (min == max) {
                    LOG_INF("%s = %d; min = %d; max = %d; in range "
                            "[%d, %d] (unity: %s)",
                            name, value, min, max, range_min, range_max,
                            unity ? unity : "N/A");
                } else {
                    LOG_INF("%s = %d; min = %d; max = %d; in "
                            "range [%d, %d] (unity: %s)",
                            name, value, min, max, range_min, range_max,
                            unity ? unity : "N/A");
                }
            }
            return true;
        }
    } else {
        if (verbose) {
            LOG_INF("skipped: %s = %d; min = %d; max = %d (unity: %s)", name,
                    value, min, max, unity ? unity : "N/A");
        }
    }

    return true;
}

uint32_t
app_assert_soft_count()
{
    return error_count;
}

void
app_assert_init(void (*assert_callback)(fatal_error_info_t *err_info))
{
    assert_hard_user_cb = assert_callback;

    k_work_init(&fatal_error.work, fatal);
}
