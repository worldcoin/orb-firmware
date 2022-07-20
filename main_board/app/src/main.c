#include "battery/battery.h"
#include "button/button.h"
#include "fan/fan.h"
#include "ir_camera_system/ir_camera_system.h"
#include "liquid_lens/liquid_lens.h"
#include "power_sequence/power_sequence.h"
#include "runner/runner.h"
#include "sound/sound.h"
#include "stepper_motors/motors_tests.h"
#include "stepper_motors/stepper_motors.h"
#include "system/logs.h"
#include "ui/front_leds/front_leds.h"
#include "ui/front_leds/front_leds_tests.h"
#include "ui/operator_leds/operator_leds.h"
#include "ui/operator_leds/operator_leds_tests.h"
#include "ui/rgb_leds.h"
#include "version/version.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <device.h>
#include <dfu.h>
#include <dfu/dfu_tests.h>
#include <ir_camera_system/ir_camera_system_tests.h>
#include <pb_encode.h>
#include <temperature/temperature.h>
#include <zephyr.h>

#ifdef CONFIG_ORB_LIB_HEALTH_MONITORING
#include "heartbeat.h"
#include "pubsub/pubsub.h"
#endif

#include <logging/log.h>
LOG_MODULE_REGISTER(main);

static bool jetson_up_and_running = false;

void
run_tests()
{
#if defined(CONFIG_TEST_MOTORS) || defined(RUN_ALL_TESTS)
    motors_tests_init();
#endif
#if defined(CONFIG_TEST_DFU) || defined(RUN_ALL_TESTS)
    dfu_tests_init();
#endif
#if defined(CONFIG_TEST_OPERATOR_LEDS) || defined(RUN_ALL_TESTS)
    operator_leds_tests_init();
#endif
#if defined(CONFIG_TEST_USER_LEDS) || defined(RUN_ALL_TESTS)
    front_unit_rdb_leds_tests_init();
#endif
#if defined(CONFIG_TEST_IR_CAMERA_SYSTEM) || defined(RUN_ALL_TESTS)
    ir_camera_system_tests_init();
#endif
}

/**
 * Callback called in fatal assertion before system reset
 * ⚠️ No context-switch should be performed:
 *     to be provided by the caller of this function
 */
static void
app_assert_cb(fatal_error_info_t *err_info)
{
    if (jetson_up_and_running) {
        // fatal error, try to warn Jetson
        McuMessage fatal_error = {.which_message = McuMessage_m_message_tag,
                                  .message.m_message.which_payload =
                                      McuToJetson_fatal_error_tag};
        can_message_t to_send;
        pb_ostream_t stream =
            pb_ostream_from_buffer(to_send.bytes, sizeof(to_send.bytes));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &fatal_error,
                                    PB_ENCODE_DELIMITED);
        to_send.size = stream.bytes_written;
        to_send.destination = CONFIG_CAN_ADDRESS_DEFAULT_REMOTE;

        if (encoded) {
            // important: send in blocking mode
            (void)can_messaging_blocking_tx(&to_send);
        }
    } else if (err_info != NULL) {
        // TODO store error
    }
}

void
main(void)
{
    int err_code;

    app_assert_init(app_assert_cb);

    // CAN initialization first to allow logs over CAN
    err_code = can_messaging_init(runner_handle_new);
    ASSERT_SOFT(err_code);

    err_code = logs_init();
    ASSERT_SOFT(err_code);

    err_code = power_turn_on_jetson();
    ASSERT_SOFT(err_code);

    err_code = sound_init();
    ASSERT_SOFT(err_code);

    err_code = front_leds_init();
    ASSERT_SOFT(err_code);

    err_code = operator_leds_init();
    ASSERT_SOFT(err_code);

    // PVCC supplies IR LEDs
    err_code = power_turn_on_super_cap_charger();
    if (err_code == RET_SUCCESS) {
        err_code = power_turn_on_pvcc();
        if (err_code == RET_SUCCESS) {
            err_code = ir_camera_system_init();
            ASSERT_SOFT(err_code);
        } else {
            ASSERT_SOFT(err_code);
        }
    } else {
        ASSERT_SOFT(err_code);
    }

    err_code = motors_init();
    ASSERT_SOFT(err_code);

    err_code = liquid_lens_init();
    ASSERT_SOFT(err_code);

    err_code = dfu_init();
    ASSERT_SOFT(err_code);

    uint16_t hw = 0;
    err_code = version_get_hardware_rev(&hw);
    ASSERT_SOFT(err_code);
    LOG_INF("Hardware version: %u", hw);

    temperature_init();

    err_code = button_init();
    ASSERT_SOFT(err_code);

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    err_code = battery_init();
    ASSERT_SOFT(err_code);
#endif

    // set up operator LED depending on image state
    if (dfu_primary_is_confirmed()) {
        operator_leds_set_pattern(
            DistributorLEDsPattern_DistributorRgbLedPattern_ALL_GREEN,
            OPERATOR_LEDS_ALL_MASK, NULL);
    } else {
        operator_leds_set_pattern(
            DistributorLEDsPattern_DistributorRgbLedPattern_RGB,
            OPERATOR_LEDS_ALL_MASK, &(RgbColor)RGB_ORANGE);
    }

    // launch tests if any is defined
    run_tests();

    // we consider the image working if no errors were reported before
    // Jetson sent first messages, and we didn't report any error
    while (1) {
        k_msleep(10000);

        // as soon as the Jetson sends the first message, send firmware version
        // come back after the delay above and another message from the Jetson
        // to confirm the image
        if (!jetson_up_and_running && runner_successful_jobs_count() > 0) {
            version_send(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

            uint32_t error_count = app_assert_soft_count();
            if (error_count) {
                LOG_ERR("Error count during boot: %u", error_count);

                // do not confirm image
                return;
            }

            jetson_up_and_running = true;
            continue;
        }

        if (jetson_up_and_running && runner_successful_jobs_count() > 1) {
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
