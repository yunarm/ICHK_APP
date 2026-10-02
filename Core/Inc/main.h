/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#include "stm32g0xx_hal.h"

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
#define RUNL_1_Pin GPIO_PIN_2
#define RUNL_1_GPIO_Port GPIOA
#define RUNL_2_Pin GPIO_PIN_3
#define RUNL_2_GPIO_Port GPIOA
#define HOST_USART1_TX_Pin GPIO_PIN_9
#define HOST_USART1_TX_GPIO_Port GPIOA
#define HOST_USART1_RX_Pin GPIO_PIN_10
#define HOST_USART1_RX_GPIO_Port GPIOA
#define CH_SEL_1_Pin GPIO_PIN_0
#define CH_SEL_1_GPIO_Port GPIOD
#define CH_SEL_2_Pin GPIO_PIN_1
#define CH_SEL_2_GPIO_Port GPIOD
#define CH_SEL_3_Pin GPIO_PIN_2
#define CH_SEL_3_GPIO_Port GPIOD
#define CH_SEL_4_Pin GPIO_PIN_3
#define CH_SEL_4_GPIO_Port GPIOD
#define CH_SEL_5_Pin GPIO_PIN_3
#define CH_SEL_5_GPIO_Port GPIOB
#define CH_SEL_6_Pin GPIO_PIN_4
#define CH_SEL_6_GPIO_Port GPIOB
#define CH_SEL_7_Pin GPIO_PIN_5
#define CH_SEL_7_GPIO_Port GPIOB
#define CH_SEL_8_Pin GPIO_PIN_6
#define CH_SEL_8_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
