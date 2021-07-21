//
// Created by Cyril on 14/07/2021.
//

#include "lsm303.h"
#include <board.h>
#include <stm32f3xx_it.h>
#include <errors.h>
#include <logging.h>
#include <string.h>

static I2C_HandleTypeDef m_i2c_handle;
static DMA_HandleTypeDef m_dma_i2c1_rx;
static DMA_HandleTypeDef m_dma_i2c1_tx;

static void
(*m_fifo_full_cb)(void) = NULL;

static void
(*m_data_ready_cb)(void) = NULL;

/// Data ready interrupt
void
EXTI4_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != 0x00u)
    {
        if (m_fifo_full_cb != NULL)
        {
            m_fifo_full_cb();
        }

        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
    }
}

void
DMA1_Channel6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_i2c1_tx);
}

void
DMA1_Channel7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&m_dma_i2c1_rx);
}

void
I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&m_i2c_handle);
}

void
I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&m_i2c_handle);
}

static void
interrupt_init(void)
{
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    // IMU interrupt pin
    init.Pin = LSM303_INT1_PIN;
    init.Mode = GPIO_MODE_IT_RISING;
    init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOE, &init);

    // Enable interrupt EXTI
    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

static void
i2c_further_init(struct __I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef init;
    HAL_StatusTypeDef ret;

    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    init.Pin = GPIO_PIN_6;
    init.Mode = GPIO_MODE_AF_OD;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &init);

    init.Pin = GPIO_PIN_7;
    init.Mode = GPIO_MODE_AF_OD;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &init);


    /* Peripheral DMA init */
    __HAL_RCC_DMA1_CLK_ENABLE();

    m_dma_i2c1_rx.Instance = DMA1_Channel7;
    m_dma_i2c1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    m_dma_i2c1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    m_dma_i2c1_rx.Init.MemInc = DMA_MINC_ENABLE;
    m_dma_i2c1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    m_dma_i2c1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    m_dma_i2c1_rx.Init.Mode = DMA_NORMAL;
    m_dma_i2c1_rx.Init.Priority = DMA_PRIORITY_LOW;

    ret = HAL_DMA_Init(&m_dma_i2c1_rx);
    ASSERT(ret);

    __HAL_LINKDMA(&m_i2c_handle, hdmarx, m_dma_i2c1_rx);

    m_dma_i2c1_tx.Instance = DMA1_Channel6;
    m_dma_i2c1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    m_dma_i2c1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    m_dma_i2c1_tx.Init.MemInc = DMA_MINC_ENABLE;
    m_dma_i2c1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    m_dma_i2c1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    m_dma_i2c1_tx.Init.Mode = DMA_NORMAL;
    m_dma_i2c1_tx.Init.Priority = DMA_PRIORITY_LOW;

    ret = HAL_DMA_Init(&m_dma_i2c1_tx);
    ASSERT(ret);

    __HAL_LINKDMA(&m_i2c_handle, hdmatx, m_dma_i2c1_tx);
}

static void
i2c_rx_done_cb(struct __I2C_HandleTypeDef *hi2c)
{
    if (m_data_ready_cb != NULL)
    {
        m_data_ready_cb();
        m_data_ready_cb = NULL;
    }
}

void
lsm303_read(uint8_t *buffer, size_t length, void (*data_ready_cb)(void))
{
    HAL_StatusTypeDef err_code;
    uint8_t data[6] = {0x00};

    // clear INT1 pin by reading INT1_SRC
    err_code = HAL_I2C_Mem_Read(&m_i2c_handle,
                                ACC_I2C_ADDRESS | 0x1,
                                LSM303DLHC_INT1_SOURCE_A,
                                1,
                                data,
                                1,
                                10);
    ASSERT(err_code);

    // get sample count
    err_code = HAL_I2C_Mem_Read(&m_i2c_handle,
                                ACC_I2C_ADDRESS | 0x1,
                                LSM303DLHC_FIFO_SRC_REG_A,
                                1,
                                data,
                                1,
                                10);
    ASSERT(err_code);

    uint8_t size = data[0] & 0x1F;
    LOG_INFO("%u samples ready", size);

    ASSERT_BOOL(length >= size * 6);

    memset(buffer, 0, length);
    err_code = HAL_I2C_Mem_Read_DMA(&m_i2c_handle,
                                    ACC_I2C_ADDRESS | 0x1,
                                    LSM303DLHC_OUT_X_L_A | 0x80,
                                    1,
                                    buffer,
                                    size * 6);
    ASSERT(err_code);

    // now waiting for interrupt callback to be called on completion (i2c_rx_done_cb)
    // use callback to warn function user about completion
    m_data_ready_cb = data_ready_cb;
}

void
lsm303_start(void (*fifo_full_cb)(void))
{
    HAL_StatusTypeDef ret;

    uint8_t data[6] = {0x00};

    interrupt_init();

    // set acquisition configuration
    data[0] = LSM303DLHC_CTRL_REG1_A;
    data[1] = LSM303DLHC_NORMAL_MODE | LSM303DLHC_ODR_10_HZ | LSM303DLHC_AXES_ENABLE;
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);

    data[0] = LSM303DLHC_CTRL_REG4_A;
    data[1] = LSM303DLHC_BlockUpdate_Continuous | LSM303DLHC_BLE_LSB | LSM303DLHC_FULLSCALE_2G
        | LSM303DLHC_HR_ENABLE;
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);

    /// setup FIFO
    // reset FIFO
    data[0] = LSM303DLHC_FIFO_CTRL_REG_A;
    data[1] = 0;
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);

    // set FIFO mode, trigger INT1, size of 16 samples
    data[0] = LSM303DLHC_FIFO_CTRL_REG_A;
    data[1] = 0xC0 | 0x20 | (16 - 1);
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);

    // interrupt on FIFO watermark
    data[0] = LSM303DLHC_CTRL_REG3_A;
    data[1] = LSM303DLHC_IT1_WTM;
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);

    // enable FIFO TODO: read LSM303DLHC_CTRL_REG5_A before in order to prevent overwriting values
    data[0] = LSM303DLHC_CTRL_REG5_A;
    data[1] = 0x40;
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);

    m_fifo_full_cb = fifo_full_cb;
}

static void
init_i2c(void)
{
    HAL_StatusTypeDef err_code;

    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

    /* I2C peripheral init */
    m_i2c_handle.Instance = I2C1;
    m_i2c_handle.Init.Timing = 0x2000090E;
    m_i2c_handle.Init.OwnAddress1 = 0;
    m_i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    m_i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    m_i2c_handle.Init.OwnAddress2 = 0;
    m_i2c_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    m_i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    m_i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    m_i2c_handle.MspInitCallback = i2c_further_init;
    err_code = HAL_I2C_Init(&m_i2c_handle);
    ASSERT(err_code);

    err_code = HAL_I2CEx_ConfigAnalogFilter(&m_i2c_handle, I2C_ANALOGFILTER_ENABLE);
    ASSERT(err_code);

    err_code = HAL_I2CEx_ConfigDigitalFilter(&m_i2c_handle, 0);
    ASSERT(err_code);

    // register callback function to be called when I2C RX (using DMA) is complete
    HAL_I2C_RegisterCallback(&m_i2c_handle, HAL_I2C_MEM_RX_COMPLETE_CB_ID, i2c_rx_done_cb);

    // enable I2C IRQ
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
}

void
lsm303_init(void)
{
    HAL_StatusTypeDef ret;

    init_i2c();

    // reboot LSM303 memory content
    uint8_t data[2] = {0};
    data[0] = LSM303DLHC_CTRL_REG5_A;
    data[1] = LSM303DLHC_BOOT_REBOOTMEMORY;
    ret = HAL_I2C_Master_Transmit(&m_i2c_handle, ACC_I2C_ADDRESS, data, 2, 10);
    ASSERT(ret);
}
