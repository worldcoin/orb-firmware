#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

#include "orb_logs.h"
#include <app_assert.h>
#include <compilers.h>
#include <zephyr/random/rand32.h>

/// Based on fatal and assert tests, see
/// zephyr/tests/ztest/error_hook/README.txt

/* test case type */
enum error_case_e {
    FATAL_ACCESS,
    FATAL_ILLEGAL_INSTRUCTION,
    FATAL_DIVIDE_ZERO,
    FATAL_K_PANIC,
    FATAL_K_OOPS,
    /* FATAL_IN_ISR, not implemented */
    ASSERT_FAIL,
    ASSERT_IN_ISR,
    USER_ASSERT_HARD,
    USER_ASSERT_HARD_IN_ISR,
#if defined(CONFIG_USERSPACE)
    USER_FATAL_Z_OOPS,
#endif
    WATCHDOG,
    TEST_ERROR_CASE_COUNT
};

/*
 * Do not optimize to prevent GCC from generating invalid
 * opcode exception instruction instead of real instruction.
 */
__no_optimization static void
trigger_fault_access(void)
{
    *(volatile uint32_t *)0xbadcafe;
}

/*
 * Do not optimize to prevent GCC from generating invalid
 * opcode exception instruction instead of real instruction.
 */
__no_optimization static void
trigger_fault_illegal_instruction(void)
{
    void *a = NULL;

    /* execute an illegal instruction */
    ((void (*)(void)) & a)();
}

/*
 * Do not optimize the divide instruction. GCC will generate invalid
 * opcode exception instruction instead of real divide instruction.
 */
__no_optimization static void
trigger_fault_divide_zero(void)
{
    int a = 1;
    int b = 0;

    /* divide by zero */
    a = a / b;
    printk("a is %d\n", a);
}

/* a handler using by irq_offload  */
static void
kernel_assert_in_isr(const void *p)
{
    __ASSERT(p != NULL, "parameter a should not be NULL!");
}

static void
user_assert_in_isr(const void *p)
{
    UNUSED_PARAMETER(p);

    ASSERT_HARD_BOOL(false);
}

#if defined(CONFIG_USERSPACE)
static void
trigger_z_oops(void)
{
    /* Set up a dummy syscall frame, pointing to a valid area in memory. */
    _current->syscall_frame = _image_ram_start;

    Z_OOPS(true);
}
#endif

void
fatal_errors_test(void)
{
    uint32_t sub_type = sys_rand32_get() % TEST_ERROR_CASE_COUNT;

    printk("Triggering error: %u/%u\n", sub_type, TEST_ERROR_CASE_COUNT);
    k_msleep(100);

    switch (sub_type) {
    case FATAL_ACCESS:
        trigger_fault_access();
        break;
    case FATAL_ILLEGAL_INSTRUCTION:
        trigger_fault_illegal_instruction();
        break;
    case FATAL_DIVIDE_ZERO:
        trigger_fault_divide_zero();
        break;
    case FATAL_K_PANIC:
        k_panic();
        break;
    case FATAL_K_OOPS:
        k_oops();
        break;
    case ASSERT_FAIL:
        __ASSERT(sub_type != ASSERT_FAIL, "Explicitly triggered assert");
        break;
    case ASSERT_IN_ISR:
        irq_offload(kernel_assert_in_isr, NULL);
        break;
    case USER_ASSERT_HARD:
        ASSERT_HARD_BOOL(false);
        break;
    case USER_ASSERT_HARD_IN_ISR:
        irq_offload(user_assert_in_isr, NULL);
        break;
#ifdef CONFIG_WATCHDOG
    case WATCHDOG:
        (void)watchdog_init();
        break;
#endif
#if defined(CONFIG_USERSPACE)
    case ZUSER_FATAL_Z_OOPS:
        trigger_z_oops();
        break;
#endif
    default:
        printk("Unimplemented\n");
    }
}
