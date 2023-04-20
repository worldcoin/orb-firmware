#include "gnss/gnss.h"
#include "optics/1d_tof/tof_1d.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/liquid_lens/liquid_lens.h"
#include "optics/mirrors/mirrors.h"
#include "power/battery/battery.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "runner/runner.h"
#include "system/logs.h"
#include "system/version/version.h"
#include "temperature/fan/fan.h"
#include "temperature/fan/fan_tach.h"
#include "temperature/sensors/temperature.h"
#include "ui/ambient_light/als.h"
#include "ui/button/button.h"
#include "ui/front_leds/front_leds.h"
#include "ui/operator_leds/operator_leds.h"
#include "ui/sound/sound.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <dfu.h>
#include <pb_encode.h>
#include <storage.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_ORB_LIB_HEALTH_MONITORING
#include "heartbeat.h"
#endif

#include <fatal.h>
#include <watchdog.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

static bool jetson_up_and_running = false;

#ifdef CONFIG_ZTEST_NEW_API
#include <zephyr/ztest.h>

ZTEST_SUITE(hil, NULL, NULL, NULL, NULL, NULL);
#endif

/**
 * @brief execute ZTests or other runtime tests
 */
static void
run_tests()
{
#if defined(CONFIG_ZTEST_NEW_API)
    ztest_run_all(NULL);
    ztest_verify_all_test_suites_ran();
#endif

#if defined(CONFIG_ORB_LIB_ERRORS_TESTS)
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

static void
send_reset_reason(void)
{
    uint32_t reset_reason = fatal_get_status_register();
    if (reset_reason != 0) {
        FatalError fatal_error = {0};
        if (IS_WATCHDOG(reset_reason)) {
            fatal_error.reason = FatalError_FatalReason_FATAL_WATCHDOG;
            publish_new(&fatal_error, sizeof(fatal_error),
                        SecToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_SOFTWARE(reset_reason)) {
            fatal_error.reason = FatalError_FatalReason_FATAL_SOFTWARE_UNKNOWN;
            publish_new(&fatal_error, sizeof(fatal_error),
                        SecToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_BOR(reset_reason)) {
            fatal_error.reason = FatalError_FatalReason_FATAL_BROWNOUT;
            publish_new(&fatal_error, sizeof(fatal_error),
                        SecToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_PIN(reset_reason)) {
            fatal_error.reason = FatalError_FatalReason_FATAL_PIN_RESET;
            publish_new(&fatal_error, sizeof(fatal_error),
                        SecToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_LOW_POWER(reset_reason)) {
            fatal_error.reason = FatalError_FatalReason_FATAL_LOW_POWER;
            publish_new(&fatal_error, sizeof(fatal_error),
                        SecToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
    }
}

__maybe_unused static void
wait_jetson_up(void)
{
    // wait for Jetson to show activity before sending our version
    while (!jetson_up_and_running) {
        k_msleep(5000);

        // as soon as the Jetson sends the first message, send firmware version
        if (runner_successful_jobs_count() > 0) {
            version_fw_send(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

            uint32_t error_count = app_assert_soft_count();
            if (error_count) {
                LOG_ERR("Error count during boot: %u", error_count);
            }

            send_reset_reason();

            jetson_up_and_running = true;
        }
    }
}

void
initialize(void)
{
    int err_code;

    fatal_init();

    LOG_INF("🚀");

    err_code = storage_init();
    ASSERT_SOFT(err_code);

    err_code = logs_init();
    ASSERT_SOFT(err_code);

    app_assert_init(app_assert_cb);

#ifndef CONFIG_ORB_LIB_ERRORS_TESTS
    err_code = watchdog_init();
    ASSERT_SOFT(err_code);
#endif

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
    err_code = boot_turn_on_jetson();
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
    err_code = boot_turn_on_super_cap_charger();
    if (err_code == RET_SUCCESS) {
        err_code = boot_turn_on_pvcc();
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

    err_code = mirrors_init();
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
}

#ifdef CONFIG_ZTEST
void
test_main(void)
{
    initialize();
    run_tests();
}
#else
void
main(void)
{
    initialize();
    run_tests();
    wait_jetson_up();
}
#endif
