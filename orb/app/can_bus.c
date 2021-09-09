//
// Created by Cyril on 02/09/2021.
//

#include "can_bus.h"
#include <FreeRTOS.h>
#include <boards.h>
#include <errors.h>
#include <serializer.h>
#include <logging.h>
#include <compilers.h>
#include <queue.h>
#include <app_config.h>
#include "task.h"
#include "isotp.h"

/// Tasks
static TaskHandle_t m_can_rx_task_handle = NULL;
static TaskHandle_t m_can_process_task_handle = NULL;

/// RX task has higher priority over processing task
/// because we want to have RX packets handled within the 100ms timeframe
/// see \c ISO_TP_DEFAULT_RESPONSE_TIMEOUT
#define TASK_PRIORITY_CAN_RX        (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_CAN_PROCESS   (tskIDLE_PRIORITY + 2)

#define ISOTP_RX_TX_BUFFER_SIZE     PROTOBUF_DATA_MAX_SIZE

const uint8_t DLC_TO_BYTES[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
static FDCAN_HandleTypeDef m_fdcan_handle = {0};

static QueueHandle_t m_rx_queue_handle = 0;
#define RX_QUEUE_SIZE   8
typedef struct
{
    can_id_t e_id;
    uint8_t length;
    uint8_t rx_buf[64];
} rx_message_t;

typedef struct
{
    uint8_t tx_buffer[ISOTP_RX_TX_BUFFER_SIZE];
    uint8_t rx_buffer[ISOTP_RX_TX_BUFFER_SIZE];
    IsoTpLink isotp_handle;
    void
    (*tx_complete_cb)(ret_code_t ret);
    void
    (*rx_complete_cb)(uint8_t *data, size_t len);
    bool tx_busy;
    bool is_init;
} can_isotp_obj_t;

static can_isotp_obj_t m_isotp[2] = {0};

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

/// 💥 ISR context
/// \param hfdcan
/// \param rx_fifo0_it
static void
rx_done_cb(FDCAN_HandleTypeDef *hfdcan, uint32_t rx_fifo0_it)
{
    UNUSED_PARAMETER(hfdcan);
    FDCAN_RxHeaderTypeDef rx_header = {0};
    rx_message_t msg = {0};

    if ((rx_fifo0_it & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
    {

        uint32_t err_code =
            HAL_FDCAN_GetRxMessage(&m_fdcan_handle,
                                   FDCAN_RX_FIFO0,
                                   &rx_header,
                                   msg.rx_buf);
        ASSERT(err_code);

        // check identifier
        if (rx_header.Identifier < CAN_ID_BASE
            || rx_header.Identifier >= (CAN_ID_BASE + CAN_ID_COUNT))
        {
            // received message with unknown identifier, discarding
            return;
        }

        // get identifier as enum
        msg.e_id = (rx_header.Identifier - CAN_ID_BASE);

        if (m_isotp[msg.e_id].is_init)
        {
            msg.length = DLC_TO_BYTES[rx_header.DataLength >> 16];

            BaseType_t switch_to_higher_priority_task = pdFALSE;
            xQueueSendToBackFromISR(m_rx_queue_handle, &msg, &switch_to_higher_priority_task);

            portYIELD_FROM_ISR(switch_to_higher_priority_task);
        }
    }
}

_Noreturn static void
can_rx_task(void *t)
{
    UNUSED_PARAMETER(t);
    rx_message_t msg;

    while (1)
    {
        if (xQueueReceive(m_rx_queue_handle, &msg, portMAX_DELAY))
        {
            // process rx message
            isotp_on_can_message(&m_isotp[msg.e_id].isotp_handle, msg.rx_buf, msg.length);

            // check if full message received
            if (m_isotp[msg.e_id].isotp_handle.receive_status == ISOTP_RECEIVE_STATUS_FULL)
            {
                uint16_t out_size = 0;

                // it is safe to use the same rx_buffer as used in iso_tp
                // because rx_buffer won't be written into as the only place is it modified is from
                // that task (calling isotp_on_can_message)
                int ret = isotp_receive(&m_isotp[msg.e_id].isotp_handle,
                                        m_isotp[msg.e_id].rx_buffer,
                                        sizeof(m_isotp[msg.e_id].rx_buffer),
                                        &out_size);

                // warn user that rx complete
                if (ret == ISOTP_RET_OK && m_isotp[msg.e_id].rx_complete_cb != NULL)
                {
                    m_isotp[msg.e_id].rx_complete_cb(m_isotp[msg.e_id].rx_buffer, out_size);
                }
            }
        }
    }
}

/// TX transaction has been performed
/// 💥 ISR context
/// \param hfdcan
/// \param BufferIndexes
static void
tx_done_cb(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes)
{
    UNUSED_PARAMETER(hfdcan);
    UNUSED_PARAMETER(BufferIndexes);

    BaseType_t switch_to_higher_priority_task = pdFALSE;

    // One TX transaction over, unblocking can_process_task to see if tx pending
    vTaskNotifyGiveFromISR(m_can_process_task_handle, &switch_to_higher_priority_task);

    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

_Noreturn static void
can_process_task(void *t)
{
    UNUSED_PARAMETER(t);

    int32_t delay = ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

    xTaskNotifyGive(m_can_process_task_handle);
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay));

        delay = ISO_TP_DEFAULT_RESPONSE_TIMEOUT;

        // process each isotp instance
        for (int i = 0; i < sizeof(m_isotp) / sizeof(can_isotp_obj_t); ++i)
        {
            isotp_poll(&m_isotp[i].isotp_handle);

            // if we send Consecutive Frame within a Block Size
            // we want to call isotp_poll as soon as Separation Time expires
            // let's compute the minimum delay we want
            if (m_isotp[i].isotp_handle.send_status
                == ISOTP_SEND_STATUS_INPROGRESS
                && m_isotp[i].isotp_handle.send_bs_remain)
            {
                int min_delay_ms =
                    (int) (m_isotp[CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES].isotp_handle.send_timer_st
                        - isotp_user_get_ms());

                if (min_delay_ms > 0 && min_delay_ms < delay)
                {
                    delay = min_delay_ms;
                }
            }
            else if (m_isotp[i].isotp_handle.send_status == ISOTP_SEND_STATUS_IDLE
                && m_isotp[i].tx_busy)
            {
                if (m_isotp[i].tx_complete_cb != NULL)
                {
                    m_isotp[i].tx_complete_cb(RET_SUCCESS);
                }
                m_isotp[i].tx_busy = false;
            }
            else if (m_isotp[i].isotp_handle.send_status == ISOTP_SEND_STATUS_ERROR)
            {
                LOG_ERROR("Error sending (ID 0x%03x), aborting", CAN_ID_BASE + i);

                // warn user
                if (m_isotp[i].tx_complete_cb)
                {
                    m_isotp[i].tx_complete_cb(RET_ERROR_INTERNAL);
                }

                // reset structure
                memset(&m_isotp[i], 0, sizeof(can_isotp_obj_t));
            }
        }
    }
}

void
can_bind(can_id_t e_id,
         void (*tx_complete_cb)(ret_code_t),
         void (*rx_complete_cb)(uint8_t *, size_t))
{
    memset(&m_isotp[e_id], 0, sizeof(can_isotp_obj_t));

    isotp_init_link(&m_isotp[e_id].isotp_handle, (e_id + CAN_ID_BASE),
                    m_isotp[e_id].tx_buffer, sizeof(m_isotp[e_id].tx_buffer),
                    m_isotp[e_id].rx_buffer, sizeof(m_isotp[e_id].rx_buffer));

    m_isotp[e_id].tx_complete_cb = tx_complete_cb;
    m_isotp[e_id].rx_complete_cb = rx_complete_cb;
    m_isotp[e_id].is_init = true;
}

ret_code_t
can_send(can_id_t e_id, uint8_t *data, uint16_t len)
{
    int ret;

    if (!m_isotp[e_id].is_init)
    {
        // not bound, call can_bind
        return RET_ERROR_INVALID_STATE;
    }
    else if (m_isotp[e_id].tx_busy)
    {
        // cannot send at the moment, busy
        return RET_ERROR_BUSY;
    }

    ret = isotp_send_with_id(&m_isotp[e_id].isotp_handle, (e_id + CAN_ID_BASE), data, len);
    if (ret != ISOTP_RET_OK)
    {
        // TODO parse error message
    }
    else
    {
        m_isotp[e_id].tx_busy = true;
    }

    return RET_SUCCESS;
}

static void
fdcan_msp_init(FDCAN_HandleTypeDef *hfdcan)
{
    UNUSED_PARAMETER(hfdcan);

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

    m_rx_queue_handle = xQueueCreate(RX_QUEUE_SIZE, sizeof(rx_message_t));

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
    //   - Store in Rx FIFO0 if message has standard ID that ranges from `FilterID1` to `FilterID2`
    FDCAN_FilterTypeDef filter_fifo0_config = {0};
    filter_fifo0_config.IdType = FDCAN_STANDARD_ID;
    filter_fifo0_config.FilterIndex = 0;
    filter_fifo0_config.FilterType = FDCAN_FILTER_RANGE;
    filter_fifo0_config.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

    filter_fifo0_config.FilterID1 = CAN_ID_BASE;
    filter_fifo0_config.FilterID2 = CAN_ID_BASE + CAN_ID_COUNT;

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

    // listening ID CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES
    isotp_init_link(&m_isotp[CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES].isotp_handle,
                    (CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES + CAN_ID_BASE),
                    m_isotp[CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES].tx_buffer,
                    sizeof(m_isotp[CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES].tx_buffer),
                    m_isotp[CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES].rx_buffer,
                    sizeof(m_isotp[CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES].rx_buffer));

    BaseType_t freertos_err_code = xTaskCreate(can_rx_task,
                                               "can_rx",
                                               256,
                                               NULL,
                                               TASK_PRIORITY_CAN_RX,
                                               &m_can_rx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    freertos_err_code = xTaskCreate(can_process_task,
                                    "can_process",
                                    256,
                                    NULL,
                                    TASK_PRIORITY_CAN_PROCESS,
                                    &m_can_process_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}