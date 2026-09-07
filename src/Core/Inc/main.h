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

/* USER CODE BEGIN Private defines */
/* Input devices. KEY_A/B/C use external pull-down resistors. */
#define MODE_SWITCH_Pin             GPIO_PIN_15
#define MODE_SWITCH_GPIO_Port       GPIOB
#define THERMO_MODE_Pin             GPIO_PIN_13
#define THERMO_MODE_GPIO_Port       GPIOC
#define TEMP_UP_Pin                 GPIO_PIN_11
#define TEMP_UP_GPIO_Port           GPIOB
#define TEMP_DOWN_Pin               GPIO_PIN_10
#define TEMP_DOWN_GPIO_Port         GPIOB

/* Indicators. The two LEDs are active low; the buzzer is active high. */
#define LED_RED_Pin                 GPIO_PIN_1
#define LED_RED_GPIO_Port           GPIOA
#define LED_GREEN_Pin               GPIO_PIN_2
#define LED_GREEN_GPIO_Port         GPIOA
#define BUZZER_Pin                  GPIO_PIN_12
#define BUZZER_GPIO_Port            GPIOB

/* NTC divider input: PA6 / ADC1_IN6. */
#define NTC_ADC_Pin                 GPIO_PIN_6
#define NTC_ADC_GPIO_Port           GPIOA

/* TB6612 channel A drives the fan; PB0 is TIM3_CH3 PWM output. */
#define MOTOR_PWMA_Pin              GPIO_PIN_0
#define MOTOR_PWMA_GPIO_Port        GPIOB
#define MOTOR_AIN1_Pin              GPIO_PIN_4
#define MOTOR_AIN1_GPIO_Port        GPIOA
#define MOTOR_AIN2_Pin              GPIO_PIN_5
#define MOTOR_AIN2_GPIO_Port        GPIOA
#define MOTOR_STBY_Pin              GPIO_PIN_7
#define MOTOR_STBY_GPIO_Port        GPIOA

/* Unused TB6612 channel B is held low by MX_GPIO_Init(). */
#define MOTOR_BIN1_Pin              GPIO_PIN_13
#define MOTOR_BIN1_GPIO_Port        GPIOB
#define MOTOR_BIN2_Pin              GPIO_PIN_14
#define MOTOR_BIN2_GPIO_Port        GPIOB
#define MOTOR_PWMB_Pin              GPIO_PIN_1
#define MOTOR_PWMB_GPIO_Port        GPIOB

/* OLED software-I2C pins. */
#define I2C_SCL_Pin                 GPIO_PIN_6
#define I2C_SCL_GPIO_Port           GPIOB
#define I2C_SDA_Pin                 GPIO_PIN_7
#define I2C_SDA_GPIO_Port           GPIOB

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
