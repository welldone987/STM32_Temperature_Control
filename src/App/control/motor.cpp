#include "control/motor.hpp"

#include "main.h"
#include "tim.h"

namespace app {

void Motor::init() {
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);                       // 使能 PB0 上的 PWM
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_SET);  // 退出待机
    stop();  // 初始保持停转，等温控逻辑按需启动
}

void Motor::set_speed(std::uint8_t percent) {
    if (percent > 100) {
        percent = 100;  // 防止占空比超界
    }

    // 方向固定为正转：AIN1=高、AIN2=低（见头文件 TB6612 逻辑说明）。
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);

    // 百分比 -> 比较寄存器值：percent% 的占空比。
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3,
                          static_cast<std::uint32_t>(percent) * kPwmPeriodCounts / 100);
}

void Motor::stop() {
    // 两路方向都拉低即停转；占空比也清零，输出彻底关闭。
    HAL_GPIO_WritePin(MOTOR_AIN1_GPIO_Port, MOTOR_AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_GPIO_Port, MOTOR_AIN2_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
}

// 唯一实例。用函数内 static 保证只初始化一次且顺序安全。
Motor& motor() {
    static Motor instance;
    return instance;
}

}  // namespace app
