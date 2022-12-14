#include "1d_tof/tof_1d.h"
#include "ambient_light/als.h"
#include "battery/battery.h"
#include "button/button.h"
#include "fan/fan.h"
#include "fan/fan_tach.h"
#include "fan/fan_tests.h"
#include "gnss/gnss.h"
#include "ir_camera_system/ir_camera_system.h"
#include "liquid_lens/liquid_lens.h"
#include "power_sequence/power_sequence.h"
#include "pubsub/pubsub.h"
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
#include <dfu.h>
#include <dfu/dfu_tests.h>
#include <ir_camera_system/ir_camera_system_tests.h>
#include <pb_encode.h>
#include <storage.h>
#include <storage_tests.h>
#include <temperature/temperature.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_ORB_LIB_HEALTH_MONITORING
#include "heartbeat.h"
#endif

#include <zephyr/logging/log.h>
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
#if defined(CONFIG_TEST_FAN) || defined(RUN_ALL_TESTS)
    fan_tests_init();
#endif
#if defined(CONFIG_ORB_LIB_STORAGE_TESTS) || defined(RUN_ALL_TESTS)
    storage_tests();
#endif
#ifdef CONFIG_ORB_LIB_ERRORS_TESTS
    fatal_errors_test();
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
        static McuMessage fatal_error = {
            .which_message = McuMessage_m_message_tag,
            .message.m_message.which_payload = McuToJetson_fatal_error_tag,
            .message.m_message.payload.fatal_error.reason =
                FatalError_FatalReason_FATAL_ASSERT_HARD,
        };

        static uint8_t buffer[CAN_FRAME_MAX_SIZE];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &fatal_error,
                                    PB_ENCODE_DELIMITED);

        can_message_t to_send = {.destination =
                                     CONFIG_CAN_ADDRESS_DEFAULT_REMOTE,
                                 .bytes = buffer,
                                 .size = stream.bytes_written};

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

    LOG_INF("🚀");

    err_code = storage_init();
    ASSERT_SOFT(err_code);

    err_code = logs_init();
    ASSERT_SOFT(err_code);

    app_assert_init(app_assert_cb);

#if CONFIG_ORB_LIB_CAN_MESSAGING
    err_code = can_messaging_init(runner_handle_new_can);
    ASSERT_SOFT(err_code);
#endif

#if CONFIG_ORB_LIB_UART_MESSAGING
    err_code = uart_messaging_init(runner_handle_new_uart);
    ASSERT_SOFT(err_code);
#endif

    // check battery state early on
    err_code = battery_init();
    ASSERT_SOFT(err_code);

#ifndef CONFIG_NO_JETSON_BOOT
    err_code = power_turn_on_jetson();
    ASSERT_SOFT(err_code);
#endif // CONFIG_NO_JETSON_BOOT

    err_code = fan_init();
    ASSERT_SOFT(err_code);

    temperature_init();

    err_code = sound_init();
    ASSERT_SOFT(err_code);

    err_code = front_leds_init();
    ASSERT_SOFT(err_code);

    err_code = operator_leds_init();
    ASSERT_SOFT(err_code);

#ifndef CONFIG_NO_SUPER_CAPS
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
#else
    err_code = ir_camera_system_init();
    ASSERT_SOFT(err_code);
#endif // CONFIG_NO_SUPER_CAPS

    err_code = motors_init();
    ASSERT_SOFT(err_code);

    err_code = liquid_lens_init();
    ASSERT_SOFT(err_code);

    err_code = tof_1d_init();
    ASSERT_SOFT(err_code);

    err_code = als_init();
    ASSERT_SOFT(err_code);

    err_code = dfu_init();
    ASSERT_SOFT(err_code);

    Hardware hw;
    err_code = version_get_hardware_rev(&hw);
    ASSERT_SOFT(err_code);
    LOG_INF("Hardware version: %u", hw.version);

    err_code = button_init();
    ASSERT_SOFT(err_code);

    err_code = gnss_init();
    ASSERT_SOFT(err_code);

    err_code = fan_tach_init();
    ASSERT_SOFT(err_code);

    // launch tests if any is defined
    run_tests();

    dfu_primary_confirm();

    // enable reboot of the Orb <=> turning off the Orb
    // if Jetson is turned off
    power_reboot_set_pending();

    // wait for Jetson to show activity before sending our version
    while (!jetson_up_and_running) {
        k_msleep(5000);

        // as soon as the Jetson sends the first message, send firmware version
        if (runner_successful_jobs_count() > 0) {
            fw_version_send(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

            uint32_t error_count = app_assert_soft_count();
            if (error_count) {
                LOG_ERR("Error count during boot: %u", error_count);
            }

            jetson_up_and_running = true;
        }
    }
}
