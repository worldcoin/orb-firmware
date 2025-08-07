#if defined(CONFIG_BOARD_PEARL_MAIN)
#include "gnss/gnss.h"
#endif
#include "mcu.pb.h"
#include "optics/optics.h"
#include "orb_logs.h"
#include "power/battery/battery.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "runner/runner.h"
#include "system/diag.h"
#include "system/version/version.h"
#include "temperature/fan/fan.h"
#include "temperature/fan/fan_tach.h"
#include "temperature/sensors/temperature.h"
#include "ui/ambient_light/als.h"
#include "ui/button/button.h"
#include "ui/sound/sound.h"
#include "ui/ui.h"
#include "voltage_measurement/voltage_measurement.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <dfu.h>
#include <optics/polarizer_wheel/polarizer_wheel.h>
#include <orb_fatal.h>
#include <orb_state.h>
#include <pb_encode.h>
#include <storage.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#ifdef CONFIG_MEMFAULT
#include <memfault/core/reboot_tracking.h>
#endif

#if CONFIG_ORB_LIB_WATCHDOG
#include <watchdog.h>
#endif

#ifdef CONFIG_ORB_LIB_HEALTH_MONITORING
#include "heartbeat.h"
#include "system/logs.h"
#endif

LOG_MODULE_REGISTER(main, CONFIG_MAIN_LOG_LEVEL);

static bool jetson_up_and_running = false;

static K_MUTEX_DEFINE(analog_and_i2c_mutex);

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

// generic tests
ZTEST_SUITE(hil, NULL, NULL, NULL, NULL, NULL);

// dfu unit tests
#include "system/dfu/dfu_tests.h"
ZTEST_SUITE(dfu, NULL, NULL, NULL, dfu_test_reset, NULL);

// ir camera unit tests
#include "optics/ir_camera_system/ir_camera_system.h"
ZTEST_SUITE(ir_camera, NULL, NULL, ir_camera_test_reset, ir_camera_test_reset,
            NULL);
#endif

/**
 * @brief execute ZTests or other runtime tests
 */
static void
run_tests()
{
#if defined(CONFIG_ZTEST)
    // Per default publishing of voltages is disabled
    // -> enable it for testing if voltage messages are published
    voltage_measurement_set_publish_period(1000);

    ztest_run_all(NULL, false, 1, 1);
    ztest_verify_all_test_suites_ran();
#else
    fan_tach_self_test();

    const int err_code = voltage_measurement_selftest();
    ASSERT_SOFT(err_code);
#endif

#if defined(CONFIG_ORB_LIB_ERRORS_TESTS)
    fatal_errors_trigger(FATAL_RANDOM);
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
        static orb_mcu_McuMessage fatal_error = {
            .which_message = orb_mcu_McuMessage_m_message_tag,
            .message.m_message.which_payload =
                orb_mcu_main_McuToJetson_fatal_error_tag,
            .message.m_message.payload.fatal_error.reason =
                orb_mcu_FatalError_FatalReason_FATAL_ASSERT_HARD,
        };

        static uint8_t buffer[CAN_FRAME_MAX_SIZE];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        bool encoded = pb_encode_ex(&stream, orb_mcu_McuMessage_fields,
                                    &fatal_error, PB_ENCODE_DELIMITED);

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

/**
 * Called from thread so it's fine to call k_msleep
 * @return doesn't return
 */
static int
heartbeat_timeout_handler(void)
{
    const int32_t shutdown_delay_ms = 5000;
    const orb_mcu_main_ShutdownScheduled shutdown = {
        .shutdown_reason =
            orb_mcu_main_ShutdownScheduled_ShutdownReason_HEARTBEAT_TIMEOUT,
        .has_ms_until_shutdown = true,
        .ms_until_shutdown = shutdown_delay_ms};
    publish_new((void *)&shutdown, sizeof(shutdown),
                orb_mcu_main_McuToJetson_shutdown_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

    k_msleep(shutdown_delay_ms);

    // ☠️
#ifdef CONFIG_MEMFAULT
    MEMFAULT_REBOOT_MARK_RESET_IMMINENT(
        kMfltRebootReason_HeartbeatFromJetsonTimeout);
#endif

    NVIC_SystemReset();
    return 0;
}

static void
send_reset_reason(void)
{
    uint32_t reset_reason = fatal_get_status_register();
    if (reset_reason != 0) {
        orb_mcu_FatalError fatal_error = {0};
        if (IS_WATCHDOG(reset_reason)) {
            fatal_error.reason = orb_mcu_FatalError_FatalReason_FATAL_WATCHDOG;
            publish_new(&fatal_error, sizeof(fatal_error),
                        orb_mcu_main_McuToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_SOFTWARE(reset_reason)) {
            fatal_error.reason =
                orb_mcu_FatalError_FatalReason_FATAL_SOFTWARE_UNKNOWN;
            publish_new(&fatal_error, sizeof(fatal_error),
                        orb_mcu_main_McuToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_BOR(reset_reason)) {
            fatal_error.reason = orb_mcu_FatalError_FatalReason_FATAL_BROWNOUT;
            publish_new(&fatal_error, sizeof(fatal_error),
                        orb_mcu_main_McuToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_PIN(reset_reason)) {
            fatal_error.reason = orb_mcu_FatalError_FatalReason_FATAL_PIN_RESET;
            publish_new(&fatal_error, sizeof(fatal_error),
                        orb_mcu_main_McuToJetson_fatal_error_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
        if (IS_LOW_POWER(reset_reason)) {
            fatal_error.reason = orb_mcu_FatalError_FatalReason_FATAL_LOW_POWER;
            publish_new(&fatal_error, sizeof(fatal_error),
                        orb_mcu_main_McuToJetson_fatal_error_tag,
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

static void
initialize(void)
{
    int err_code;

    fatal_init();

    err_code = storage_init();
    ASSERT_SOFT(err_code);

    // initialize runner before communication modules
    runner_init();

    app_assert_init(app_assert_cb);

#if CONFIG_ORB_LIB_WATCHDOG && !(CONFIG_ORB_LIB_WATCHDOG_SYS_INIT)
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

    err_code = version_init();
    ASSERT_SOFT(err_code);

    orb_mcu_Hardware hw = version_get();

    // voltage_measurement module is used by battery and boot -> must be
    // initialized before
    err_code = voltage_measurement_init(&hw, &analog_and_i2c_mutex);
    ASSERT_SOFT(err_code);

    // logs over CAN must be initialized after CAN-messaging module
#if defined(CONFIG_ORB_LIB_LOGS_CAN) && !defined(CONFIG_ZTEST)
    err_code = logs_init(logs_can);
    ASSERT_SOFT(err_code);
#endif

#ifdef CONFIG_ORB_LIB_HEALTH_MONITORING
    heartbeat_register_cb(heartbeat_timeout_handler);
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

    temperature_init(&hw, &analog_and_i2c_mutex);

    err_code = sound_init();
    ASSERT_SOFT(err_code);

    err_code = ui_init(&hw);
    ASSERT_SOFT(err_code);

    err_code = als_init(&hw, &analog_and_i2c_mutex);
    ASSERT_SOFT(err_code);

    err_code = dfu_init();
    ASSERT_SOFT(err_code);

    err_code = button_init();
    ASSERT_SOFT(err_code);

#if defined(CONFIG_BOARD_PEARL_MAIN)
    err_code = gnss_init();
    ASSERT_SOFT(err_code);
#endif

    // wait that jetson boots to enable super-caps as it's drawing a lot of
    // current that is needed for proper jetson boot
#if !defined(CONFIG_NO_SUPER_CAPS) && !defined(CONFIG_CI_INTEGRATION_TESTS)
    k_msleep(14000);
    err_code = boot_turn_on_super_cap_charger();
    if (err_code == RET_SUCCESS) {
        // Delay is to wait for super-cap to charge enough so that turning on
        // PVCC doesn't cause a brownout, which then disable PVCC (circuitry)
        // back and forth until stabilized. VCaps voltage is thus kept stable.
        // Ideally, we should measure the super-cap voltage but hardcoding a
        // delay works for now.
        k_msleep(6000);
        err_code = boot_turn_on_pvcc();
        if (err_code == RET_SUCCESS) {
            err_code = optics_init(&hw, &analog_and_i2c_mutex);
            ASSERT_SOFT(err_code);
        } else {
            ASSERT_SOFT(err_code);
        }
    } else {
        ASSERT_SOFT(err_code);
    }
#else
    err_code = optics_init(&hw, &analog_and_i2c_mutex);
    ASSERT_SOFT(err_code);
#endif // CONFIG_NO_SUPER_CAPS

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (hw.version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_4 ||
        hw.version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_EVT) {
        // on diamond evt, timer2 is used by fan tach & stepper but
        // pwm cannot be used as output and input for same timer
        // so we default to polarizer if one is detected
        // wait 10 seconds for polarizer homing to finish, if unsuccessful (no
        // polarizer dectected?): use fan tach
        k_msleep(10000);
        if (!polarizer_wheel_homed()) {
            err_code = fan_tach_init();
            ASSERT_SOFT(err_code);
        }
    } else {
        err_code = fan_tach_init();
        ASSERT_SOFT(err_code);
    }
#else
    err_code = fan_tach_init();
    ASSERT_SOFT(err_code);
#endif

    // done booting
    LOG_INF("🚀");
}

#ifdef CONFIG_ZTEST
void
test_main(void)
{
    initialize();
    run_tests();
}
#else
int
main(void)
{
    initialize();
    run_tests();

    // print states and test results
#ifdef DEBUG
    orb_mcu_Hardware hw = version_get();
    LOG_INF("Hardware version: main board: %u, power board: %u, front-unit: "
            "%u, reset board: %u",
            hw.version, hw.power_board, hw.front_unit, hw.reset_board);
    orb_state_dump(NULL);
#endif

    wait_jetson_up();

#if defined(DEBUG) && !defined(CONFIG_SHELL)
    while (true) {
        orb_state_dump(NULL);
        k_sleep(K_SECONDS(30));
    }
#endif

    return 0;
}
#endif
