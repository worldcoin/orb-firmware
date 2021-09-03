//
// Created by Cyril on 18/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_CONFIG_BOARD_H
#define WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_CONFIG_BOARD_H

/// Board pin-mapping

#include "stm32g4xx_hal.h"
#include <stm32g4xx_it.h>
#include <stm32g4xx_clocks.h>

#define dma_rx(x)             DMA1_Channel1 ## x
#define dma_tx(x)             DMA1_Channel2 ## x
#define usart(x)              USART3 ## x

#endif //WORLDCOIN_FIRMWARE_ORB_APP_BOARDS_CONFIG_BOARD_H
