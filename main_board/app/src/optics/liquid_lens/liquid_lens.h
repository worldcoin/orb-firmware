#ifndef LIQUID_LENS_H
#define LIQUID_LENS_H

#include <errors.h>
#include <stdbool.h>
#include <stdint.h>

#define LIQUID_LENS_MIN_CURRENT_MA (-400)
#define LIQUID_LENS_MAX_CURRENT_MA (400)

/** Sets the target current to maintain
 * @param target_current the target current clipped to [-400,400]
 * @return error code:
 *  - RET_SUCCESS: All good
 *  - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
 */
ret_code_t
liquid_set_target_current_ma(int32_t target_current);
ret_code_t
liquid_lens_init(void);
void
liquid_lens_enable(void);
void
liquid_lens_disable(void);
bool
liquid_lens_is_enabled(void);

#endif // LIQUID_LENS_H
