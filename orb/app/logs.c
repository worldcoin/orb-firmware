#include <board.h>
#include <errors.h>
#include <FreeRTOS.h>
#include <task.h>

static UART_HandleTypeDef m_uart_handle;
static DMA_HandleTypeDef m_dma_uart_tx;
static DMA_HandleTypeDef m_dma_uart_rx;

static uint8_t m_tx_buffer[DEBUG_UART_TX_BUFFER_SIZE] = {0};
static size_t m_wr_index = 0;
static size_t m_rd_index = 0;
static size_t m_chunk_size = 0;

TaskHandle_t m_logs_task = NULL;

/**
  * @brief This function handles DMA1 channel5 global interrupt.
  */
void
DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_rx);
}

/**
  * @brief This function handles DMA1 channel4 global interrupt.
  */
void
DMA1_Channel4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_tx);
}

/**
  * @brief This function handles USART1 global interrupt / USART1 wake-up interrupt through EXTI line 25.
  */
void
USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&m_uart_handle);
}

static void
rx_done_cb(UART_HandleTypeDef *huart)
{

}

/// Callback when UART TX transfer has completed
/// Notify sending task if new bytes are ready to be sent
/// \param huart UART handle
static void
tx_done_cb(UART_HandleTypeDef *huart)
{
    m_rd_index = (m_rd_index + m_chunk_size) % DEBUG_UART_TX_BUFFER_SIZE;
    m_chunk_size = 0;

    if (m_rd_index != m_wr_index)
    {
        xTaskNotifyGive(m_logs_task);
    }
}

/// Task to send debug info over UART when data is ready to be sent
/// \param params
_Noreturn static void
flush_tx(void *params)
{
    const TickType_t xBlockTime = pdMS_TO_TICKS(500);

    while (1)
    {
        // take notifications and clear all if several waiting
        uint32_t notifications = ulTaskNotifyTake(pdTRUE, xBlockTime);

        // transmit buffered data from the circular buffer
        if (notifications)
        {
            // if wraparound
            //  - transmit chunk up to the end of the buffer and come back later
            //    (notification from `tx_done_cb`
            // otherwise transmit full buffer between read and write index
            if (m_wr_index < m_rd_index)
            {
                m_chunk_size = DEBUG_UART_TX_BUFFER_SIZE - m_rd_index;
            }
            else
            {
                m_chunk_size = m_wr_index - m_rd_index;
            }

            HAL_UART_Transmit_DMA(&m_uart_handle, (uint8_t *) &m_tx_buffer[m_rd_index], m_chunk_size);
        }
    }
}

#if defined(__GNUC__)
int
_write(int fd, char *ptr, int len)
{
    // fill tx buffer up to read index
    // -> we don't overwrite data already in the buffer
    int i = 0;
    for (; i < len; ++i)
    {
        m_tx_buffer[m_wr_index] = *(ptr + i);
        m_wr_index = (m_wr_index + 1) % DEBUG_UART_TX_BUFFER_SIZE;

        if (m_wr_index == m_rd_index)
        {
            // consider increasing DEBUG_UART_TX_BUFFER_SIZE
            break;
        }
    }

    // notify sending task if at least one byte have been queued
    if (i && m_uart_handle.gState == HAL_UART_STATE_READY)
    {
        xTaskNotifyGive(m_logs_task);
    }

    return i;
}
#elif defined (__ICCARM__)

#warning "Code has never been tested"

#include "LowLevelIOInterface.h"
size_t __write(int handle, const unsigned char * buffer, size_t size)
{
  HAL_UART_Transmit_DMA(&huart1, (uint8_t *) ptr, size);
  return size;
}
#elif defined (__CC_ARM)
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif

static void
logs_further_init(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef init = {0};

    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PC4     ------> USART1_TX
    PC5     ------> USART1_RX
    */
    init.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOC, &init);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    m_dma_uart_rx.Instance = DMA1_Channel5;
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

    /* USART1_TX Init */
    m_dma_uart_tx.Instance = DMA1_Channel4;
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
logs_init()
{
    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel4_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    /* DMA1_Channel5_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

    /* USART 1 init */
    m_uart_handle.Instance = USART1;
    m_uart_handle.Init.BaudRate = 115200;
    m_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
    m_uart_handle.Init.StopBits = UART_STOPBITS_1;
    m_uart_handle.Init.Parity = UART_PARITY_NONE;
    m_uart_handle.Init.Mode = UART_MODE_TX_RX;
    m_uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    m_uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    m_uart_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    m_uart_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    m_uart_handle.MspInitCallback = logs_further_init;

    HAL_StatusTypeDef err_code = HAL_UART_Init(&m_uart_handle);
    ASSERT(err_code);

    HAL_UART_RegisterCallback(&m_uart_handle, HAL_UART_TX_COMPLETE_CB_ID, tx_done_cb);
    HAL_UART_RegisterCallback(&m_uart_handle, HAL_UART_RX_COMPLETE_CB_ID, rx_done_cb);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    xTaskCreate(flush_tx, "logs", 128, NULL, (tskIDLE_PRIORITY + 1), &m_logs_task);

    return 0;
}