//
// Created by Cyril on 18/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_ORB_V1_CONFIG_BOARD_H
#define WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_ORB_V1_CONFIG_BOARD_H

/// Board pin-mapping

#include "stm32g4xx_hal.h"
#include <stm32g4xx_it.h>
#include <stm32g4xx_clocks.h>

#define dma_rx_handler  DMA1_Channel1_IRQHandler
#define dma_tx_handler  DMA1_Channel2_IRQHandler
#define usart_handler   USART1_IRQHandler

#ifndef DEBUG_UART_TX_BUFFER_SIZE
#define DEBUG_UART_TX_BUFFER_SIZE   ((uint16_t) 512)
#endif

#endif //WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_ORB_V1_CONFIG_BOARD_H
