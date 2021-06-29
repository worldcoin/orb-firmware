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
#include <stm32f3xx_it.h>

static UART_HandleTypeDef m_uart_handle;
static DMA_HandleTypeDef m_dma_uart_tx;
static DMA_HandleTypeDef m_dma_uart_rx;

TaskHandle_t m_com_tx_task_handle = NULL;

static uint8_t m_tx_buffer[256] = {0};
//static size_t m_wr_idx = {0};
static volatile bool m_new_data_waiting = false;

/**
  * @brief This function handles DMA1 channel4 global interrupt.
  */
void
DMA1_Channel6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_rx);
}

/**
  * @brief This function handles DMA1 channel5 global interrupt.
  */
void
DMA1_Channel7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_tx);
}

/**
  * @brief This function handles USART2 global interrupt / USART2 wake-up interrupt through EXTI line 25.
  */
void
USART2_IRQHandler(void)
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

}

/// Callback when UART TX transfer has completed
/// Notify sending task if new bytes are ready to be sent
/// 💥 ISR context
/// \param huart UART handle
static void
tx_done_cb(UART_HandleTypeDef *huart)
{
    if (serializer_data_waiting())
    {
        BaseType_t switch_to_higher_priority_task = pdFALSE;
        vTaskNotifyGiveFromISR(m_com_tx_task_handle, &switch_to_higher_priority_task);

        // If switch_to_higher_priority_task is now set to pdTRUE then a context switch
        // should be performed to ensure the interrupt returns directly to the highest
        // priority task.  The macro used for this purpose is dependent on the port in
        // use and may be called portEND_SWITCHING_ISR().
        portYIELD_FROM_ISR(switch_to_higher_priority_task);
    }
}

/// The goal of that task is to:
/// - get the awaiting data to be sent
/// - pack the data (protobuf)
/// - send it over the UART link
/// @param t unused at that point
_Noreturn static void
com_tx_task(void *t)
{
    uint32_t notifications = 0;

    while (1)
    {
        // get pending notifications
        // block task while waiting
        notifications = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (notifications || serializer_data_waiting())
        {
            // pack new waiting data
            uint32_t length = serializer_pack_waiting(m_tx_buffer,
                                                      sizeof m_tx_buffer);

            // append CRC16

            if (length != 0)
            {
                LOG_INFO("Sending: l %luB", length);

                HAL_StatusTypeDef err_code = HAL_UART_Transmit_DMA(&m_uart_handle,
                                                                   (uint8_t *) m_tx_buffer,
                                                                   (uint16_t) length);
                if (err_code == HAL_OK)
                {
                    // free serializer waiting list
                }
            }
        }
    }
}

void
com_new_data()
{
    // if UART not being used, emit new notification to TX task
    // otherwise, the notification will be sent from TX callback
    if (m_uart_handle.gState == HAL_UART_STATE_READY)
    {
        xTaskNotifyGive(m_com_tx_task_handle);
    }
}

static void
com_further_init(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef init = {0};

    /* Peripheral clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    init.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &init);

    /* USART2 DMA Init */
    /* USART2_RX Init */
    m_dma_uart_rx.Instance = DMA1_Channel6;
    m_dma_uart_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    m_dma_uart_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    m_dma_uart_rx.Init.MemInc = DMA_MINC_ENABLE;
    m_dma_uart_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    m_dma_uart_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    m_dma_uart_rx.Init.Mode = DMA_NORMAL;
    m_dma_uart_rx.Init.Priority = DMA_PRIORITY_LOW;

    HAL_StatusTypeDef err_code = HAL_DMA_Init(&m_dma_uart_rx);
    ASSERT(err_code);

    __HAL_LINKDMA(huart, hdmarx, m_dma_uart_rx);

    /* USART2_TX Init */
    m_dma_uart_tx.Instance = DMA1_Channel7;
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

uint32_t
com_init(void)
{
    uint32_t err_code;

    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA1_Channel6_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    /* DMA1_Channel7_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

    m_uart_handle.Instance = USART2;
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

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    BaseType_t freertos_err_code = xTaskCreate(com_tx_task,
                                               "com_tx",
                                               150,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_com_tx_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);

    return 0;
}