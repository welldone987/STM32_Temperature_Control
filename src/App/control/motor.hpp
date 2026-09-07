#pragma once

#include <cstdint>

namespace app {

/*
 * 散热风扇电机，由 TB6612 驱动芯片驱动。
 *
 * 接线与控制方式（见 .ioc 引脚标签）：
 *   - PWM   : PB0 = TIM3_CH3，决定转速（占空比 0-100%）
 *   - AIN1/AIN2 : PA4/PA5，决定转向。本应用固定 AIN1=高、AIN2=低（正转）
 *   - STBY  : PA7，驱动使能脚，初始化时拉高保持使能
 *
 * TB6612 逻辑：AIN1=1、AIN2=0 为正转，占空比由 PWMA 输入；两路都为 0 则停转。
 *
 * 所有权：仅控制任务可以操作（RTOS 拆分后），避免多处同时写 PWM/方向。
 */
class Motor {
public:
    Motor() = default;
    // 电机是独占硬件资源，禁止拷贝。
    Motor(const Motor&) = delete;
    Motor& operator=(const Motor&) = delete;

    // 启动 PWM 输出并把 TB6612 拉出待机（STBY=高），随后置于停止态。
    void init();

    // 设置转速百分比，自动钳位到 0-100；方向固定为正转。
    void set_speed(std::uint8_t percent);

    // 停转：两路方向输入都拉低 + 占空比清零。
    void stop();

private:
    // TIM3 自动重装值（ARR=999），配合预分频得到约 1kHz PWM。
    // 占空比换算：percent/100 * (ARR) 。
    static constexpr std::uint32_t kPwmPeriodCounts = 999;
};

// 全局唯一的电机实例。
Motor& motor();

}  // namespace app
