#include "button/button.h"
#include "fan/fan.h"
#include "ir_camera_system/ir_camera_system.h"
#include "power_sequence/power_sequence.h"
#include "ui/front_leds/front_leds.h"
#include "ui/operator_leds/operator_leds.h"
#include <device.h>
#include <drivers/gpio.h>
#ifdef CONFIG_TEST_IR_CAMERA_SYSTEM
#include <ir_camera_system/ir_camera_system_test.h>
#endif
#include "liquid_lens/liquid_lens.h"
#include "messaging/incoming_message_handling.h"
#include "sound/sound.h"
#include "stepper_motors/motors_tests.h"
#include "stepper_motors/stepper_motors.h"
#include "ui/front_leds/front_leds_tests.h"
#include "ui/operator_leds/operator_leds_tests.h"
#include "ui/rgb_leds.h"
#include "version/version.h"

#ifdef CONFIG_ORB_LIB_HEALTH_MONITORING
#include "heartbeat.h"
#endif

#if CONFIG_BOARD_STM32G484_EVAL
#include "messaging/messaging_tests.h"
#endif

#include <temperature/temperature.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main);
#include <can_messaging.h>
#include <dfu.h>
#include <dfu/tests.h>
#include <zephyr.h>

void
run_tests()
{
#ifdef CONFIG_BOARD_STM32G484_EVAL
    LOG_WRN("Running tests");
    messaging_tests_init();
#endif
#ifdef CONFIG_TEST_MOTORS
    motors_tests_init();
#endif
#ifdef CONFIG_TEST_DFU
    tests_dfu_init();
#endif
#ifdef CONFIG_TEST_OPERATOR_LEDS
    operator_leds_tests_init();
#endif
#ifdef CONFIG_TEST_USER_LEDS
    front_unit_rdb_leds_tests_init();
#endif
}

#ifdef CONFIG_TEST_IR_CAMERA_SYSTEM
void
main()
{
    __ASSERT(ir_camera_system_init() == 0,
             "Error initializing IR camera system");
    ir_camera_system_test();
}
#else // CONFIG_TEST_IR_CAMERA_SYSTEM
void
main(void)
{
    __ASSERT(power_turn_on_jetson() == 0, "Jetson power-on error");
    __ASSERT(init_sound() == 0, "Error initializing sound");

    __ASSERT(front_leds_init() == 0, "Error doing front unit RGB LEDs");
    __ASSERT(operator_leds_init() == 0, "Error doing operator RGB LEDs");

    __ASSERT(power_turn_on_super_cap_charger() == 0,
             "Error enabling super cap charger");
    __ASSERT(power_turn_on_pvcc() == 0, "Error turning on pvcc");
    __ASSERT(ir_camera_system_init() == 0,
             "Error initializing IR camera system");
    __ASSERT(motors_init() == 0, "Error initializing motors");
    __ASSERT(liquid_lens_init() == 0, "Error initializing liquid lens");

    // init messaging first
    // temperature module depends on messaging
    can_messaging_init(incoming_message_handle);
    dfu_init();
    temperature_init();
    button_init();

    // set up operator LED depending on image state
    if (dfu_primary_is_confirmed()) {
        operator_leds_set_pattern(
            DistributorLEDsPattern_DistributorRgbLedPattern_ALL_GREEN,
            OPERATOR_LEDS_ALL_MASK, NULL);
    } else {
        RgbColor custom = RGB_ORANGE;
        operator_leds_set_pattern(
            DistributorLEDsPattern_DistributorRgbLedPattern_RGB,
            OPERATOR_LEDS_ALL_MASK, &custom);
    }

    // launch tests if any is defined
    run_tests();

    // we consider the image working if no errors were reported before
    // Jetson sent first messages, and we didn't report any error
    bool jetson_up_and_running = false;
    while (1) {
        k_msleep(10000);

        // as soon as the Jetson sends the first message, send firmware version
        // come back after the delay above and another message from the Jetson
        // to confirm the image
        if (!jetson_up_and_running && incoming_message_acked_counter() > 0) {
            version_send();

            if (app_assert_count()) {
                // do not confirm image
                return;
            }

            jetson_up_and_running = true;
            continue;
        }

        if (jetson_up_and_running && incoming_message_acked_counter() > 1) {
            // the orb is now up and running
            LOG_INF("Confirming image");
            int err_code = dfu_primary_confirm();
            if (err_code == 0) {
                operator_leds_set_pattern(
                    DistributorLEDsPattern_DistributorRgbLedPattern_ALL_GREEN,
                    OPERATOR_LEDS_ALL_MASK, NULL);
            }

            return;
        }
    }
}

#endif // CONFIG_TEST_IR_CAMERA_SYSTEM
