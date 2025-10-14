
#include "cli.h"

#include "date.h"
#include "orb_logs.h"

#include <bootutil/image.h>
#include <compilers.h>
#include <main.pb.h>
#include <optics/ir_camera_system/ir_camera_timer_settings.h>
#include <orb_state.h>
#include <power/battery/battery.h>
#include <runner/runner.h>
#include <stdlib.h>
#include <system/version/version.h>
#include <ui/rgb_leds/operator_leds/operator_leds.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(cli, LOG_LEVEL_INF);

static int
execute_reboot(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: orb reboot <delay_s>");
        return -EINVAL;
    }

    uint32_t delay = strtoul(argv[1], NULL, 10);
    if (delay > 60) {
        shell_error(sh, "Delay must be 0-60 seconds");
        return -EINVAL;
    }

    orb_mcu_main_JetsonToMcu message = {.which_payload =
                                            orb_mcu_main_JetsonToMcu_reboot_tag,
                                        .payload.reboot.delay = delay};

    shell_print(sh, "Rebooting in %u seconds", delay);
    return runner_handle_new_cli(&message);
}

static int
execute_version(const struct shell *sh, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    version_print((void *)sh);

    return 0;
}

static int
execute_state(const struct shell *sh, size_t argc, char **argv)
{
    UNUSED_PARAMETER(sh);
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);
    shell_print(sh, "Hardware component states:");
    orb_state_dump(sh);

    return 0;
}

static int
execute_battery(const struct shell *sh, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    battery_dump_stats(sh);

    return 0;
}

static int
execute_fan(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: orb fan <speed>");
        return -EINVAL;
    }

    uint32_t speed = strtoul(argv[1], NULL, 10);
    if (speed > 100) {
        shell_error(sh, "Fan speed must be 0-100");
        return -EINVAL;
    }

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_fan_speed_tag,
        .payload.fan_speed.payload.value = speed};

    shell_print(sh, "Setting fan speed to %u", speed);
    return runner_handle_new_cli(&message);
}

static int
execute_mirror(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(
            sh, "Usage: orb mirror <phi_millidegrees> <theta_millidegrees>");
        return -EINVAL;
    }

    uint32_t phi = strtoul(argv[1], NULL, 10);
    uint32_t theta = strtoul(argv[2], NULL, 10);

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_mirror_angle_tag,
        .payload.mirror_angle = {.angle_type =
                                     orb_mcu_main_MirrorAngleType_PHI_THETA,
                                 .phi_angle_millidegrees = phi,
                                 .theta_angle_millidegrees = theta}};

    shell_print(sh, "Setting mirror angles: phi=%u, theta=%u (millidegrees)",
                phi, theta);
    return runner_handle_new_cli(&message);
}

static int
execute_heartbeat(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: orb heartbeat <timeout_seconds>");
        return -EINVAL;
    }

    uint32_t timeout = strtoul(argv[1], NULL, 10);

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_heartbeat_tag,
        .payload.heartbeat.timeout_seconds = timeout};

    shell_print(sh, "Sending heartbeat with timeout %u seconds", timeout);
    return runner_handle_new_cli(&message);
}

static int
execute_voltage(const struct shell *sh, size_t argc, char **argv)
{
    // TODO print voltages
    if (argc < 2) {
        shell_error(sh, "Usage: orb voltage <period_ms>");
        return -EINVAL;
    }

    uint32_t period = strtoul(argv[1], NULL, 10);

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_voltage_request_tag,
        .payload.voltage_request.transmit_period_ms = period};

    shell_print(sh, "Requesting voltage measurements every %u ms", period);
    return runner_handle_new_cli(&message);
}

static int
execute_liquid_lens(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(sh, "Usage: orb liquid_lens <current_ma> <enable>");
        return -EINVAL;
    }

    int32_t current = strtol(argv[1], NULL, 10);
    bool enable = (strcmp(argv[2], "1") == 0 || strcmp(argv[2], "true") == 0);

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_liquid_lens_tag,
        .payload.liquid_lens = {.current = current, .enable = enable}};

    shell_print(sh, "Setting liquid lens: current=%d mA, enable=%s", current,
                enable ? "true" : "false");
    return runner_handle_new_cli(&message);
}

static int
execute_homing(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(sh, "Usage: orb homing <mode> <axis>");
        shell_error(
            sh,
            "Mode: 0=stall_detection, 1=one_blocking_end, 2=known_position");
        shell_error(sh, "Axis: 0=both, 1=phi, 2=theta");
        return -EINVAL;
    }

    uint32_t mode = strtoul(argv[1], NULL, 10);
    uint32_t axis = 0;
    if (argc == 3) {
        axis = strtoul(argv[2], NULL, 10);
    }

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_do_homing_tag,
        .payload.do_homing = {.homing_mode = mode, .angle = axis}};

    shell_print(sh, "Starting mirror homing: mode=%u, axis=%u", mode, axis);
    return runner_handle_new_cli(&message);
}

static int
execute_fps(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: orb fps <fps>");
        return -EINVAL;
    }

    uint32_t fps = strtoul(argv[1], NULL, 10);

    orb_mcu_main_JetsonToMcu message = {.which_payload =
                                            orb_mcu_main_JetsonToMcu_fps_tag,
                                        .payload.fps.fps = fps};

    shell_print(sh, "Setting camera FPS to %u", fps);
    return runner_handle_new_cli(&message);
}

static int
execute_user_leds(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh,
                    "Usage: user_leds <pattern> [start_angle] [angle_length] "
                    "[r] [g] [b] [pulsing_period_ms] [pulsing_scale]");
        return -EINVAL;
    }

    orb_mcu_main_JetsonToMcu message = {0};
    message.which_payload = orb_mcu_main_JetsonToMcu_user_leds_pattern_tag;

    // Parse pattern
    int pattern = strtol(argv[1], NULL, 10);
    if (pattern < 0 ||
        pattern >
            orb_mcu_main_UserLEDsPattern_UserRgbLedPattern_RGB_ONLY_CENTER) {
        shell_error(sh, "Invalid pattern. Use 0-5");
        return -EINVAL;
    }
    message.payload.user_leds_pattern.pattern =
        (orb_mcu_main_UserLEDsPattern_UserRgbLedPattern)pattern;

    // Set defaults
    message.payload.user_leds_pattern.start_angle = 0;
    message.payload.user_leds_pattern.angle_length = 360;
    message.payload.user_leds_pattern.pulsing_period_ms = 1000;
    message.payload.user_leds_pattern.pulsing_scale = 1.0f;
    message.payload.user_leds_pattern.has_custom_color = false;

    // Parse optional parameters
    if (argc > 2) {
        message.payload.user_leds_pattern.start_angle =
            strtoul(argv[2], NULL, 10);
    }
    if (argc > 3) {
        message.payload.user_leds_pattern.angle_length =
            strtol(argv[3], NULL, 10);
    }
    if (argc > 6) {
        // Custom color provided
        message.payload.user_leds_pattern.has_custom_color = true;
        message.payload.user_leds_pattern.custom_color.red =
            strtoul(argv[4], NULL, 10);
        message.payload.user_leds_pattern.custom_color.green =
            strtoul(argv[5], NULL, 10);
        message.payload.user_leds_pattern.custom_color.blue =
            strtoul(argv[6], NULL, 10);
    }
    if (argc > 7) {
        message.payload.user_leds_pattern.pulsing_period_ms =
            strtoul(argv[7], NULL, 10);
    }
    if (argc > 8) {
        message.payload.user_leds_pattern.pulsing_scale = strtof(argv[8], NULL);
    }

    ret_code_t ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to execute command: %d", ret);
        return -EIO;
    }

    shell_print(sh, "User LED pattern command sent");
    return 0;
}

static int
execute_op_leds(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: op_leds <pattern|brightness> [args...]");
        shell_print(sh, "  pattern <pattern_id> [mask] [r g b]");
        shell_print(sh, "  brightness <0-255>");
        return -EINVAL;
    }

    if (strcmp(argv[1], "pattern") == 0) {
        if (argc < 3) {
            shell_error(sh,
                        "Usage: op_leds pattern <pattern_id> [mask] [r g b]");
            return -EINVAL;
        }

        orb_mcu_main_JetsonToMcu message = {0};
        message.which_payload =
            orb_mcu_main_JetsonToMcu_distributor_leds_pattern_tag;

        // Parse pattern ID
        int pattern_id = strtol(argv[2], NULL, 0);
        if (pattern_id >= 0 &&
            pattern_id <=
                orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_BOOT_ANIMATION) {
            message.payload.distributor_leds_pattern.pattern = pattern_id;
        }

        // Parse optional mask (default to all LEDs)
        uint32_t mask = OPERATOR_LEDS_ALL_MASK;
        if (argc >= 4) {
            mask = strtoul(argv[3], NULL, 0);
        }
        message.payload.distributor_leds_pattern.leds_mask = mask;

        // Parse optional RGB color
        if (argc >= 7) {
            uint8_t r = strtoul(argv[4], NULL, 0);
            uint8_t g = strtoul(argv[5], NULL, 0);
            uint8_t b = strtoul(argv[6], NULL, 0);

            message.payload.distributor_leds_pattern.custom_color.red = r;
            message.payload.distributor_leds_pattern.custom_color.green = g;
            message.payload.distributor_leds_pattern.custom_color.blue = b;
        }

        ret_code_t ret = runner_handle_new_cli(&message);
        if (ret != RET_SUCCESS) {
            shell_error(sh, "Failed to execute command: %d", ret);
            return -EIO;
        }

        shell_print(sh, "Operator LED pattern set: %d, mask: 0x%x", pattern_id,
                    mask);
        return 0;

    } else if (strcmp(argv[1], "brightness") == 0) {
        if (argc < 3) {
            shell_error(sh, "Usage: op_leds brightness <0-255>");
            return -EINVAL;
        }

        uint32_t brightness = strtoul(argv[2], NULL, 0);
        if (brightness > 255) {
            shell_error(sh, "Brightness must be 0-255");
            return -EINVAL;
        }

        orb_mcu_main_JetsonToMcu message = {0};
        message.which_payload =
            orb_mcu_main_JetsonToMcu_distributor_leds_brightness_tag;
        message.payload.distributor_leds_brightness.brightness = brightness;

        ret_code_t ret = runner_handle_new_cli(&message);
        if (ret != RET_SUCCESS) {
            shell_error(sh, "Failed to execute command: %d", ret);
            return -EIO;
        }

        shell_print(sh, "Operator LED brightness set to: %u", brightness);
        return 0;

    } else {
        shell_error(sh, "Unknown subcommand: %s", argv[1]);
        shell_print(sh, "Available subcommands: pattern, brightness");
        return -EINVAL;
    }
}

static int
execute_power_cycle(const struct shell *sh, size_t argc, char **argv)
{
    if (argc <= 2) {
        shell_error(sh, "Usage: power_cycle <line> <duration_ms>");
        shell_print(sh, "  line: power supply line identifier");
        shell_print(
            sh,
            "    (0 = wifi_3v3, 1 = lte_3v3, 2 = sd_3v3, 4 = heat_camera_2v8)");
        shell_print(sh, "  duration_ms: duration in milliseconds");
        return -EINVAL;
    }

    uint32_t line = strtoul(argv[1], NULL, 10);
    uint32_t duration_ms = 0;
    if (argc == 3) {
        duration_ms = strtoul(argv[2], NULL, 10);
    }

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_power_cycle_tag,
        .payload.power_cycle = {.line = line, .duration_ms = duration_ms}};

    ret_code_t ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to execute power cycle command: %d", ret);
        return -EIO;
    }

    shell_print(sh, "Power cycle command sent for line %u, duration %u ms",
                line, duration_ms);
    return 0;
}

static int
execute_date(const struct shell *sh, size_t argc, char **argv)
{
    if (argc == 1) {
        date_print();
        return 0;
    }

    if (argc != 6) {
        shell_error(sh, "Usage: date [<year> <month> <day> <hour> <minute>]");
        return -EINVAL;
    }

    const uint32_t year = strtoul(argv[1], NULL, 10);
    const uint32_t month = strtoul(argv[2], NULL, 10);
    const uint32_t day = strtoul(argv[3], NULL, 10);
    const uint32_t hour = strtoul(argv[4], NULL, 10);
    const uint32_t minute = strtoul(argv[5], NULL, 10);

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_set_time_tag,
        .payload.set_time.which_format = orb_mcu_Time_human_readable_tag,
        .payload.set_time.format.human_readable = {.year = year,
                                                   .month = month,
                                                   .day = day,
                                                   .hour = hour,
                                                   .min = minute}};

    ret_code_t ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to send date command: %d", ret);
        return -EIO;
    }
    shell_print(sh, "Set date command sent for date %u/%u/%u %u:%u", year,
                month, day, hour, minute);
    return 0;
}

#ifdef CONFIG_BOARD_DIAMOND_MAIN
static int
execute_white_leds(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: white_leds <brightness>");
        shell_help(sh);
        return -EINVAL;
    }

    uint32_t brightness = strtoul(argv[1], NULL, 10);

    if (brightness > 1000) {
        shell_error(sh, "Brightness value %u out of range [0,1000]",
                    brightness);
        return -EINVAL;
    }

    orb_mcu_main_JetsonToMcu message = {
        .which_payload = orb_mcu_main_JetsonToMcu_white_leds_brightness_tag,
        .payload.white_leds_brightness.brightness = brightness};

    ret_code_t ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to send white LEDs brightness command: %d",
                    ret);
        return -EIO;
    }

    shell_print(sh, "White LEDs brightness set to %u", brightness);
    return 0;
}

static int
execute_polarizer(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: polarizer <command> [args]");
        shell_print(sh, "Commands:");
        shell_print(sh, "  home                    - Home the polarizer wheel");
        shell_print(sh,
                    "  pass_through [speed]    - Set to pass-through position");
        shell_print(
            sh,
            "  horizontal [speed]      - Set to horizontal polarization (0°)");
        shell_print(
            sh,
            "  vertical [speed]        - Set to vertical polarization (90°)");
        shell_print(
            sh, "  angle <angle> [speed]   - Set custom angle in decidegrees");
        shell_print(sh, "Speed is optional (0 = default speed)");
        return -EINVAL;
    }

    orb_mcu_main_JetsonToMcu message = {0};
    message.which_payload = orb_mcu_main_JetsonToMcu_polarizer_tag;
    message.payload.polarizer.speed = 0; // Default speed

    // Parse speed if provided (for commands that support it)
    uint32_t speed = 0;
    if (argc >= 3) {
        char *endptr;
        speed = strtoul(argv[2], &endptr, 10);
        if (*endptr != '\0') {
            shell_error(sh, "Invalid speed value: %s", argv[2]);
            return -EINVAL;
        }
    }

    if (strcmp(argv[1], "home") == 0) {
        message.payload.polarizer.command =
            orb_mcu_main_Polarizer_Command_POLARIZER_HOME;
        shell_print(sh, "Homing polarizer wheel...");
    } else if (strcmp(argv[1], "pass_through") == 0) {
        message.payload.polarizer.command =
            orb_mcu_main_Polarizer_Command_POLARIZER_PASS_THROUGH;
        message.payload.polarizer.speed = speed;
        shell_print(sh,
                    "Setting polarizer to pass-through position (speed: %u)",
                    speed);
    } else if (strcmp(argv[1], "horizontal") == 0) {
        message.payload.polarizer.command =
            orb_mcu_main_Polarizer_Command_POLARIZER_0_HORIZONTAL;
        message.payload.polarizer.speed = speed;
        shell_print(sh, "Setting polarizer to horizontal (0°) (speed: %u)",
                    speed);
    } else if (strcmp(argv[1], "vertical") == 0) {
        message.payload.polarizer.command =
            orb_mcu_main_Polarizer_Command_POLARIZER_90_VERTICAL;
        message.payload.polarizer.speed = speed;
        shell_print(sh, "Setting polarizer to vertical (90°) (speed: %u)",
                    speed);
    } else if (strcmp(argv[1], "angle") == 0) {
        if (argc < 3) {
            shell_error(sh,
                        "Angle command requires angle value in decidegrees");
            return -EINVAL;
        }

        char *endptr;
        uint32_t angle = strtoul(argv[2], &endptr, 10);
        if (*endptr != '\0') {
            shell_error(sh, "Invalid angle value: %s", argv[2]);
            return -EINVAL;
        }

        // Parse speed if provided as 4th argument
        if (argc >= 4) {
            speed = strtoul(argv[3], &endptr, 10);
            if (*endptr != '\0') {
                shell_error(sh, "Invalid speed value: %s", argv[3]);
                return -EINVAL;
            }
        }

        message.payload.polarizer.command =
            orb_mcu_main_Polarizer_Command_POLARIZER_CUSTOM_ANGLE;
        message.payload.polarizer.angle_decidegrees = angle;
        message.payload.polarizer.speed = speed;
        shell_print(
            sh, "Setting polarizer to custom angle %u decidegrees (speed: %u)",
            angle, speed);
    } else {
        shell_error(sh, "Unknown polarizer command: %s", argv[1]);
        return -EINVAL;
    }

    ret_code_t ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to execute polarizer command: %d", ret);
        return -EIO;
    }

    return 0;
}
#endif

static int
execute_ir_leds(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_error(sh, "Usage: ir_leds <wavelength> <duration_us>");
        shell_print(sh, "Wavelengths: see `InfraredLEDs` in "
                        "https://github.com/worldcoin/orb-messages/blob/main/"
                        "messages/main.proto [0-10]");
        shell_print(sh, "Duration: 0-%uµs",
                    IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US);
        return -EINVAL;
    }

    uint32_t wavelength = strtoul(argv[1], NULL, 10);
    uint32_t duration_us = strtoul(argv[2], NULL, 10);

    // Validate wavelength
    if (wavelength >
        orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_940NM_SINGLE) {
        shell_error(sh, "Invalid wavelength. Use 0-10");
        return -EINVAL;
    }

    // Validate duration
    if (duration_us > IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US) {
        shell_error(sh, "Duration: %uµs, must be [0,%u] µs", duration_us,
                    IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US);
        return -EINVAL;
    }

    orb_mcu_main_JetsonToMcu message = {0};

    // First set the LED on duration
    message.which_payload = orb_mcu_main_JetsonToMcu_led_on_time_tag;
    message.payload.led_on_time.on_duration_us = duration_us;

    ret_code_t ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to set LED duration: %d", ret);
        return -EIO;
    }

    // Then enable the LEDs with the specified wavelength
    message.which_payload = orb_mcu_main_JetsonToMcu_infrared_leds_tag;
    message.payload.infrared_leds.wavelength =
        (orb_mcu_main_InfraredLEDs_Wavelength)wavelength;

    ret = runner_handle_new_cli(&message);
    if (ret != RET_SUCCESS) {
        shell_error(sh, "Failed to enable IR LEDs: %d", ret);
        return -EIO;
    }

    shell_print(sh, "IR LEDs set: wavelength=%u, duration=%uus", wavelength,
                duration_us);
    return 0;
}

static int
execute_runner_stats(const struct shell *sh, size_t argc, char **argv)
{
    UNUSED_PARAMETER(argc);
    UNUSED_PARAMETER(argv);

    shell_print(sh, "Successful jobs: %u", runner_successful_jobs_count());
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_orb,
    SHELL_CMD(reboot, NULL, "Reboot system with optional delay",
              execute_reboot),
    SHELL_CMD(version, NULL, "Show firmware and hardware versions",
              execute_version),
    SHELL_CMD(state, NULL, "Show hardware states", execute_state),
    SHELL_CMD(battery, NULL, "Show battery information", execute_battery),
    SHELL_CMD(fan, NULL, "Control fan speed", execute_fan),
    SHELL_CMD(mirror, NULL, "Control mirror angle", execute_mirror),
    SHELL_CMD(user_leds, NULL, "Control user LEDs", execute_user_leds),
    SHELL_CMD(op_leds, NULL, "Control operator LEDs", execute_op_leds),
    SHELL_CMD(fps, NULL, "Set camera FPS", execute_fps),
    SHELL_CMD(liquid_lens, NULL, "Control liquid lens", execute_liquid_lens),
    SHELL_CMD(homing, NULL, "Perform mirror homing", execute_homing),
    SHELL_CMD(heartbeat, NULL, "Send heartbeat", execute_heartbeat),
    SHELL_CMD(voltage, NULL, "Request voltage measurements", execute_voltage),
    SHELL_CMD(power_cycle, NULL, "Power cycle supply lines",
              execute_power_cycle),
    SHELL_CMD(date, NULL, "Set/Get date", execute_date),
    SHELL_CMD(ir_leds, NULL, "Set IR LED duration and enable", execute_ir_leds),
#ifdef CONFIG_BOARD_DIAMOND_MAIN
    SHELL_CMD(white_leds, NULL, "Control white LEDs", execute_white_leds),
    SHELL_CMD(polarizer, NULL, "Control polarizer wheel", execute_polarizer),
#endif
    SHELL_CMD(stats, NULL, "Show runner statistics", execute_runner_stats),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(orb, &sub_orb, "Orb commands", NULL);
