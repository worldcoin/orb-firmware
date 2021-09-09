//
// Created by Cyril on 09/09/2021.
//

#ifndef ORB_FIRMWARE_ORB_COMMON_BOARD_PROTO2_H
#define ORB_FIRMWARE_ORB_COMMON_BOARD_PROTO2_H

#include "stm32g4xx_hal.h"
#include <stm32g4xx_it.h>
#include <stm32g4xx_clocks.h>

#define dma_rx(x)             DMA1_Channel1 ## x
#define dma_tx(x)             DMA1_Channel2 ## x
#define usart(x)              USART3 ## x

typedef enum
{
    LED2 = 0U,
    LED_GREEN = LED2,
    LEDn
}Led_TypeDef;

#if defined (USE_NUCLEO_64)
typedef enum
{
  BUTTON_USER = 0U,
  BUTTONn
}Button_TypeDef;

typedef enum
{
  BUTTON_MODE_GPIO = 0U,
  BUTTON_MODE_EXTI = 1U
}ButtonMode_TypeDef;

#endif //ORB_FIRMWARE_ORB_COMMON_BOARD_PROTO2_H
