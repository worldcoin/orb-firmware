// include kernel.h first
// clang-format off
#include <zephyr/kernel.h>
#include <zephyr/arch/arm/aarch32/exc.h>
// clang-format on
#include <compilers.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(fatal);

static bool recursive_call_flag = false;

/// Fatal kernel error handling, resets the sytem
/// Reimplementation of weak `k_sys_fatal_error_handler` based on kernel/fatal.c
/// \param reason see k_fatal_error_reason
/// \param esf
void
k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
    ARG_UNUSED(esf);

    // "Debugging" in first call only,
    // Debugging might lead to new kernel panics as the current state is
    // undefined so skip debugging in case of recursive call and reset
    if (recursive_call_flag == false) {
        recursive_call_flag = true;

        // halt first if debugger attached
        // print logs later, if possible
        HALT_IF_DEBUGGING();

        LOG_PANIC();
        LOG_ERR("FATAL kernel error: %u", reason);
    }

    NVIC_SystemReset();
    CODE_UNREACHABLE;
}
