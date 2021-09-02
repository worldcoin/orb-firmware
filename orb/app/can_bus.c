//
// Created by Cyril on 02/09/2021.
//

#include "can_bus.h"
#include <board.h>
#include <errors.h>
#include <serializer.h>
#include <logging.h>
#include "FreeRTOS.h"
#include "task.h"

static FDCAN_HandleTypeDef m_fdcan_handle;
static FDCAN_FilterTypeDef m_filter_fifo0_config;

static TaskHandle_t m_com_tx_task_handle = NULL;
static TaskHandle_t m_com_rx_task_handle = NULL;

static uint8_t m_tx_buffer[64] = {0};
static uint8_t m_rx_buffer[64] = {0};

void
FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&m_fdcan_handle);
}

/// 💥 ISR context
/// \param hfdcan
/// \param BufferIndexes
static void
tx_done_cb(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;
    vTaskNotifyGiveFromISR(m_com_tx_task_handle, &switch_to_higher_priority_task);

    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

_Noreturn static void
can_tx_task(void *t)
{
    uint32_t err_code;
    uint32_t can_tx_ready = 0;
    uint8_t message_marker = 0;

    // init notification as ready
    xTaskNotifyGive(m_com_tx_task_handle);

    while (1)
    {
        // block task while waiting for CAN to be ready
        can_tx_ready = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (can_tx_ready)
        {
            memset(m_tx_buffer, 0, sizeof m_tx_buffer);

            // wait for new data and pack it instantly
            // task will be put in blocking state until data is ready to be packed
            uint16_t length =
                (uint16_t) serializer_pack_next_blocking(&m_tx_buffer[0],
                                                         sizeof m_tx_buffer);

            // length = 0 means error with encoding
            // length > 64 is not possible
            if (length != 0 && length <= 64)
            {
                FDCAN_TxHeaderTypeDef tx_header = {0};

                tx_header.Identifier = 0x111;
                tx_header.IdType = FDCAN_STANDARD_ID;
                tx_header.TxFrameType = FDCAN_DATA_FRAME;
                tx_header.DataLength = FDCAN_DLC_BYTES_64;
                tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
                tx_header.BitRateSwitch = FDCAN_BRS_ON;
                tx_header.FDFormat = FDCAN_FD_CAN;
                tx_header.TxEventFifoControl = FDCAN_STORE_TX_EVENTS;
                tx_header.MessageMarker = message_marker;
                err_code = HAL_FDCAN_AddMessageToTxFifoQ(&m_fdcan_handle, &tx_header, m_tx_buffer);
                ASSERT(err_code);

                message_marker = (uint8_t) (message_marker + 1) & 0xFF;

                LOG_INFO("Queued CAN frame 0x%02x", message_marker);
                LOG_DEBUG("Sent: 0x%02x 0x%02x 0x%02x 0x%02x", m_tx_buffer[0], m_tx_buffer[1], m_tx_buffer[2], m_tx_buffer[3]);
            }
            else
            {
                LOG_ERROR("Error with encoded frame, length: %u", length);
            }
        }
    }
}

/// 💥 ISR context
/// \param hfdcan
/// \param RxFifo0ITs
static void
rx_done_cb(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
    {
        BaseType_t switch_to_higher_priority_task = pdFALSE;
        vTaskNotifyGiveFromISR(m_com_rx_task_handle, &switch_to_higher_priority_task);

        // If switch_to_higher_priority_task is now set to pdTRUE then a context switch
        // should be performed to ensure the interrupt returns directly to the highest
        // priority task.  The macro used for this purpose is dependent on the port in
        // use and may be called portEND_SWITCHING_ISR().
        portYIELD_FROM_ISR(switch_to_higher_priority_task);
    }
}

_Noreturn static void
can_rx_task(void *t)
{
    uint32_t notifications = 0;
    uint32_t err_code;
    FDCAN_RxHeaderTypeDef RxHeader;

    while (1)
    {
        notifications = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications)
        {
            memset(m_rx_buffer, 0, sizeof(m_rx_buffer));

            err_code = HAL_FDCAN_GetRxMessage(&m_fdcan_handle, FDCAN_RX_FIFO0, &RxHeader, m_rx_buffer);
            if (err_code == HAL_OK)
            {

                LOG_DEBUG("Received: 0x%02x 0x%02x 0x%02x 0x%02x", m_rx_buffer[0], m_rx_buffer[1], m_rx_buffer[2], m_rx_buffer[3]);
            }
        }
    }
}

static void
fdcan_msp_init(FDCAN_HandleTypeDef *hfdcan)
{
    GPIO_InitTypeDef init = {0};

    /* Peripheral clock enable */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**FDCAN1 GPIO Configuration
    PB8-BOOT0     ------> FDCAN1_RX
    PB9     ------> FDCAN1_TX
    */
    init.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    init.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOB, &init);
}

void
can_init(void)
{
    uint32_t err_code;

    m_fdcan_handle.Instance = FDCAN1;
    m_fdcan_handle.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    m_fdcan_handle.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
    m_fdcan_handle.Init.Mode =
        FDCAN_MODE_EXTERNAL_LOOPBACK; // TX connected to both RX and external device
    m_fdcan_handle.Init.AutoRetransmission = ENABLE;
    m_fdcan_handle.Init.TransmitPause = ENABLE;
    m_fdcan_handle.Init.ProtocolException = DISABLE;
    m_fdcan_handle.Init.NominalPrescaler = 1;
    m_fdcan_handle.Init.NominalSyncJumpWidth = 16;
    m_fdcan_handle.Init.NominalTimeSeg1 = 63;
    m_fdcan_handle.Init.NominalTimeSeg2 = 16;
    m_fdcan_handle.Init.DataPrescaler = 1;
    m_fdcan_handle.Init.DataSyncJumpWidth = 4;
    m_fdcan_handle.Init.DataTimeSeg1 = 5;
    m_fdcan_handle.Init.DataTimeSeg2 = 4;
    m_fdcan_handle.Init.StdFiltersNbr = 1;
    m_fdcan_handle.Init.ExtFiltersNbr = 1;
    m_fdcan_handle.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    m_fdcan_handle.MspInitCallback = fdcan_msp_init;

    err_code = HAL_FDCAN_Init(&m_fdcan_handle);
    ASSERT(err_code);

    m_fdcan_handle.TxBufferCompleteCallback = tx_done_cb;
    m_fdcan_handle.RxFifo0Callback = rx_done_cb;

    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    // configure FDCAN filter for RX FIFO0
    // Filter config:
    //   - Store in Rx FIFO0 if message has standard ID that equals `FilterID1` or `FilterID2`
    m_filter_fifo0_config.IdType = FDCAN_STANDARD_ID;
    m_filter_fifo0_config.FilterIndex = 0;
    m_filter_fifo0_config.FilterType = FDCAN_FILTER_DUAL;
    m_filter_fifo0_config.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    m_filter_fifo0_config.FilterID1 = 0x111;
    m_filter_fifo0_config.FilterID2 = 0x222;

    err_code = HAL_FDCAN_ConfigFilter(&m_fdcan_handle, &m_filter_fifo0_config);
    ASSERT(err_code);

    // all non-matching frames are rejected
    // RX FIFO1 not used
    // Frames with Extended ID are all rejected
    err_code = HAL_FDCAN_ConfigGlobalFilter(&m_fdcan_handle,
                                            FDCAN_REJECT,
                                            FDCAN_REJECT,
                                            FDCAN_FILTER_REMOTE,
                                            FDCAN_REJECT_REMOTE);
    ASSERT(err_code);

    // enable interrupt HAL_FDCAN_TxBufferCompleteCallback
    HAL_FDCAN_ActivateNotification(&m_fdcan_handle,
                                   FDCAN_IT_TX_COMPLETE,
                                   FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1 | FDCAN_TX_BUFFER2);

    // enable interrupt HAL_FDCAN_RxFifo0Callback
    HAL_FDCAN_ActivateNotification(&m_fdcan_handle,
                                   FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_MESSAGE_LOST,
                                   0);

    // start FDCAN controller, listening CAN bus
    err_code = HAL_FDCAN_Start(&m_fdcan_handle);
    ASSERT(err_code);

    // create TX & RX tasks
    BaseType_t freertos_err_code = xTaskCreate(can_tx_task,
                                               "can_tx",
                                               256,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_com_tx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    freertos_err_code = xTaskCreate(can_rx_task,
                                    "can_rx",
                                    256,
                                    NULL,
                                    (tskIDLE_PRIORITY + 2),
                                    &m_com_rx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}