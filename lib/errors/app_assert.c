#include "compilers.h"
#include "errors.h"
#include "kernel.h"
#include <arch/cpu.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(assert);

#define OOPS(x) k_oops(x)
static uint32_t error_count = 0;

__WEAK void
app_assert_handler(int32_t error_code, uint32_t line_num,
                   const uint8_t *p_file_name)
{
    LOG_ERR("Failing assert %s:%d, error %d\n", p_file_name, line_num,
            error_code);

    ++error_count;

    // stop if debugging
    __BKPT(0);

    k_msleep(1000);

    // FIXME not an elegant way to reset
    NVIC_SystemReset();
}

uint32_t
app_assert_count()
{
    return error_count;
}
