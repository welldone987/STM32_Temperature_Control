#ifndef APP_H
#define APP_H

#include "stm32f1xx_hal.h"

/**
 * @brief 初始化温控应用及其依赖的板级模块。
 * @return HAL_OK 表示 OLED、NTC 和电机均初始化成功，否则返回 HAL_ERROR。
 *
 * 该函数只在系统启动阶段调用一次，负责加载持久化阈值、初始化业务状态、
 * 建立按键防抖基准时间，并完成第一次界面绘制。
 */
HAL_StatusTypeDef App_Init(void);

/**
 * @brief 执行一次非阻塞业务调度。
 *
 * 主循环应持续调用本函数。内部所有周期任务均通过 HAL_GetTick() 判断是否
 * 到期，不在业务运行阶段使用长时间阻塞延时。
 */
void App_Process(void);

#endif /* APP_H */
