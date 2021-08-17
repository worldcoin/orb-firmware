#include <logs.h>
#include <logging.h>
#include <com.h>
#include <FreeRTOS.h>
#include <task.h>
#include <data_provider.h>
#include <mcu_messaging.pb.h>
#include <control.h>
#include <deserializer.h>
#include <serializer.h>
#include <watchdog.h>
#include <app_config.h>
#include <imu.h>
#include <diag.h>
#include "board.h"
#include "errors.h"
#include "version.h"

static TaskHandle_t m_test_task_handle = NULL;

void
vApplicationIdleHook(void);

void
vApplicationIdleHook(void)
{
    watchdog_reload();
}

_Noreturn __unused static void
test_task(void *t)
{
    ret_code_t err_code;
    PowerButton button = {.pressed = OnOff_OFF};
    BatteryVoltage bat = {.battery_mvolts = 3700};
    vTaskDelay(500);

    LOG_DEBUG("Setting new data from test_task");

    while (1)
    {
        vTaskDelay(1000);

        err_code =
            data_queue_message_payload(McuToJetson_power_button_tag, &button, sizeof(button));
        ASSERT(err_code);

        button.pressed = (1 - button.pressed);

        vTaskDelay(1000);

        err_code = data_queue_message_payload(McuToJetson_battery_voltage_tag, &bat, sizeof(bat));
        ASSERT(err_code);

        bat.battery_mvolts += 1;
    }

    // task delete itself
    vTaskDelete(NULL);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int
main(void)
{
    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    diag_reset_cause_t reset_cause = reset_cause_get();

    watchdog_init(WATCHDOG_TIMEOUT_MS);

#ifdef DEBUG
    logs_init();

    LOG_INFO("🤖");
    LOG_INFO("Firmware v%u.%u.%u, hw:%u",
             FIRMWARE_VERSION_MAJOR,
             FIRMWARE_VERSION_MINOR,
             FIRMWARE_VERSION_PATCH,
             HARDWARE_REV);
    LOG_INFO("Reset reason: %s", diag_reset_cause_get_name(reset_cause));
#endif

    serializer_init();
    deserializer_init();

#ifdef STM32F3_DISCOVERY
    // com module between MCU and jetson: UART based
    com_init();

    imu_init();
    imu_start();
#endif

    control_init();

    BaseType_t freertos_err_code = xTaskCreate(test_task, "test",
                                               512,
                                               NULL,
                                               (tskIDLE_PRIORITY + 1),
                                               &m_test_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    /* Start scheduler */
    vTaskStartScheduler();

    /* We should never get here as control is now taken by the scheduler */
    HAL_NVIC_SystemReset();
}
