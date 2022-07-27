#include "app_assert.h"
#include <logging/log.h>
#include <random/rand32.h>

/// Based on fatal and assert tests, see
/// zephyr/tests/ztest/error_hook/README.txt

/* test case type */
enum {
    TEST_CATCH_FATAL_ACCESS,
    TEST_CATCH_FATAL_ILLEAGAL_INSTRUCTION,
    TEST_CATCH_FATAL_DIVIDE_ZERO,
    TEST_CATCH_FATAL_K_PANIC,
    TEST_CATCH_FATAL_K_OOPS,
    /* TEST_CATCH_FATAL_IN_ISR, not implemented */
    TEST_CATCH_ASSERT_FAIL,
    TEST_CATCH_ASSERT_IN_ISR,
    TEST_CATCH_USER_ASSERT_HARD,
    TEST_CATCH_USER_ASSERT_HARD_IN_ISR,
#if defined(CONFIG_USERSPACE)
    TEST_CATCH_USER_FATAL_Z_OOPS,
#endif
    TEST_ERROR_MAX
} error_case_type;

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
    uint32_t sub_type = sys_rand32_get() % TEST_ERROR_MAX;

    printk("Triggering error: %u\n", sub_type);

    switch (sub_type) {
    case TEST_CATCH_FATAL_ACCESS:
        trigger_fault_access();
        break;
    case TEST_CATCH_FATAL_ILLEAGAL_INSTRUCTION:
        trigger_fault_illegal_instruction();
        break;
    case TEST_CATCH_FATAL_DIVIDE_ZERO:
        trigger_fault_divide_zero();
        break;
    case TEST_CATCH_FATAL_K_PANIC:
        k_panic();
        break;
    case TEST_CATCH_FATAL_K_OOPS:
        k_oops();
        break;
    case TEST_CATCH_ASSERT_FAIL:
        __ASSERT(sub_type != TEST_CATCH_ASSERT_FAIL,
                 "Explicitly triggered assert");
        break;
    case TEST_CATCH_ASSERT_IN_ISR:
        irq_offload(kernel_assert_in_isr, NULL);
        break;
    case TEST_CATCH_USER_ASSERT_HARD:
        ASSERT_HARD_BOOL(false);
        break;
    case TEST_CATCH_USER_ASSERT_HARD_IN_ISR:
        irq_offload(user_assert_in_isr, NULL);
        break;
#if defined(CONFIG_USERSPACE)
    case ZTEST_CATCH_USER_FATAL_Z_OOPS:
        trigger_z_oops();
        break;
#endif
    default:
        printk("Unimplemented\n");
    }
}
