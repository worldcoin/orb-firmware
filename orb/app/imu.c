//
// Created by Cyril on 14/07/2021.
//

#include "imu.h"
#include <lsm303.h>
#include <logging.h>
#include <app_config.h>
#include <mcu_messaging.pb.h>
#include <data_provider.h>
#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t m_imu_task_handle = NULL;

#define IMU_TASK_NOTIF_FIFO_FULL    0
#define IMU_TASK_NOTIF_DATA_READY   1

/// Notify IMU task that FIFO is full, ready to be flushed
/// 💥 ISR context
__attribute__((unused)) static void
fifo_full_handler(void)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(m_imu_task_handle, IMU_TASK_NOTIF_FIFO_FULL, &switch_to_higher_priority_task);

    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

/// Notify IMU task that data is ready to be parsed
/// 💥 ISR context
__attribute__((unused)) static void
data_ready_handler(void)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(m_imu_task_handle, IMU_TASK_NOTIF_DATA_READY, &switch_to_higher_priority_task);

    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

/// This task waits for the following events, in that order:
///   1 - IMU FIFO is full and must be flushed
///   2 - Data is ready to be parsed
/// \param t
_Noreturn static void
imu_task(void *t)
{
    ret_code_t err_code;
    uint32_t notifications = 0;
    uint8_t buffer[6*ACCEL_FIFO_SAMPLES_COUNT] = {0};

#ifdef STM32F3_DISCOVERY
    lsm303_start(fifo_full_handler);
#endif

    while (1)
    {
        // block task while waiting for FIFO to be ready
        notifications = ulTaskNotifyTakeIndexed(IMU_TASK_NOTIF_FIFO_FULL, pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications)
        {
#ifdef STM32F3_DISCOVERY
            // Read lsm303 data, non-blocking
            lsm303_read(buffer, sizeof(buffer), data_ready_handler);
#endif
            // waiting for data to be flushed so that we can parse the new samples
            notifications = ulTaskNotifyTakeIndexed(IMU_TASK_NOTIF_DATA_READY, pdTRUE, pdMS_TO_TICKS(700));
            {
                if (notifications)
                {
                    // parsing buffer and computing average
                    IMUData imu = {0};

                    for (int i = 0; i < ACCEL_FIFO_SAMPLES_COUNT; ++i)
                    {
                        imu.accel_x += *(int16_t*) &buffer[0+6*i];
                        imu.accel_y += *(int16_t*) &buffer[2+6*i];
                        imu.accel_z += *(int16_t*) &buffer[4+6*i];
                    }

                    imu.accel_x /= ACCEL_FIFO_SAMPLES_COUNT;
                    imu.accel_y /= ACCEL_FIFO_SAMPLES_COUNT;
                    imu.accel_z /= ACCEL_FIFO_SAMPLES_COUNT;

                    LOG_INFO("IMU data: (%li, %li, %li)", imu.accel_x, imu.accel_y, imu.accel_z);

                    err_code = data_queue_message_payload(McuToJetson_imu_data_tag, &imu, sizeof(imu));
                    ASSERT(err_code);
                }
                else
                {
                    LOG_ERROR("Timeout reading IMU data");
                }

                notifications = 0;
            }

            notifications = 0;
        }
    }
}

void
imu_start(void)
{
    BaseType_t freertos_err_code = xTaskCreate(imu_task,
                                               "imu",
                                               650,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_imu_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}

ret_code_t
imu_init(void)
{
#ifdef STM32F3_DISCOVERY
    lsm303_init();
#endif
    // l3g_init();

    return RET_SUCCESS;
}