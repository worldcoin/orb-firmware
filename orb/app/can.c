//
// Created by Cyril on 02/09/2021.
//

#include "can.h"
#include <board.h>
#include <errors.h>
#include <serializer.h>
#include <logging.h>
#include <compilers.h>
#include <app_config.h>
#include <deserializer.h>
#include "FreeRTOS.h"
#include "task.h"
#include "isotp.h"

static FDCAN_HandleTypeDef m_fdcan_handle = {0};

static TaskHandle_t m_can_tx_task_handle = NULL;
static TaskHandle_t m_can_rx_task_handle = NULL;
static TaskHandle_t m_can_process_task_handle = NULL;

#define PROTOBUF_DATA_MAX_PAYLOAD       128
#define ISOTP_DATA_MAX_PAYLOAD          64

static IsoTpLink m_isotp_data_protobuf_handle; // for data from main MCU to Jetson
static IsoTpLink m_isotp_commands_protobuf_handle; // for data from Jetson to main MCU

static uint8_t m_data_protobuf_buffer[PROTOBUF_DATA_MAX_PAYLOAD] =
    {0};  // for transmitted protobuf data, also used by isotp library (data overwritten)
static uint8_t m_rx_isotp_buffer[ISOTP_DATA_MAX_PAYLOAD] = {0}; // for received ISO-TP control packets

static uint8_t m_data_commands_buffer[PROTOBUF_DATA_MAX_PAYLOAD] =
    {0};  // for received protobuf commands, also used by isotp library (data overwritten)
static uint8_t m_tx_isotp_buffer[ISOTP_DATA_MAX_PAYLOAD] = {0}; // for control packets

/// Print debug message
/// Reimplementation of function called from ISO-TP lib
/// \param message
/// \param ...
void
isotp_user_debug(const char *message, ...)
{
    LOG_DEBUG("%s", message);
}

/// Send CAN frame
/// Reimplementation of function called from ISO-TP lib
/// \param arbitration_id
/// \param data
/// \param size
/// \return
int
isotp_user_send_can(const uint32_t arbitration_id,
                    const uint8_t *data, const uint8_t size)
{
    static uint32_t err_code;
    static uint8_t message_marker = 0;

    FDCAN_TxHeaderTypeDef tx_header = {0};

    // convert size to DLC
    if (size <= 8)
    {
        tx_header.DataLength = (uint32_t) size << 16UL;
    }
    else
    {
        switch (size)
        {
            case 12:
            {
                tx_header.DataLength = FDCAN_DLC_BYTES_12;
            }
                break;
            case 16:
            {
                tx_header.DataLength = FDCAN_DLC_BYTES_16;
            }
                break;
            case 32:
            {
                tx_header.DataLength = FDCAN_DLC_BYTES_32;
            }
                break;
            case 64:
            {
                tx_header.DataLength = FDCAN_DLC_BYTES_64;
            }
                break;

            default:
            {
                LOG_ERROR("Incorrect size: %u", size);
            }
        }
    }

    tx_header.Identifier = arbitration_id & 0x7FF;
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_ON;
    tx_header.FDFormat = FDCAN_FD_CAN;
    tx_header.TxEventFifoControl = FDCAN_STORE_TX_EVENTS;
    tx_header.MessageMarker = message_marker;
    err_code = HAL_FDCAN_AddMessageToTxFifoQ(&m_fdcan_handle, &tx_header, (uint8_t *) data);
    ASSERT(err_code);

    message_marker = (uint8_t) (message_marker + 1) & 0xFF;

    LOG_INFO("Queued CAN frame 0x%02x", message_marker);
//    LOG_DEBUG("Protobuf: 0x%02x 0x%02x 0x%02x 0x%02x",
//              m_data_protobuf_buffer[0],
//              m_data_protobuf_buffer[1],
//              m_data_protobuf_buffer[2],
//              m_data_protobuf_buffer[3]);

    return ISOTP_RET_OK;
}

/// Get millisecond
/// Reimplementation of function called from ISO-TP lib
/// \return
uint32_t
isotp_user_get_ms(void)
{
    return xTaskGetTickCount();
}

void
FDCAN1_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&m_fdcan_handle);
}

/// TX transaction has been performed
/// 💥 ISR context
/// \param hfdcan
/// \param BufferIndexes
static void
tx_done_cb(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;

    // One TX transaction over, unblocking can_process_task to see if tx pending
    vTaskNotifyGiveFromISR(m_can_process_task_handle, &switch_to_higher_priority_task);

    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

_Noreturn static void
data_consumer(void *t)
{
    uint32_t can_tx_ready = 0;

    // init: TX is ready
    xTaskNotifyGive(m_can_tx_task_handle);

    while (1)
    {
        // block task while waiting for CAN to be ready
        can_tx_ready = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (can_tx_ready)
        {
            memset(m_data_protobuf_buffer, 0, sizeof m_data_protobuf_buffer);

            // wait for new data and pack it instantly
            // task will be put in blocking state until data is ready to be packed
            uint16_t length =
                (uint16_t) serializer_pack_next_blocking(&m_data_protobuf_buffer[0],
                                                         sizeof m_data_protobuf_buffer);

            if (length != 0)
            {
                isotp_send(&m_isotp_data_protobuf_handle, m_data_protobuf_buffer, length);
                LOG_INFO("Sending protobuf message, len %u", length);
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
/// \param rx_fifo0_it
static void
rx_done_cb(FDCAN_HandleTypeDef *hfdcan, uint32_t rx_fifo0_it)
{
    UNUSED_PARAMETER(hfdcan);

    if ((rx_fifo0_it & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
    {
        BaseType_t switch_to_higher_priority_task = pdFALSE;
        vTaskNotifyGiveFromISR(m_can_rx_task_handle, &switch_to_higher_priority_task);

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
    FDCAN_RxHeaderTypeDef rx_header = {0};

    while (1)
    {
        notifications = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications)
        {
            memset(m_rx_isotp_buffer, 0, sizeof(m_rx_isotp_buffer));
            memset((uint8_t *) &rx_header, 0, sizeof(rx_header));

            err_code =
                HAL_FDCAN_GetRxMessage(&m_fdcan_handle, FDCAN_RX_FIFO0, &rx_header, m_rx_isotp_buffer);
            if (err_code == HAL_OK)
            {
//                LOG_DEBUG("Received: 0x%02x 0x%02x 0x%02x 0x%02x",
//                          m_rx_isotp_buffer[0],
//                          m_rx_isotp_buffer[1],
//                          m_rx_isotp_buffer[2],
//                          m_rx_isotp_buffer[3]);
            }

            const uint8_t DLC_TO_BYTES[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

            if (rx_header.Identifier == CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES)
            {
                isotp_on_can_message(&m_isotp_commands_protobuf_handle,
                                     m_rx_isotp_buffer,
                                     (uint8_t) DLC_TO_BYTES[rx_header.DataLength >> 16]);
            }
            else if (rx_header.Identifier == CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES)
            {
                isotp_on_can_message(&m_isotp_data_protobuf_handle,
                                     m_rx_isotp_buffer,
                                     (uint8_t) DLC_TO_BYTES[rx_header.DataLength >> 16]);
            }
        }
    }
}

_Noreturn static void
can_process_task(void *t)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;

    // create ISO-TP handles for the different IDs we are handling
    isotp_init_link(&m_isotp_commands_protobuf_handle, CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES,
                    m_tx_isotp_buffer, sizeof(m_tx_isotp_buffer),
                    m_data_commands_buffer, sizeof(m_data_commands_buffer));

    isotp_init_link(&m_isotp_data_protobuf_handle, CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES,
                    m_data_protobuf_buffer, sizeof(m_data_protobuf_buffer),
                    m_rx_isotp_buffer, sizeof(m_rx_isotp_buffer));

    while (1)
    {
        // wait for TX done or 100ms
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

        isotp_poll(&m_isotp_data_protobuf_handle);
        isotp_poll(&m_isotp_commands_protobuf_handle);

        // ready to send more?
        if (m_isotp_data_protobuf_handle.send_status == ISOTP_SEND_STATUS_IDLE)
        {
            vTaskNotifyGiveFromISR(m_can_tx_task_handle, &switch_to_higher_priority_task);
        }
        else if (m_isotp_data_protobuf_handle.send_status == ISOTP_SEND_STATUS_ERROR)
        {
            LOG_ERROR("Send error, resetting ISO-TP handle (data)");
            isotp_init_link(&m_isotp_data_protobuf_handle, CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES,
                            m_data_protobuf_buffer, sizeof(m_data_protobuf_buffer),
                            m_rx_isotp_buffer, sizeof(m_rx_isotp_buffer));
        }

        // Get full payload if status FULL
        if (m_isotp_data_protobuf_handle.receive_status == ISOTP_RECEIVE_STATUS_FULL)
        {
            uint16_t out_size = 0;
            isotp_receive(&m_isotp_data_protobuf_handle, m_data_protobuf_buffer, sizeof(m_data_protobuf_buffer), &out_size);

            ret_code_t
                err_code = deserializer_unpack_push(m_data_protobuf_buffer, out_size);
            if (err_code == RET_SUCCESS)
            {
                LOG_INFO("Unpacked %uB", out_size);
            }
            else
            {
                LOG_ERROR("Error unpacking data");
            }
        }

        if (m_isotp_commands_protobuf_handle.receive_status == ISOTP_RECEIVE_STATUS_FULL)
        {
            uint16_t out_size = 0;
            isotp_receive(&m_isotp_data_protobuf_handle, m_data_commands_buffer, sizeof(m_data_commands_buffer), &out_size);
            LOG_INFO("Received command: %uB", out_size);

            ret_code_t
                err_code = deserializer_unpack_push(m_data_commands_buffer, out_size);
            if (err_code == RET_SUCCESS)
            {
                LOG_INFO("Unpacked %uB", out_size);
            }
            else
            {
                LOG_ERROR("Error unpacking data");
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
        FDCAN_MODE_EXTERNAL_LOOPBACK; // FIXME: TX connected to both RX and external device
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

    // configure FDCAN filter for RX FIFO0
    // Arbitration phase: priority given to lower number identifiers over higher number identifiers.
    // Filter config:
    //   - Store in Rx FIFO0 if message has standard ID that equals `FilterID1` or `FilterID2`
    FDCAN_FilterTypeDef filter_fifo0_config = {0};
    filter_fifo0_config.IdType = FDCAN_STANDARD_ID;
    filter_fifo0_config.FilterIndex = 0;
    filter_fifo0_config.FilterType = FDCAN_FILTER_DUAL;
    filter_fifo0_config.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

    filter_fifo0_config.FilterID1 = CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES;
    filter_fifo0_config.FilterID2 = CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES;

    err_code = HAL_FDCAN_ConfigFilter(&m_fdcan_handle, &filter_fifo0_config);
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

    // register interrupt callbacks
    err_code = HAL_FDCAN_RegisterTxBufferCompleteCallback(&m_fdcan_handle, tx_done_cb);
    ASSERT(err_code);

    err_code = HAL_FDCAN_RegisterRxFifo0Callback(&m_fdcan_handle, rx_done_cb);
    ASSERT(err_code);

    // enable interrupt line
    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    // enable interrupts
    err_code = HAL_FDCAN_ActivateNotification(&m_fdcan_handle,
                                              FDCAN_IT_TX_COMPLETE,
                                              FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1
                                                  | FDCAN_TX_BUFFER2);
    ASSERT(err_code);

    err_code = HAL_FDCAN_ActivateNotification(&m_fdcan_handle,
                                              FDCAN_IT_RX_FIFO0_NEW_MESSAGE
                                                  | FDCAN_IT_RX_FIFO0_MESSAGE_LOST,
                                              0);
    ASSERT(err_code);

    // start FDCAN controller, listening CAN bus
    err_code = HAL_FDCAN_Start(&m_fdcan_handle);
    ASSERT(err_code);

    // create TX & RX tasks
    BaseType_t freertos_err_code = xTaskCreate(data_consumer,
                                               "can_tx",
                                               256,
                                               NULL,
                                               (tskIDLE_PRIORITY + 3),
                                               &m_can_tx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    freertos_err_code = xTaskCreate(can_rx_task,
                                    "can_rx",
                                    256,
                                    NULL,
                                    (tskIDLE_PRIORITY + 3),
                                    &m_can_rx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    freertos_err_code = xTaskCreate(can_process_task,
                                    "can_process",
                                    256,
                                    NULL,
                                    (tskIDLE_PRIORITY + 3),
                                    &m_can_process_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}