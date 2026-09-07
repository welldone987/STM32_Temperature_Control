/**
 * @file bsp_motor.c
 * @brief TB6612 通道 A 风扇驱动封装。
 *
 * PB0/TIM3_CH3 提供 PWM，PA4/PA5 控制方向，PA7 控制 STBY。
 * 业务层只使用百分比接口，不需要知道定时器周期和桥臂电平组合。
 */

#include "bsp_motor.h"

#include "main.h"
#include "tim.h"

HAL_StatusTypeDef Motor_Init(void)
{
    HAL_StatusTypeDef status;

    /* 先清空 PWM 和方向输入，再启动定时器并拉高 STBY，避免上电瞬间误转。 */
    Motor_Stop();
    status = HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);

    if (status != HAL_OK) {
        HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_RESET);
        return status;
    }

    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_SET);
    return HAL_OK;
}

void Motor_SetSpeed(uint8_t percentage)
{
    uint32_t compare;

    /* 0% 统一走停止路径，确保 PWM 与两个方向引脚同时复位。 */
    if (percentage == 0U) {
        Motor_Stop();
        return;
    }
    if (percentage > 100U) {
        percentage = 100U;
    }

    /* AIN1=1、AIN2=0 定义为当前硬件的风扇正转方向。 */
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);

    /* ARR+1 是一个完整 PWM 周期的计数数目，使用 32 位计算避免乘法溢出。 */
    compare = ((uint32_t)percentage * (htim3.Init.Period + 1U)) / 100U;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, compare);
}

void Motor_Stop(void)
{
    /* 先撤销 PWM，再复位方向输入，保证停止过程没有额外有效脉冲。 */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0U);
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
}
