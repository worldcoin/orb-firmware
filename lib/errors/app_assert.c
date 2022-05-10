#include "app_assert.h"
#include "compilers.h"
#include "kernel.h"
#include <arch/cpu.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>
LOG_MODULE_REGISTER(assert);

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

    if (assert_hard_user_cb) {
        assert_hard_user_cb(&fatal_error.error_info);
    }

    // busy wait for 2 seconds...
    k_busy_wait(2000000);

    HALT_IF_DEBUGGING();

    NVIC_SystemReset();
}

__WEAK void
app_assert_hard_handler(int32_t error_code, uint32_t line_num,
                        const uint8_t *p_file_name)
{
    fatal_error.error_info.err_code = error_code;
    fatal_error.error_info.line_num = line_num;
    memset(fatal_error.error_info.filename, 0,
           sizeof(fatal_error.error_info.filename));
    strlcpy(fatal_error.error_info.filename, p_file_name,
            sizeof(fatal_error.error_info.filename));

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
                        const uint8_t *p_file_name)
{
    LOG_ERR("%s:%d, error %d", p_file_name, line_num, error_code);

    ++error_count;

    // SeggerRTT sets the CPU as being debugged, but we don't want to halt/block
    // in this assert
#if !CONFIG_LOG_BACKEND_RTT
    // stop if debugging
    HALT_IF_DEBUGGING();
#endif
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
