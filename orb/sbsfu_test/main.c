/**
  ******************************************************************************
  * @file    main.c
  * @author  MCD Application Team
  * @brief   Main application file.
  *          This application demonstrates Firmware Update, protections
  *          and crypto testing functionalities.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "se_def.h"
#include "flash_if.h"
#include "se_interface_application.h"
#include "common.h"
#include "test_protections.h"
#include "fw_update_app.h"
#include "se_user_code.h"


/** @addtogroup USER_APP User App Example
  * @{
  */


/** @addtogroup USER_APP_COMMON Common
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/



#define USER_APP_NBLINKS  ((uint8_t) 1U)
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t *pUserAppId;
const uint8_t UserAppId = 'A';
SE_ErrorStatus eRetStatus = SE_ERROR;
SE_StatusTypeDef eStatus;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
void FW_APP_PrintMainMenu(void);
_Noreturn void FW_APP_Run(void);

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  uint32_t i = 0U;
  /*  set example to const : this const changes in binary without rebuild */
  pUserAppId = (uint8_t *)&UserAppId;



  /* STM32G4xx HAL library initialization:
  - Configure the Flash prefetch
  - Systick timer is configured by default as source of time base, but user
  can eventually implement his proper time base source (a general purpose
  timer for example or other time source), keeping in mind that Time base
  duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
  handled in milliseconds basis.
  - Set NVIC Group Priority to 4
  - Low Level Initialization
  */
  HAL_Init();

  /* De-configure previous configuration done during SBSFU execution */
  HAL_RCC_DeInit();

  /* Configure the system clock */
  SystemClock_Config();

  /* Flash driver initialization*/
  FLASH_If_Init();

  /* Board BSP  Configuration-------------------------------------------------*/

  /* LED Init*/
  // TODO?

  /* If the SecureBoot configured the IWDG, UserApp must reload IWDG counter with value defined in the reload register*/
  WRITE_REG(IWDG->KR, IWDG_KEY_RELOAD);

  /* Configure Communication module */
  COM_Init();

  /* Configure button TODO */
//  BUTTON_INIT();

  printf("\r\n======================================================================");
  printf("\r\n=              (C) COPYRIGHT 2017 STMicroelectronics                 =");
  printf("\r\n=                                                                    =");
  printf("\r\n=                          User App #%c                               =", *pUserAppId);
  printf("\r\n======================================================================");
  printf("\r\n\r\n");


  /* User App firmware runs*/
  FW_APP_Run();

  while (1U)
  {}

}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 150000000
  *            HCLK(Hz)                       = 150000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            HSI Frequency(Hz)              = 16000000
  *            PLL_M                          = 4
  *            PLL_N                          = 75
  *            PLL_P                          = 2
  *            PLL_Q                          = 2
  *            PLL_R                          = 2
  *            Flash Latency(WS)              = 7
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* Activate PLL with HSI as source */
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN            = 75;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType           = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | \
                                           RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource        = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider       = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider      = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider      = RCC_HCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}


/**
  * @brief  Display the TEST Main Menu choices on HyperTerminal
  * @param  None.
  * @retval None.
  */
void FW_APP_PrintMainMenu(void)
{
  printf("\r\n=================== Main Menu ============================\r\n\n");
  printf("  Download a new Fw Image ------------------------------- 1\r\n\n");
  printf("  Test Protections -------------------------------------- 2\r\n\n");
  printf("  Test SE User Code ------------------------------------- 3\r\n\n");
  printf("  Multiple download ------------------------------------- 4\r\n\n");
  printf("  Validate a FW Image------------------------------------ 5\r\n\n");
  printf("  Selection :\r\n\n");
}

/**
  * @brief  Display the TEST Main Menu choices on HyperTerminal
  * @param  None.
  * @retval None.
  */
_Noreturn void FW_APP_Run(void)
{
  uint8_t key = 0U;

  /* Print Main Menu message*/
  FW_APP_PrintMainMenu();

  while (1U)
  {
    /* If the SecureBoot configured the IWDG, UserApp must reload IWDG counter with value defined in the reload
       register */
    WRITE_REG(IWDG->KR, IWDG_KEY_RELOAD);

    /* Clean the input path */
    COM_Flush();

    /* Receive key */
    if (COM_Receive(&key, 1U, RX_TIMEOUT) == HAL_OK)
    {
      switch (key)
      {
        case '1' :
          FW_UPDATE_Run();
          break;
        case '2' :
          TEST_PROTECTIONS_RunMenu();
          break;
        case '3' :
          SE_USER_CODE_RunMenu();
          break;
        case '4' :
          FW_UPDATE_MULTIPLE_RunMenu();
          break;
        case '5' :
          FW_VALIDATE_RunMenu();
          break;
        default:
          printf("Invalid Number !\r");
          break;
      }

      /*Print Main Menu message*/
      FW_APP_PrintMainMenu();
    }

//    BSP_LED_Toggle(LED_GREEN);
  }
}


#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1U)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
