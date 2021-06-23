#include "FreeRTOS.h"
#include <task.h>
#include <logs.h>
#include "stdint.h"
#include "errors.h"
#include "compilers.h"
#include "logging.h"
#include "board.h"

/**
 * Function is implemented as weak so that it can be overwritten by custom application error handler
 * when needed.
 */
__WEAK void
app_error_fault_handler(uint32_t id, uint32_t pc, long info)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(pc);

    LOG_ERROR("Fatal error: 0x%lx %s:%lu",
              ((error_info_t *) info)->err_code,
              ((error_info_t *) info)->p_file_name,
              ((error_info_t *) info)->line_num);

    logs_final_flush();

    vTaskEndScheduler();

    // software breakpoint
    __builtin_trap();
}

void
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
    error_info_t error_info = {
        .line_num    = line_num,
        .p_file_name = p_file_name,
        .err_code    = error_code,
    };
    app_error_fault_handler(0, 0, (long) (&error_info));
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  *         Only when USE_FULL_ASSERT is defined
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    LOG_ERROR("Optional assert failed: %s:%lu", file, line);
}
#endif