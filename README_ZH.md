**中文** | [English](README.md)

# STM32 智能温度控制系统

![STM32F103](https://img.shields.io/badge/-STM32F103-03234B?logo=stmicroelectronics&logoColor=white)
![STM32 HAL](https://img.shields.io/badge/-STM32%20HAL-03234B?logo=stmicroelectronics&logoColor=white)
![CubeCLT](https://img.shields.io/badge/-CubeCLT-03234B?logo=stmicroelectronics&logoColor=white)
![C](https://img.shields.io/badge/-C-A8B9CC?logo=c&logoColor=black)
[![Build](https://img.shields.io/github/actions/workflow/status/welldone987/STM32_Temperature_Control/stm32-ci.yml?branch=main&label=Build)](https://github.com/welldone987/STM32_Temperature_Control/actions/workflows/stm32-ci.yml)

基于 **STM32F103C8T6** 的智能温控项目。系统使用 NTC 采集环境温度，通过 OLED 显示、按键设置阈值，并使用 TB6612 + PWM 自动调节风扇转速。

当前软件已重构为 **STM32CubeMX / HAL + FreeRTOS + C++17 应用层**，使用 **STM32CubeCLT（GNU Arm Toolchain + CMake + Ninja）** 构建。

## 主要功能

- ADC1 + DMA 循环采样 NTC，温度链为：50 点 DMA 均值 → 中值滤波 → 滑动平均 → Steinhart-Hart → 分段线性标定
- OLED 显示实时温度、上下限、风扇状态与传感器故障
- 按键设置高/低温报警阈值，参数通过 CRC16 校验后写入片内 Flash
- TB6612 驱动风扇，25–50 ℃ 区间内由 10% 至 100% 线性调速
- 温度越界时通过 LED + 蜂鸣器报警；NTC 异常时停止自动风扇
- USART1 输出运行遥测信息，便于调试温度链和控制状态
- FreeRTOS 全静态创建任务和队列，不依赖动态内存；C++ 层关闭异常、RTTI 和线程安全静态初始化

## 软件结构

```text
src/
├─ App/                    C++17 应用层
│  ├─ common/              公共类型与配置
│  ├─ control/             风扇与温控执行逻辑
│  ├─ sensor/              NTC 采样、滤波、温度换算与故障检测
│  ├─ display/             OLED 与软件 I²C
│  ├─ storage/             Flash 参数持久化
│  ├─ rtos/                FreeRTOS 任务、队列与 ISR glue
│  └─ config/              FreeRTOSConfig
├─ Core/                   STM32CubeMX 生成的初始化与 HAL glue
├─ Drivers/                STM32F1 HAL / CMSIS
├─ FreeRTOS/               FreeRTOS Kernel
├─ cmake/                  STM32CubeMX 与 GNU Arm CMake 配置
├─ CMakeLists.txt
├─ CMakePresets.json
├─ stairwayB.ioc
├─ STM32F103XX_FLASH.ld
└─ startup_stm32f103xb.s
```

应用由 CubeMX 生成的 `main.c` 通过 C 接口进入 C++ 层：

```text
main.c
  ├─ app_init()
  │    ├─ 读取持久化阈值
  │    ├─ 启动 ADC DMA
  │    └─ 初始化执行器
  └─ app_start()
       └─ FreeRTOS Scheduler
            ├─ control   10 ms   按键 / 报警 / 风扇控制
            ├─ sensor   100 ms   温度采样与滤波
            ├─ display  500 ms   OLED 增量刷新
            └─ storage  event    Flash 后台写入
```

任务间主要使用长度为 1 的覆写队列和任务通知传递最新状态，避免不必要的互斥与堆分配。

## 硬件

- MCU：STM32F103C8T6，72 MHz
- 温度传感器：10 kΩ NTC，PA6 / ADC1_IN6
- 风扇驱动：TB6612FNG，PB0 / TIM3_CH3 PWM
- OLED：PB6 / PB7 软件 I²C
- 调试串口：USART1，115200-8-N-1
- 其他：3 个设置按键、温控开关、红绿 LED、蜂鸣器

![硬件原理图](Hardware/原理图.png)

## 构建

安装 STM32CubeCLT，并确保其中的 `arm-none-eabi-gcc`、CMake 和 Ninja 已加入环境变量。

```bash
cd src
cmake --preset Debug
cmake --build --preset Debug
```

体积优化构建：

```bash
cmake --preset MinSizeRel
cmake --build --preset MinSizeRel
```

生成的 ELF 与 map 文件位于 `src/build/<Preset>/`。

如需修改 GPIO、ADC、DMA、TIM 或 USART 配置，优先修改 `src/stairwayB.ioc` 并通过 STM32CubeMX 重新生成；业务逻辑集中在 `src/App/`，与 CubeMX 生成层分离。

## CI

GitHub Actions 在 push / pull request 时使用 STM32CubeCLT 环境分别构建 `Debug` 与 `MinSizeRel`，并上传 ELF 和 map 文件作为构建产物。
