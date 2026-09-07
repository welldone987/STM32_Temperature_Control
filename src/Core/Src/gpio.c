/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO configuration for the temperature controller board.
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

#include "gpio.h"

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* Establish safe output levels before switching pins to output mode. */
  HAL_GPIO_WritePin(GPIOA, LED_RED_Pin | LED_GREEN_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA,
                    MOTOR_AIN1_Pin | MOTOR_AIN2_Pin | MOTOR_STBY_Pin,
                    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB,
                    BUZZER_Pin | MOTOR_BIN1_Pin | MOTOR_BIN2_Pin | MOTOR_PWMB_Pin,
                    GPIO_PIN_RESET);

  /* OLED software-I2C bus is open-drain and idles high. */
  HAL_GPIO_WritePin(GPIOB, I2C_SCL_Pin | I2C_SDA_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = LED_RED_Pin | LED_GREEN_Pin |
                        MOTOR_AIN1_Pin | MOTOR_AIN2_Pin | MOTOR_STBY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BUZZER_Pin | MOTOR_BIN1_Pin |
                        MOTOR_BIN2_Pin | MOTOR_PWMB_Pin;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = I2C_SCL_Pin | I2C_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* KEY_A/B/C already have external pull-down resistors on the PCB. */
  GPIO_InitStruct.Pin = MODE_SWITCH_Pin | TEMP_UP_Pin | TEMP_DOWN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* The external temperature-control switch is active low. */
  GPIO_InitStruct.Pin = THERMO_MODE_Pin;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* USER CODE BEGIN 2 */
/* USER CODE END 2 */
