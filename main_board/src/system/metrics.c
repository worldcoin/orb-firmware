#include "memfault/ports/zephyr/thread_metrics.h"
#include <memfault/metrics/metrics.h>

//! Set the list of threads to monitor for stack usage
MEMFAULT_METRICS_DEFINE_THREAD_METRICS(
#if defined(CONFIG_MEMFAULT_METRICS_THREADS_DEFAULTS)
    {
        .thread_name = "idle",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_idle_pct_max),
    },
    {
        .thread_name = "sysworkq",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_sysworkq_pct_max),
    },
#endif
    {
        .thread_name = "main",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_main_pct_max),
    },
    {
        .thread_name = "can_mon",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_can_mon_pct_max),
    },
    {
        .thread_name = "can_rx",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_can_rx_pct_max),
    },
    {
        .thread_name = "can_isotp_rx",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_can_isotp_rx_pct_max),
    },
    {
        .thread_name = "can_tx",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_can_tx_pct_max),
    },
    {
        .thread_name = "can_tx_isotp",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_can_tx_isotp_pct_max),
    },
    {
        .thread_name = "dfu",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_dfu_pct_max),
    },
    {
        .thread_name = "heartbeat",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_heartbeat_pct_max),
    },
    {
        .thread_name = "uart_rx",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_uart_rx_pct_max),
    },
    {
        .thread_name = "watchdog",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_watchdog_pct_max),
    },
    {
        .thread_name = "gnss",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_gnss_pct_max),
    },
    {
        .thread_name = "tof_1d",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_tof_1d_pct_max),
    },
    {
        .thread_name = "liquid_lens",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_liquid_lens_pct_max),
    },
    {
        .thread_name = "mirror_ah_phi_stalldetect",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_mirror_ah_phi_stalldetect_pct_max),
    },
    {
        .thread_name = "mirror_ah_theta_stalldetect",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_mirror_ah_theta_stalldetect_pct_max),
    },
    {
        .thread_name = "motor_ah_phi_one_end",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_motor_ah_phi_one_end_pct_max),
    },
    {
        .thread_name = "motor_ah_theta_one_end",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_motor_ah_theta_one_end_pct_max),
    },
    {
        .thread_name = "battery",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_battery_pct_max),
    },
    {
        .thread_name = "reboot",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_reboot_pct_max),
    },
    {
        .thread_name = "pub_stored",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_pub_stored_pct_max),
    },
    {
        .thread_name = "runner",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_runner_pct_max),
    },
    {
        .thread_name = "fan_tach",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_fan_tach_pct_max),
    },
    {
        .thread_name = "temperature",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_temperature_pct_max),
    },
    {
        .thread_name = "als",
        .stack_usage_metric_key = MEMFAULT_METRICS_KEY(memory_als_pct_max),
    },
    {
        .thread_name = "front_leds",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_front_leds_pct_max),
    },
    {
        .thread_name = "operator_leds",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_operator_leds_pct_max),
    },
    {
        .thread_name = "white_leds",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_white_leds_pct_max),
    },
    {
        .thread_name = "voltage_measurement_adc1",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_voltage_measurement_adc1_pct_max),
    },
    {
        .thread_name = "voltage_measurement_adc4",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_voltage_measurement_adc4_pct_max),
    },
    {
        .thread_name = "voltage_measurement_adc5",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_voltage_measurement_adc5_pct_max),
    },
    {
        .thread_name = "voltage_measurement_publish",
        .stack_usage_metric_key =
            MEMFAULT_METRICS_KEY(memory_voltage_measurement_publish_pct_max),
    });
