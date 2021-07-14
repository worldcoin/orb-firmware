//
// Created by Cyril on 14/07/2021.
//

#include <lsm303.h>
#include "board.h"
#include <stm32f3xx_hal_i2c.h>
#include "imu.h"

static I2C_HandleTypeDef m_i2c_handle;
static DMA_HandleTypeDef m_dma_i2c1_rx;
static DMA_HandleTypeDef m_dma_i2c1_tx;

static void
i2c_further_init(struct __I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef init;
    HAL_StatusTypeDef ret;

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

    /* Peripheral clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* Peripheral DMA init*/

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
init_i2c(void)
{
    HAL_StatusTypeDef ret;

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

    ret = HAL_I2C_Init(&m_i2c_handle);
    ASSERT(ret);

    ret = HAL_I2CEx_ConfigAnalogFilter(&m_i2c_handle, I2C_ANALOGFILTER_ENABLE);
    ASSERT(ret);

    ret = HAL_I2CEx_ConfigDigitalFilter(&m_i2c_handle, 0);
    ASSERT(ret);
}

ret_code_t
imu_init(void)
{
    init_i2c();

    lsm303_init();

    return RET_SUCCESS;
}