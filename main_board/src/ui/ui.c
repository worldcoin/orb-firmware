#include "ui.h"
#include "pubsub/pubsub.h"
#include "ui/rgb_leds/front_leds/front_leds.h"
#include "ui/rgb_leds/operator_leds/operator_leds.h"
#include <app_assert.h>

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#include "ui/rgb_leds/cone_leds/cone_leds.h"
#include "ui/white_leds/white_leds.h"
#include <zephyr/toolchain/gcc.h>
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

#if defined(CONFIG_DT_HAS_DIAMOND_CONE_ENABLED)
    BUILD_ASSERT(CONFIG_SC18IS606_INIT_PRIO > CONFIG_I2C_INIT_PRIO_INST_2,
                 "I2C4 must be configured before using it by SC16IS606.");
    BUILD_ASSERT(CONFIG_LED_STRIP_INIT_PRIORITY >
                     CONFIG_SC18IS606_CHANNEL_INIT_PRIO,
                 "SC18IS606 must be configured before using it by the LED "
                 "strip driver.");

    cone_present = (cone_leds_init() == RET_SUCCESS);
    ui_cone_present_send(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
#endif
#endif

    return RET_SUCCESS;
}
