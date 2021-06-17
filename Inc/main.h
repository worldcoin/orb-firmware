/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal.h"

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define UC_TSC_CAP_Pin GPIO_PIN_2
#define UC_TSC_CAP_GPIO_Port GPIOE
#define SUPPLY_3V8_PG_Pin GPIO_PIN_3
#define SUPPLY_3V8_PG_GPIO_Port GPIOE
#define UC_TSC_ELECTRODE_Pin GPIO_PIN_4
#define UC_TSC_ELECTRODE_GPIO_Port GPIOE
#define nBTN_POWER_Pin GPIO_PIN_13
#define nBTN_POWER_GPIO_Port GPIOC
#define SUPPLY_EN_1V8_Pin GPIO_PIN_14
#define SUPPLY_EN_1V8_GPIO_Port GPIOC
#define TOBY_SUPPLY_EN_Pin GPIO_PIN_15
#define TOBY_SUPPLY_EN_GPIO_Port GPIOC
#define UC_OC_UNIT_PWM3_Pin GPIO_PIN_9
#define UC_OC_UNIT_PWM3_GPIO_Port GPIOF
#define UC_OC_UNIT_PWM4_Pin GPIO_PIN_10
#define UC_OC_UNIT_PWM4_GPIO_Port GPIOF
#define SUPPLY_1V8_PG_Pin GPIO_PIN_0
#define SUPPLY_1V8_PG_GPIO_Port GPIOC
#define SUPPLY_3V3_PG_Pin GPIO_PIN_1
#define SUPPLY_3V3_PG_GPIO_Port GPIOC
#define JETSON_SYS_RESET_Pin GPIO_PIN_2
#define JETSON_SYS_RESET_GPIO_Port GPIOC
#define EN_12V_SUPPLY_Pin GPIO_PIN_3
#define EN_12V_SUPPLY_GPIO_Port GPIOC
#define UC_BAT_VOLTAGE_SENS_Pin GPIO_PIN_0
#define UC_BAT_VOLTAGE_SENS_GPIO_Port GPIOA
#define GPS_RESET_Pin GPIO_PIN_1
#define GPS_RESET_GPIO_Port GPIOA
#define JETSON_MOD_SLEEP_Pin GPIO_PIN_2
#define JETSON_MOD_SLEEP_GPIO_Port GPIOA
#define UC_AMP_SEL_UC_nJETSON_Pin GPIO_PIN_3
#define UC_AMP_SEL_UC_nJETSON_GPIO_Port GPIOA
#define SUPPLY_EN_3V3_Pin GPIO_PIN_4
#define SUPPLY_EN_3V3_GPIO_Port GPIOA
#define UC_SD_SCK_Pin GPIO_PIN_5
#define UC_SD_SCK_GPIO_Port GPIOA
#define UC_SD_MISO_Pin GPIO_PIN_6
#define UC_SD_MISO_GPIO_Port GPIOA
#define UC_SD_MOSI_Pin GPIO_PIN_7
#define UC_SD_MOSI_GPIO_Port GPIOA
#define UC_ADC_EXT_NTC1_Pin GPIO_PIN_4
#define UC_ADC_EXT_NTC1_GPIO_Port GPIOC
#define UC_ADC_EXT_NTC2_Pin GPIO_PIN_5
#define UC_ADC_EXT_NTC2_GPIO_Port GPIOC
#define LSM9DS1_DRDY_M_Pin GPIO_PIN_1
#define LSM9DS1_DRDY_M_GPIO_Port GPIOB
#define LSM9DS1_INT_A_G_Pin GPIO_PIN_2
#define LSM9DS1_INT_A_G_GPIO_Port GPIOB
#define UC_CAM2_FLASH_FROM_SENSOR_Pin GPIO_PIN_7
#define UC_CAM2_FLASH_FROM_SENSOR_GPIO_Port GPIOE
#define UC_SD_CD_Pin GPIO_PIN_8
#define UC_SD_CD_GPIO_Port GPIOE
#define UC_SD_nCS_Pin GPIO_PIN_9
#define UC_SD_nCS_GPIO_Port GPIOE
#define UC_SD_POWER_nENABLE_Pin GPIO_PIN_10
#define UC_SD_POWER_nENABLE_GPIO_Port GPIOE
#define UC_FRONT_UNIT_PWM2_Pin GPIO_PIN_11
#define UC_FRONT_UNIT_PWM2_GPIO_Port GPIOE
#define UC_FRONT_UNIT_PWM3_Pin GPIO_PIN_13
#define UC_FRONT_UNIT_PWM3_GPIO_Port GPIOE
#define UC_FRONT_UNIT_PWM4_Pin GPIO_PIN_14
#define UC_FRONT_UNIT_PWM4_GPIO_Port GPIOE
#define VBAT_AUDIO_EN_Pin GPIO_PIN_11
#define VBAT_AUDIO_EN_GPIO_Port GPIOB
#define AUDIO_UC_LRCLK_Pin GPIO_PIN_12
#define AUDIO_UC_LRCLK_GPIO_Port GPIOB
#define AUDIO_UC_SCLK_Pin GPIO_PIN_13
#define AUDIO_UC_SCLK_GPIO_Port GPIOB
#define AUDIO_SD_MUX_TO_UC_Pin GPIO_PIN_14
#define AUDIO_SD_MUX_TO_UC_GPIO_Port GPIOB
#define AUDIO_SD_UC_TO_MUX_Pin GPIO_PIN_15
#define AUDIO_SD_UC_TO_MUX_GPIO_Port GPIOB
#define nUART_UC_TO_BAT_Pin GPIO_PIN_8
#define nUART_UC_TO_BAT_GPIO_Port GPIOD
#define nUART_BAT_TO_UC_Pin GPIO_PIN_9
#define nUART_BAT_TO_UC_GPIO_Port GPIOD
#define VBAT_FRONT_UNIT_EN_Pin GPIO_PIN_10
#define VBAT_FRONT_UNIT_EN_GPIO_Port GPIOD
#define UC_OC_UNIT_PWM1_Pin GPIO_PIN_12
#define UC_OC_UNIT_PWM1_GPIO_Port GPIOD
#define UC_OC_UNIT_PWM2_Pin GPIO_PIN_13
#define UC_OC_UNIT_PWM2_GPIO_Port GPIOD
#define UC_FRONT_UNIT_PWM1_Pin GPIO_PIN_6
#define UC_FRONT_UNIT_PWM1_GPIO_Port GPIOC
#define UC_LUCID_CAM1_GPIO_OUT_Pin GPIO_PIN_8
#define UC_LUCID_CAM1_GPIO_OUT_GPIO_Port GPIOA
#define AUDIO_AMP_nFAULT_Pin GPIO_PIN_10
#define AUDIO_AMP_nFAULT_GPIO_Port GPIOA
#define BUCK_5V_EN_Pin GPIO_PIN_11
#define BUCK_5V_EN_GPIO_Port GPIOA
#define JETSON_nSYS_RESET_1V8_Pin GPIO_PIN_12
#define JETSON_nSYS_RESET_1V8_GPIO_Port GPIOA
#define SUPPLY_5V_PG_Pin GPIO_PIN_6
#define SUPPLY_5V_PG_GPIO_Port GPIOF
#define AUDIO_AMP_nPDN_Pin GPIO_PIN_15
#define AUDIO_AMP_nPDN_GPIO_Port GPIOA
#define UART_UC_TO_GPS_Pin GPIO_PIN_10
#define UART_UC_TO_GPS_GPIO_Port GPIOC
#define UART_GPS_TO_UC_Pin GPIO_PIN_11
#define UART_GPS_TO_UC_GPIO_Port GPIOC
#define CAM_VBAT_EN_Pin GPIO_PIN_12
#define CAM_VBAT_EN_GPIO_Port GPIOC
#define JETSON_POWER_nEN_Pin GPIO_PIN_0
#define JETSON_POWER_nEN_GPIO_Port GPIOD
#define JETSON_SHUTDOWN_REQ_Pin GPIO_PIN_1
#define JETSON_SHUTDOWN_REQ_GPIO_Port GPIOD
#define JETSON_SLEEP_WAKE_Pin GPIO_PIN_2
#define JETSON_SLEEP_WAKE_GPIO_Port GPIOD
#define UC_LUCID_CAM1_GPIO_IN_Pin GPIO_PIN_3
#define UC_LUCID_CAM1_GPIO_IN_GPIO_Port GPIOD
#define UC_LUCID_CAM1_GPIO_IN_EXTI_IRQn EXTI3_IRQn
#define VBAT_OC_UNIT_EN_Pin GPIO_PIN_4
#define VBAT_OC_UNIT_EN_GPIO_Port GPIOD
#define UART_DEBUG_UC_TO_EXT_Pin GPIO_PIN_5
#define UART_DEBUG_UC_TO_EXT_GPIO_Port GPIOD
#define UART_DEBUG_EXT_TO_UC_Pin GPIO_PIN_6
#define UART_DEBUG_EXT_TO_UC_GPIO_Port GPIOD
#define LED_GREEN_Pin GPIO_PIN_4
#define LED_GREEN_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_5
#define LED_RED_GPIO_Port GPIOB
#define USART_STM32_TO_JETSON_Pin GPIO_PIN_6
#define USART_STM32_TO_JETSON_GPIO_Port GPIOB
#define USART_JETSON_TO_STM32_Pin GPIO_PIN_7
#define USART_JETSON_TO_STM32_GPIO_Port GPIOB
#define UC_PWM_FAN_Pin GPIO_PIN_8
#define UC_PWM_FAN_GPIO_Port GPIOB
#define UC_CAM_TRIGGER_TO_SENSOR_Pin GPIO_PIN_9
#define UC_CAM_TRIGGER_TO_SENSOR_GPIO_Port GPIOB
#define USB_nRESET_Pin GPIO_PIN_0
#define USB_nRESET_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
