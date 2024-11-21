#ifdef CONFIG_ORB_LIB_WATCHDOG
#include "watchdog.h"
#endif

#include "orb_logs.h"
#include <app_assert.h>
#include <compilers.h>
#include <zephyr/random/random.h>

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

__no_optimization static void
trigger_fault_bus()
{
    /** see Memfault's SDK: components/demo/src/panics/memfault_demo_panics.c */
    void (*unaligned_func)(void) = (void (*)(void))0x50000001;
    unaligned_func();
}

__no_optimization static void
trigger_fault_memmanage()
{
    /** see Memfault's SDK: components/demo/src/panics/memfault_demo_panics.c */
    // Per "Relation of the MPU to the system memory map" in ARMv7-M reference
    // manual:
    //
    // "The MPU is restricted in how it can change the default memory map
    // attributes associated with
    //  System space, that is, for addresses 0xE0000000 and higher. System space
    //  is always marked as XN, Execute Never."
    //
    // So we can trip a MemManage exception by simply attempting to execute any
    // addresss >= 0xE000.0000
    void (*bad_func)(void) = (void (*)(void))0xEEEEDEAD;
    bad_func();
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
__maybe_unused static void
kernel_assert_in_isr(const void *p)
{
    // unused if CONFIG_ASSERT is not enabled
    UNUSED_PARAMETER(p);

    __ASSERT(p != NULL, "parameter a should not be NULL!");
}

__maybe_unused static void
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

__maybe_unused static bool
watchdog_feed_callback(void)
{
    /* Deliberately prevent feeding the watchdog */
    return false;
}

void
fatal_errors_trigger(enum error_case_e type)
{
    if (type == FATAL_RANDOM) {
        type = sys_rand32_get() % TEST_ERROR_CASE_COUNT;
    }

    printk("Triggering error: %u/%u\n", type, TEST_ERROR_CASE_COUNT);
    k_msleep(100);

    switch (type) {
    case FATAL_ACCESS:
        trigger_fault_access();
        break;
    case FATAL_ILLEGAL_INSTRUCTION:
        trigger_fault_illegal_instruction();
        break;
    case FATAL_BUSFAULT:
        trigger_fault_bus();
        break;
    case FATAL_MEMMANAGE:
        trigger_fault_memmanage();
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
        __ASSERT(type != ASSERT_FAIL, "Explicitly triggered assert");
        break;
    case USER_ASSERT_HARD:
        ASSERT_HARD_BOOL(false);
        break;
#if defined(CONFIG_IRQ_OFFLOAD)
    case ASSERT_IN_ISR:
        irq_offload(kernel_assert_in_isr, NULL);
        break;
    case USER_ASSERT_HARD_IN_ISR:
        irq_offload(user_assert_in_isr, NULL);
        break;
#endif
#if defined(CONFIG_USERSPACE)
    case ZUSER_FATAL_Z_OOPS:
        trigger_z_oops();
        break;
#endif
#ifdef CONFIG_ORB_LIB_WATCHDOG
    case FATAL_WATCHDOG:
        (void)watchdog_init(watchdog_feed_callback);
        break;
#endif

    default:
        printk("Unimplemented\n");
    }
}
