//
// Created by Cyril on 14/07/2021.
//

#include "imu.h"
#include <lsm303.h>
#include <l3g.h>
#include <logging.h>
#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t m_imu_task_handle = NULL;

#define IMU_TASK_NOTIF_FIFO_FULL    0
#define IMU_TASK_NOTIF_DATA_READY   1

/// Notify IMU task that FIFO is full, ready to be flushed
/// 💥 ISR context
static void
fifo_full_handler(void)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;
    vTaskNotifyGiveIndexedFromISR(m_imu_task_handle, IMU_TASK_NOTIF_FIFO_FULL, &switch_to_higher_priority_task);

    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

/// Notify IMU task that data is ready to be parsed
/// 💥 ISR context
static void
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
    uint32_t notifications = 0;
    uint8_t buffer[6*32] = {0};

    lsm303_start(fifo_full_handler);

    while (1)
    {
        // block task while waiting for FIFO to be ready
        notifications = ulTaskNotifyTakeIndexed(IMU_TASK_NOTIF_FIFO_FULL, pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications)
        {
            // Read lsm303 data, non-blocking
            lsm303_read(buffer, sizeof(buffer), data_ready_handler);

            // waiting for data to be flushed so that we can parse the new samples
            notifications = ulTaskNotifyTakeIndexed(IMU_TASK_NOTIF_DATA_READY, pdTRUE, pdMS_TO_TICKS(700));
            {
                if (notifications)
                {
                    LOG_INFO("Parsing IMU data: %i", *(int16_t*) &buffer[180]);
                }
                else
                {
                    LOG_WARNING("Timeout reading IMU data");
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
    lsm303_init();
    // l3g_init();

    return RET_SUCCESS;
}