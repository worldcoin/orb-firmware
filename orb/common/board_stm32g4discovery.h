//
// Created by Cyril on 09/09/2021.
//

#ifndef ORB_FIRMWARE_ORB_COMMON_BOARD_STM32DISCOVERY_H
#define ORB_FIRMWARE_ORB_COMMON_BOARD_STM32DISCOVERY_H

#include "stm32g4xx_hal.h"
#include <stm32g4xx_it.h>
#include <stm32g4xx_clocks.h>

#define dma_rx(x)             DMA1_Channel1 ## x
#define dma_tx(x)             DMA1_Channel2 ## x
#define usart(x)              USART3 ## x

//typedef enum
//{
//    LED2 = 0U,
//    LED_GREEN = LED2,
//    LEDn
//}Led_TypeDef;

#endif //ORB_FIRMWARE_ORB_COMMON_BOARD_STM32DISCOVERY_H
