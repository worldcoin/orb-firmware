/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f3xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
 ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F3xx_IT_H
#define __STM32F3xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void
NMI_Handler(void);
void
HardFault_Handler(void);
void
MemManage_Handler(void);
void
BusFault_Handler(void);
void
UsageFault_Handler(void);
void
DebugMon_Handler(void);

// com module
void
UART4_IRQHandler(void);
void
DMA2_Channel3_IRQHandler(void);
void
DMA2_Channel5_IRQHandler(void);

// log module
void
DMA1_Channel4_IRQHandler(void);
void
DMA1_Channel5_IRQHandler(void);
void
USART1_IRQHandler(void);

// IMU - LSM303
void
DMA1_Channel6_IRQHandler(void);
void
DMA1_Channel7_IRQHandler(void);
void
EXTI4_IRQHandler(void);
void
I2C1_EV_IRQHandler(void);
void
I2C1_ER_IRQHandler(void);

// IMU - L3G
void
EXTI1_IRQHandler(void);

void
TIM1_UP_TIM16_IRQHandler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F3xx_IT_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
