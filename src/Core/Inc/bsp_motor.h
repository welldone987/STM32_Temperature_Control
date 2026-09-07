#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include "main.h"

/**
 * @brief 启动 TIM3_CH3 PWM，并使能 TB6612 通道 A。
 * @return PWM 启动结果；失败时保持 STBY 低电平，电机不会被使能。
 */
HAL_StatusTypeDef Motor_Init(void);

/**
 * @brief 按正转方向设置风扇占空比。
 * @param percentage 期望占空比，范围 0–100；超过 100 时自动限幅。
 */
void Motor_SetSpeed(uint8_t percentage);

/** @brief 将 PWM 和方向输入清零，使通道 A 停止输出。 */
void Motor_Stop(void);

#endif /* BSP_MOTOR_H */

