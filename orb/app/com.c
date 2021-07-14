//
// Created by Cyril on 28/06/2021.
//

#include <errors.h>
#include <serializer.h>
#include <logging.h>
#include <mcu_messaging.pb.h>
#include <board.h>
#include "FreeRTOS.h"
#include "task.h"
#include "com.h"
#include "app_config.h"
#include <stm32f3xx_it.h>
#include <deserializer.h>

static UART_HandleTypeDef m_uart_handle;
static DMA_HandleTypeDef m_dma_uart_tx;
static DMA_HandleTypeDef m_dma_uart_rx;

static CRC_HandleTypeDef m_crc_handle = {0};

static TaskHandle_t m_com_tx_task_handle = NULL;
static TaskHandle_t m_com_rx_task_handle = NULL;

static uint8_t m_tx_buffer[COM_TX_BUFFER_SIZE] = {0};
static uint8_t m_rx_buffer[COM_RX_BUFFER_SIZE] = {0};

#define FRAME_PROTOCOL_MAGIC        (0xdead)
#define FRAME_PROTOCOL_MAGIC_SIZE   (2u)
#define FRAME_PROTOCOL_LENGTH_SIZE  (2u)
#define FRAME_PROTOCOL_HEADER_SIZE  (FRAME_PROTOCOL_MAGIC_SIZE + FRAME_PROTOCOL_LENGTH_SIZE)
#define FRAME_PROTOCOL_FOOTER_SIZE  (2u) // crc16

/**
  * @brief This function handles DMA global interrupt.
  */
void
DMA2_Channel3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_rx);
}

/**
  * @brief This function handles DMA global interrupt.
  */
void
DMA2_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_tx);
}

/**
  * @brief This function handles UART4 global interrupt
  */
void
UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&m_uart_handle);
}

///
/// 💥 ISR context
///
/// \param huart
static void
rx_done_cb(UART_HandleTypeDef *huart)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;
    vTaskNotifyGiveFromISR(m_com_rx_task_handle, &switch_to_higher_priority_task);

    // If switch_to_higher_priority_task is now set to pdTRUE then a context switch
    // should be performed to ensure the interrupt returns directly to the highest
    // priority task.  The macro used for this purpose is dependent on the port in
    // use and may be called portEND_SWITCHING_ISR().
    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

_Noreturn static void
com_rx_task(void *t)
{
    uint32_t notifications = 0;
    uint16_t count = 1;
    uint32_t index = 0;
    uint16_t received_crc16 = 0;

    while (1)
    {
        ASSERT_BOOL((index + count) < COM_RX_BUFFER_SIZE);

        // Receiving UART using DMA and wait for RX callback to notify the task about completion
        HAL_UART_Receive_DMA(&m_uart_handle, &m_rx_buffer[index], count);
        notifications = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications)
        {
            switch (index)
            {
                case 0:
                {
                    if (m_rx_buffer[index] == 0xad)
                    {
                        index = 1;
                        count = 1;
                    }
                }
                    break;

                case 1:
                {
                    if (m_rx_buffer[index] == 0xde)
                    {
                        // magic word detected!
                        index = 2;
                        count = 2;
                    }
                    else
                    {
                        // magic word not detected yet, reset index
                        index = 0;
                        count = 1;
                    }
                }
                    break;

                case 2:
                {
                    // let's receive the payload
                    count = *(uint16_t *) &m_rx_buffer[index];
                    index = 4;
                }
                    break;

                case 4:
                {
                    // push index using previously used `count`
                    index += count;

                    // now reading CRC16
                    count = 2;
                }
                    break;

                default:
                {
                    // we just read the CRC16
                    received_crc16 = *(uint16_t *) &m_rx_buffer[index];

                    // fixme add mutex over m_crc_handle usage
                    uint16_t crc16 =
                        (uint16_t) HAL_CRC_Calculate(&m_crc_handle,
                                                     (uint32_t *) &m_rx_buffer[4],
                                                     (index - 4));
                    if (received_crc16 == crc16)
                    {
                        ret_code_t
                            err_code = deserializer_unpack_push(&m_rx_buffer[4], (index - 4));
                        ASSERT(err_code); // consider increasing DESERIALIZER_QUEUE_SIZE
                    }
                    else
                    {
                        LOG_WARNING("Wrong CRC, dismissing received frame");
                    }

                    // we can reset the index for next message to be read
                    index = 0;
                    count = 1;
                }
            }
        }
    }
}

/// Callback when UART TX transfer has completed: UART is in READY state
/// 💥 ISR context
/// \param huart UART handle
static void
tx_done_cb(UART_HandleTypeDef *huart)
{
    BaseType_t switch_to_higher_priority_task = pdFALSE;
    vTaskNotifyGiveFromISR(m_com_tx_task_handle, &switch_to_higher_priority_task);

    // If switch_to_higher_priority_task is now set to pdTRUE then a context switch
    // should be performed to ensure the interrupt returns directly to the highest
    // priority task.  The macro used for this purpose is dependent on the port in
    // use and may be called portEND_SWITCHING_ISR().
    portYIELD_FROM_ISR(switch_to_higher_priority_task);
}

/// The goal of that task is to:
/// - get the awaiting data to be sent
/// - pack the data (protobuf)
/// - encapsulate the data by appending a CRC16
/// - send it over the UART link
/// Task notifications are being used to put the task into blocking state while waiting for UART peripheral to be ready
/// @param t unused at that point
_Noreturn static void
com_tx_task(void *t)
{
    uint32_t notifications = 0;

    // wait for UART peripheral to be ready
    while (m_uart_handle.gState != HAL_UART_STATE_READY)
    {
        portYIELD();
    }

    // init notification, as UART is now ready
    xTaskNotifyGive(m_com_tx_task_handle);

    while (1)
    {
        // block task while waiting for UART to be ready
        notifications = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications)
        {
            // wait for new data and pack it instantly
            // task will be put in blocking state until data is ready to be packed
            uint16_t length =
                (uint16_t) serializer_pack_next_blocking(&m_tx_buffer[FRAME_PROTOCOL_HEADER_SIZE],
                                                         sizeof m_tx_buffer
                                                             - FRAME_PROTOCOL_HEADER_SIZE);

            if (length != 0)
            {
                // prepend magic and frame length
                *(uint16_t *) m_tx_buffer = FRAME_PROTOCOL_MAGIC;
                *(uint16_t *) &m_tx_buffer[FRAME_PROTOCOL_MAGIC_SIZE] = length;

                // append CRC16 (CRC-CCITT (XMODEM))
                uint16_t crc16 =
                    (uint16_t) HAL_CRC_Calculate(&m_crc_handle,
                                                 (uint32_t *) &m_tx_buffer[FRAME_PROTOCOL_HEADER_SIZE],
                                                 length);

                memcpy(&m_tx_buffer[length + FRAME_PROTOCOL_HEADER_SIZE],
                       (uint8_t *) &crc16,
                       sizeof crc16);
                length =
                    (uint16_t) (length + (FRAME_PROTOCOL_HEADER_SIZE + FRAME_PROTOCOL_FOOTER_SIZE));

                LOG_INFO("Sending: l %uB", length);

                HAL_StatusTypeDef err_code = HAL_UART_Transmit_DMA(&m_uart_handle,
                                                                   (uint8_t *) m_tx_buffer,
                                                                   (uint16_t) length);
                if (err_code == HAL_OK)
                {
                    // TODO free serializer waiting list only when UART is transmitting
                }
            }
        }
    }
}

static void
com_further_init(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef init = {0};
    HAL_StatusTypeDef err_code;

    /* Peripheral clock enable */
    __HAL_RCC_UART4_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**UART4 GPIO Configuration
    PC10     ------> UART4_TX
    PC11     ------> UART4_RX
    */
    init.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Alternate = GPIO_AF5_UART4;
    HAL_GPIO_Init(GPIOC, &init);

    /* UART DMA Init */
    m_dma_uart_rx.Instance = DMA2_Channel3;
    m_dma_uart_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    m_dma_uart_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    m_dma_uart_rx.Init.MemInc = DMA_MINC_ENABLE;
    m_dma_uart_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    m_dma_uart_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    m_dma_uart_rx.Init.Mode = DMA_NORMAL;
    m_dma_uart_rx.Init.Priority = DMA_PRIORITY_LOW;

    err_code = HAL_DMA_Init(&m_dma_uart_rx);
    ASSERT(err_code);

    __HAL_LINKDMA(huart, hdmarx, m_dma_uart_rx);

    m_dma_uart_tx.Instance = DMA2_Channel5;
    m_dma_uart_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    m_dma_uart_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    m_dma_uart_tx.Init.MemInc = DMA_MINC_ENABLE;
    m_dma_uart_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    m_dma_uart_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    m_dma_uart_tx.Init.Mode = DMA_NORMAL;
    m_dma_uart_tx.Init.Priority = DMA_PRIORITY_LOW;

    err_code = HAL_DMA_Init(&m_dma_uart_tx);
    ASSERT(err_code);

    __HAL_LINKDMA(huart, hdmatx, m_dma_uart_tx);
}

void
com_init(void)
{
    uint32_t err_code;

    /* DMA controller clock enable */
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA interrupt configuration */
    HAL_NVIC_SetPriority(DMA2_Channel3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Channel3_IRQn);
    HAL_NVIC_SetPriority(DMA2_Channel5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Channel5_IRQn);

    /* UART peripheral init */
    m_uart_handle.Instance = UART4;
    m_uart_handle.Init.BaudRate = 115200;
    m_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
    m_uart_handle.Init.StopBits = UART_STOPBITS_1;
    m_uart_handle.Init.Parity = UART_PARITY_NONE;
    m_uart_handle.Init.Mode = UART_MODE_TX_RX;
    m_uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    m_uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    m_uart_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    m_uart_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    m_uart_handle.MspInitCallback = com_further_init;
    err_code = HAL_UART_Init(&m_uart_handle);
    ASSERT(err_code);

    HAL_UART_RegisterCallback(&m_uart_handle, HAL_UART_TX_COMPLETE_CB_ID, tx_done_cb);
    HAL_UART_RegisterCallback(&m_uart_handle, HAL_UART_RX_COMPLETE_CB_ID, rx_done_cb);

    /* UART interrupt Init */
    HAL_NVIC_SetPriority(UART4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);

    // CRC init
    __HAL_RCC_CRC_CLK_ENABLE();
    m_crc_handle.Instance = CRC;
    m_crc_handle.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
    m_crc_handle.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
    m_crc_handle.Init.GeneratingPolynomial = 0x1021;
    m_crc_handle.Init.CRCLength = CRC_POLYLENGTH_16B;
    m_crc_handle.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
    m_crc_handle.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
    m_crc_handle.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
    err_code = HAL_CRC_Init(&m_crc_handle);
    ASSERT(err_code);

    BaseType_t freertos_err_code = xTaskCreate(com_tx_task,
                                               "com_tx",
                                               160,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_com_tx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    freertos_err_code = xTaskCreate(com_rx_task,
                                    "com_rx",
                                    210,
                                    NULL,
                                    (tskIDLE_PRIORITY + 2),
                                    &m_com_rx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}