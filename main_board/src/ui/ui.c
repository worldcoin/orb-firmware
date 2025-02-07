#include "ui.h"
#include "pubsub/pubsub.h"
#include "ui/rgb_leds/front_leds/front_leds.h"
#include "ui/rgb_leds/operator_leds/operator_leds.h"
#include <app_assert.h>

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#include "ui/rgb_leds/cone_leds/cone_leds.h"
#include "ui/white_leds/white_leds.h"
#include <zephyr/toolchain.h>
#endif

static bool cone_present = false;

void
ui_cone_present_send(uint32_t remote)
{
    orb_mcu_main_ConePresent cone_status = {
        .cone_present = cone_present,
    };
    publish_new(&cone_status, sizeof(cone_status),
                orb_mcu_main_McuToJetson_cone_present_tag, remote);
}

int
ui_init(void)
{
    int err_code;

    err_code = front_leds_init();
    ASSERT_SOFT(err_code);

    err_code = operator_leds_init();
    ASSERT_SOFT(err_code);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    err_code = white_leds_init();
    ASSERT_SOFT(err_code);
#endif

    return RET_SUCCESS;
}
