#ifndef LIQUID_LENS_H
#define LIQUID_LENS_H

#include <errors.h>
#include <stdbool.h>
#include <stdint.h>

void
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
