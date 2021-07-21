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
#include "board.h"
#include "errors.h"
#include "version.h"

static TaskHandle_t m_test_task_handle = NULL;

void
vApplicationIdleHook(void);

/**
  * @brief System Clock Configuration
  * @retval None
  */
static void
SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;

    uint32_t err_code = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    ASSERT(err_code);

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    err_code = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
    ASSERT(err_code);

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_UART4
        | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_TIM1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    PeriphClkInit.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
    PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
    PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;

    err_code = HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
    ASSERT(err_code);
}

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

        err_code = data_queue_message_payload(McuToJetson_power_button_tag, &button, sizeof(button));
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

    watchdog_init(WATCHDOG_TIMEOUT_MS);

#ifdef DEBUG
    logs_init();
#endif

    LOG_INFO("🤖");
    LOG_INFO("Firmware v%u.%u.%u, hw:%u",
             FIRMWARE_VERSION_MAJOR,
             FIRMWARE_VERSION_MINOR,
             FIRMWARE_VERSION_PATCH,
             HARDWARE_REV);

    serializer_init();
    deserializer_init();
    com_init();
    control_init();
    imu_init();
    imu_start();

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
