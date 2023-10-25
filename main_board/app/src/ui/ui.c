#include "ui.h"
#include "ui/front_leds/front_leds.h"
#include "ui/operator_leds/operator_leds.h"
#include <app_assert.h>

int
ui_init(void)
{
    int err_code;

    err_code = front_leds_init();
    ASSERT_SOFT(err_code);

    err_code = operator_leds_init();
    ASSERT_SOFT(err_code);

    return RET_SUCCESS;
}
