//
// Created by Cyril on 18/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_ORB_V1_CONFIG_BOARD_H
#define WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_ORB_V1_CONFIG_BOARD_H

/// Board pin-mapping

#include "stm32f3xx_hal.h"
#include <stm32f3xx_it.h>
#include <stm32f3xx_clocks.h>

#define dma_rx_handler  DMA1_Channel5_IRQHandler
#define dma_tx_handler  DMA1_Channel4_IRQHandler
#define usart_handler   USART1_IRQHandler

#define LSM303_DRDY_PIN     GPIO_PIN_2
#define LSM303_INT1_PIN     GPIO_PIN_4
#define L3G_DRDY_PIN        GPIO_PIN_1

#ifndef DEBUG_UART_TX_BUFFER_SIZE
#define DEBUG_UART_TX_BUFFER_SIZE   ((uint16_t) 512)
#endif

#endif //WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_ORB_V1_CONFIG_BOARD_H
