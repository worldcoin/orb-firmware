#include <compilers.h>
#include <zephyr/tracing/tracing.h>

#ifndef CONFIG_TRACING_USER
#warning                                                                       \
    "CONFIG_TRACING_USER is not set, tracing will not be available, the source file can be removed from the build"
#endif

/**
 * Override the default tracing function to add a breakpoint on error.
 */
void
sys_trace_sys_init_exit_user(const struct init_entry *entry, int level,
                             int result)
{
    UNUSED_PARAMETER(entry);
    UNUSED_PARAMETER(level);

    if (result != 0) {
        /// to get the symbol name of the function that failed, use the
        /// following command:
        /// (gdb) info symbol (int)entry->init_fn
        __BKPT(0);
    }
}
