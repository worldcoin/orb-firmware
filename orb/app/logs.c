#include <logs.h>
#include <boards.h>
#include <errors.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <app_config.h>

#define dma_rx_handler  dma_rx(_IRQHandler)
#define dma_tx_handler  dma_tx(_IRQHandler)
#define usart_handler   usart(_IRQHandler)

#define dma_rx_irqn  dma_rx(_IRQn)
#define dma_tx_irqn  dma_tx(_IRQn)
#define usart_irqn   usart(_IRQn)

static UART_HandleTypeDef m_uart_handle;
static DMA_HandleTypeDef m_dma_uart_tx;
static DMA_HandleTypeDef m_dma_uart_rx;

static uint8_t m_tx_buffer[DEBUG_UART_TX_BUFFER_SIZE] = {0};
static uint16_t m_wr_index = 0;
static uint16_t m_rd_index = 0;
static uint16_t m_chunk_size = 0;

TaskHandle_t m_logs_task = NULL;

/**
  * @brief This function handles DMA for UART RX global interrupt
  */
void
dma_rx_handler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_rx);
}

/**
  * @brief This function handles DMA for UART TX global interrupt
  */
void
dma_tx_handler(void)
{
    HAL_DMA_IRQHandler(&m_dma_uart_tx);
}

/**
  * @brief This function handles USART1 global interrupt / USART1 wake-up interrupt through EXTI line 25.
  */
void
usart_handler(void)
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
    m_rd_index = (m_rd_index + m_chunk_size) % DEBUG_UART_TX_BUFFER_SIZE;
    m_chunk_size = 0;

    if (m_rd_index != m_wr_index)
    {
        BaseType_t switch_to_higher_priority_task = pdFALSE;
        vTaskNotifyGiveFromISR(m_logs_task, &switch_to_higher_priority_task);

        // If switch_to_higher_priority_task is now set to pdTRUE then a context switch
        // should be performed to ensure the interrupt returns directly to the highest
        // priority task.  The macro used for this purpose is dependent on the port in
        // use and may be called portEND_SWITCHING_ISR().
        portYIELD_FROM_ISR(switch_to_higher_priority_task);
    }
}

/// Task to send debug info over UART when data is ready to be sent
/// \param params
_Noreturn static void
flush_tx(void *params)
{
    // we could wait indefinitely but that's not possible with the FreeRTOS API
    // so we set the wait time to 1 second
    const TickType_t xBlockTime = pdMS_TO_TICKS(1000);

    while (1)
    {
        // take notifications and clear all if several waiting
        // task will be set to blocked state while waiting for a new notification
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
                m_chunk_size = (uint16_t) (DEBUG_UART_TX_BUFFER_SIZE - m_rd_index);
            }
            else
            {
                m_chunk_size = (uint16_t) (m_wr_index - m_rd_index);
            }

            HAL_UART_Transmit_DMA(&m_uart_handle,
                                  (uint8_t *) &m_tx_buffer[m_rd_index],
                                  m_chunk_size);
        }
    }
}

#if defined(__GNUC__)
int
_write(int fd, char *ptr, int len);

int
_write(int fd, char *ptr, int len)
{
    // fill tx buffer up to read index
    // -> we don't overwrite data already in the buffer
    int i = 0;
    for (; i < len; ++i)
    {
        m_tx_buffer[m_wr_index] = *(ptr + i);
        m_wr_index = (uint16_t) (m_wr_index + (uint16_t) 1) % DEBUG_UART_TX_BUFFER_SIZE;

        if (m_wr_index == m_rd_index)
        {
            // consider increasing DEBUG_UART_TX_BUFFER_SIZE
            break;
        }
    }

    // notify sending task if at least one byte have been queued
    if (i && m_uart_handle.gState == HAL_UART_STATE_READY)
    {
        // check if running from interrupt
        if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
        {
            BaseType_t switch_to_higher_priority_task = pdFALSE;
            vTaskNotifyGiveFromISR(m_logs_task, &switch_to_higher_priority_task);
            portYIELD_FROM_ISR(switch_to_higher_priority_task);
        }
        else
        {
            xTaskNotifyGive(m_logs_task);
        }
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
logs_msp_init(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef init = {0};

    /* Peripheral clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**USART GPIO Configuration
    PC10     ------> USART TX
    PC11     ------> USART RX
    */
    init.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOC, &init);

    /* USART DMA Init */
    /* USART RX Init */
    m_dma_uart_rx.Instance = dma_rx();
#ifdef STM32G4
    m_dma_uart_rx.Init.Request = DMA_REQUEST_USART3_RX;
#endif
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

    /* USART TX Init */
    m_dma_uart_tx.Instance = dma_tx();
#ifdef STM32G4
    m_dma_uart_tx.Init.Request = DMA_REQUEST_USART3_TX;
#endif
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
logs_final_flush(void)
{
    HAL_StatusTypeDef err_code;

    // abort any ongoing task using UART to force print
    HAL_UART_Abort(&m_uart_handle);

    while (m_rd_index != m_wr_index)
    {
        if (m_wr_index < m_rd_index)
        {
            m_chunk_size = (uint16_t) (DEBUG_UART_TX_BUFFER_SIZE - m_rd_index);
        }
        else
        {
            m_chunk_size = (uint16_t) (m_wr_index - m_rd_index);
        }

        err_code = HAL_UART_Transmit(&m_uart_handle,
                                     (uint8_t *) &m_tx_buffer[m_rd_index],
                                     m_chunk_size,
                                     1000);

        if (err_code == HAL_OK)
        {
            m_rd_index = (m_rd_index + m_chunk_size) % DEBUG_UART_TX_BUFFER_SIZE;
        }
    }
}

uint32_t
logs_init(void)
{
    /* DMA controller clock enable */
#ifdef STM32G4
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
#endif
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel4_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(dma_rx_irqn, 5, 0);
    HAL_NVIC_EnableIRQ(dma_rx_irqn);
    /* DMA1_Channel5_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(dma_tx_irqn, 5, 0);
    HAL_NVIC_EnableIRQ(dma_tx_irqn);

    memset(&m_uart_handle, 0, sizeof m_uart_handle);

    /* USART 1 init */
    m_uart_handle.Instance = usart();
    m_uart_handle.Init.BaudRate = 115200;
    m_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
    m_uart_handle.Init.StopBits = UART_STOPBITS_1;
    m_uart_handle.Init.Parity = UART_PARITY_NONE;
    m_uart_handle.Init.Mode = UART_MODE_TX_RX;
    m_uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    m_uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    m_uart_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    // m_uart_handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    m_uart_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    m_uart_handle.MspInitCallback = logs_msp_init;

    HAL_StatusTypeDef err_code = HAL_UART_Init(&m_uart_handle);
    ASSERT(err_code);

#if 0
    err_code = HAL_UARTEx_SetTxFifoThreshold(&m_uart_handle, UART_TXFIFO_THRESHOLD_1_8);
    ASSERT(err_code);

    err_code = HAL_UARTEx_SetRxFifoThreshold(&m_uart_handle, UART_RXFIFO_THRESHOLD_1_8);
    ASSERT(err_code);

    err_code =  HAL_UARTEx_DisableFifoMode(&m_uart_handle);
    ASSERT(err_code);
#endif

    HAL_UART_RegisterCallback(&m_uart_handle, HAL_UART_TX_COMPLETE_CB_ID, tx_done_cb);
    HAL_UART_RegisterCallback(&m_uart_handle, HAL_UART_RX_COMPLETE_CB_ID, rx_done_cb);

    /* USART interrupt Init */
    HAL_NVIC_SetPriority(usart_irqn, 5, 0);
    HAL_NVIC_EnableIRQ(usart_irqn);

    xTaskCreate(flush_tx, "logs", 128, NULL, (tskIDLE_PRIORITY + 1), &m_logs_task);

    return 0;
}