#ifndef APP_RTOS_RTOS_ISR_GLUE_H
#define APP_RTOS_RTOS_ISR_GLUE_H

/*
 * FreeRTOS 端口中断入口与 CubeMX 生成的向量表之间的桥接说明。
 *
 * 背景：stm32f1xx_it.c 由 CubeMX 生成，其中的 USER CODE 区在重新生成时
 * 会保留，我们把 FreeRTOS 的中断处理挂接在那里（见该文件）。
 *
 * 三个内核中断的接法各不相同，原因如下：
 *
 *  - SVC / PendSV：本固件用的 ST 版 port.c 以 "bx lr"（lr=EXC_RETURN）收尾，
 *    这要求处理函数必须是硬件异常直接跳转的向量入口，中间不能隔一层 C 函数
 *    （否则 lr 不再是 EXC_RETURN，会跳到非法地址导致 HardFault）。
 *    因此在 FreeRTOSConfig.h 里把它们 #define 成 SVC_Handler / PendSV_Handler，
 *    并让 .ioc 不生成这两个异常的包裹函数，保证符号唯一。
 *
 *  - SysTick：还要兼做 HAL 的 1ms 时基，所以在生成的 SysTick_Handler 里
 *    先 HAL_IncTick() 再调用 xPortSysTickHandler()；且必须用
 *    xTaskGetSchedulerState() 守卫，调度器启动前不能跑 RTOS 的 tick。
 */

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

// FreeRTOS 的 SysTick 处理入口。由 stm32f1xx_it.c 的 SysTick_Handler 调用，
// 调用前已确认调度器已启动（见该文件的守卫判断）。
void xPortSysTickHandler(void);

#ifdef __cplusplus
}
#endif

#endif  /* APP_RTOS_RTOS_ISR_GLUE_H */
