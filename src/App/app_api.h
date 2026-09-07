#ifndef APP_APP_API_H
#define APP_APP_API_H

/*
 * 应用层对外的 C 语言入口。
 *
 * 为什么单独提供 C 接口：CubeMX 生成的 main.c 是 C 文件，无法直接调用
 * C++ 符号；这里用 extern "C" 暴露两个入口，真正的实现都在 App/ 下的
 * C++ 源文件里。这样重新生成 main.c 也不会破坏应用层。
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 应用启动（一次性，运行在调度器启动之前）。
 * 职责：读取持久化的温度阈值、启动 ADC DMA 采样、初始化风扇驱动。
 * 前置条件：必须在所有 MX_*_Init() 之后调用（依赖外设已初始化）。
 */
void app_init(void);

/*
 * 创建全部应用任务并启动 FreeRTOS 调度器。
 * 正常情况下不会返回；main.c 里的 while(1) 只是不可达的兜底。
 */
void app_start(void);

#ifdef __cplusplus
}
#endif

#endif  // APP_APP_API_H
