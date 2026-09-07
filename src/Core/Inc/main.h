/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define THERMO_MODE_Pin GPIO_PIN_13
#define THERMO_MODE_GPIO_Port GPIOC
#define LED_RED_Pin GPIO_PIN_1
#define LED_RED_GPIO_Port GPIOA
#define LED_GREEN_Pin GPIO_PIN_2
#define LED_GREEN_GPIO_Port GPIOA
#define MOTOR_AIN1_Pin GPIO_PIN_4
#define MOTOR_AIN1_GPIO_Port GPIOA
#define MOTOR_AIN2_Pin GPIO_PIN_5
#define MOTOR_AIN2_GPIO_Port GPIOA
#define MOTOR_STBY_Pin GPIO_PIN_7
#define MOTOR_STBY_GPIO_Port GPIOA
#define MOTOR_PWMB_Pin GPIO_PIN_1
#define MOTOR_PWMB_GPIO_Port GPIOB
#define TEMP_DOWN_Pin GPIO_PIN_10
#define TEMP_DOWN_GPIO_Port GPIOB
#define TEMP_UP_Pin GPIO_PIN_11
#define TEMP_UP_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_12
#define BUZZER_GPIO_Port GPIOB
#define MOTOR_BIN1_Pin GPIO_PIN_13
#define MOTOR_BIN1_GPIO_Port GPIOB
#define MOTOR_BIN2_Pin GPIO_PIN_14
#define MOTOR_BIN2_GPIO_Port GPIOB
#define MODE_SWITCH_Pin GPIO_PIN_15
#define MODE_SWITCH_GPIO_Port GPIOB
#define OLED_SCL_Pin GPIO_PIN_6
#define OLED_SCL_GPIO_Port GPIOB
#define OLED_SDA_Pin GPIO_PIN_7
#define OLED_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/* 引脚宏（LED_RED_Pin、TEMP_UP_Pin、MODE_SWITCH_Pin、OLED_SCL_Pin 等）
 * 由 CubeMX 根据 .ioc 中的用户标签在本区域上方自动生成，
 * 不要在这里重复定义，以免重新生成时冲突。 */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
