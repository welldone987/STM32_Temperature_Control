#ifndef APP_CONFIG_FREERTOS_CONFIG_H
#define APP_CONFIG_FREERTOS_CONFIG_H

/*
 * FreeRTOS 内核配置 —— stairwayB（STM32F103C8, Cortex-M3, 72 MHz）。
 *
 * 核心约束：只用静态分配。不编译任何 heap_*.c，每个任务/队列的栈和
 * 存储都由应用层（app_rtos.cpp）提供，杜绝堆内存与碎片。
 *
 * 本文件会被 C 内核源码包含，注意保持 C 兼容。
 */

#ifdef __cplusplus
extern "C" {
#endif

/* configASSERT 失败时调用的钩子（见 app_rtos.cpp）。 */
void app_rtos_assert_failed(const char *file, unsigned long line);

#ifdef __cplusplus
}
#endif

/* ---- 调度基本参数 ---- */
#define configUSE_PREEMPTION                     1   /* 抢占式调度 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0   /* 优先级数少，不用位图优化 */
#define configUSE_TICKLESS_IDLE                  0   /* 不做低功耗空闲 */
#define configCPU_CLOCK_HZ                       (72000000UL)
#define configTICK_RATE_HZ                       ((TickType_t)1000)  /* 1ms 节拍 */
#define configMAX_PRIORITIES                     6   /* 实际只用到 5 级 */
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)  /* 空闲任务栈 */
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_16_BIT_TICKS                   0   /* 32 位节拍计数 */
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1   /* 存储完成通知用到 */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_MUTEXES                        0   /* 无共享资源，不需要互斥量 */
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIME_SLICING                   1
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0

/* ---- 内存：纯静态分配 ---- */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0   /* 关闭 pvPortMalloc */

/* ---- 钩子与诊断 ---- */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW           2   /* 栈溢出检测(方式2) */
#define configUSE_MALLOC_FAILED_HOOK             0   /* 无动态分配，无需 */
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0

/* ---- 统计：关闭，省开销 ---- */
#define configGENERATE_RUN_TIME_STATS            0
#define configUSE_TRACE_FACILITY                 0
#define configUSE_STATS_FORMATTING_FUNCTIONS     0

/* ---- 协程与软件定时器：不用 ---- */
#define configUSE_CO_ROUTINES                    0
#define configUSE_TIMERS                         0

/* ---- 中断嵌套 ----
 * F103 有 4 个优先级位。优先级数值比
 * configMAX_SYSCALL_INTERRUPT_PRIORITY 更小（即更"高"）的中断，
 * 例如优先级 0 的 DMA1_Channel1，绝不允许调用任何 FreeRTOS API。
 * ADC DMA 的错误恢复路径就是按这个约束设计的（见 ntc_sensor.cpp）。 */
#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* configASSERT 接到应用钩子，失败时停机保留现场。 */
#define configASSERT(x) \
    do { if (!(x)) app_rtos_assert_failed(__FILE__, __LINE__); } while (0)

/* ---- 中断处理函数映射 ----
 * 把端口处理函数直接映射成向量名。本固件用的 ST 版 port.c 以
 * "bx lr"(lr=EXC_RETURN) 收尾，只有当硬件异常直接跳进该函数时
 * lr 才是 EXC_RETURN，因此这两个函数必须是向量表入口本身，
 * 不能被任何 C 包裹函数中转。详见 rtos_isr_glue.h。 */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler

/* ---- 任务查询 API ----
 * 打开栈高水位/调度器状态等查询，供调试时测量实际栈用量。 */
#define INCLUDE_vTaskPrioritySet             1
#define INCLUDE_uxTaskPriorityGet            1
#define INCLUDE_vTaskDelete                  0
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_vTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1   /* SysTick 守卫判断用到 */
#define INCLUDE_xTaskGetCurrentTaskHandle    1
#define INCLUDE_uxTaskGetStackHighWaterMark  1   /* 栈高水位测量 */
#define INCLUDE_eTaskGetState                1

#endif  /* APP_CONFIG_FREERTOS_CONFIG_H */
