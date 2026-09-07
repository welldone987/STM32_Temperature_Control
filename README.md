**中文** | [English](README_EN.md)

# STM32 智能温度控制系统

基于 **STM32F103C8T6** 的裸机智能温控项目，使用 NTC 采集环境温度，通过 OLED 显示、按键设置阈值，并使用 TB6612 + PWM 根据温度自动调节风扇转速。工程采用 STM32CubeMX/HAL + Keil MDK 开发，同时包含嘉立创 EDA 硬件工程。

## 功能

- 10 kΩ NTC + ADC 温度采集，使用 Steinhart-Hart 方程换算温度
- 5 点中值滤波 + 30 点移动平均，并支持分段线性标定
- OLED 显示实时温度、上下限、风扇状态与传感器故障
- 按键设置高/低温报警阈值，参数延迟写入 Flash 并通过 CRC16 校验
- TB6612 驱动风扇，25–50 ℃ 区间内由 10% 至 100% 线性调速
- 温度越界时通过 LED + 蜂鸣器报警；NTC 异常时停止自动风扇
- USART1 每秒输出 ADC、温度、NTC 电阻、风扇占空比和故障码
- 使用 `HAL_GetTick()` 实现协作式周期调度，避免主循环长时间阻塞

## 硬件

- MCU：STM32F103C8T6，72 MHz
- 温度传感器：10 kΩ NTC，PA6 / ADC1_IN6
- 风扇驱动：TB6612FNG，PB0 / TIM3_CH3 PWM
- OLED：PB6 / PB7 软件 I²C
- 调试串口：USART1，115200-8-N-1
- 其他：3 个设置按键、温控开关、红绿 LED、蜂鸣器

![硬件原理图](Hardware/原理图.png)

## 软件结构

```text
main.c
  └─ app.c                  应用状态机与周期调度
      ├─ app_button.c       按键防抖与事件处理
      ├─ thermostat.c       温控与风扇控制策略
      ├─ bsp_ntc.c          ADC、温度换算、滤波与故障检测
      ├─ bsp_motor.c        TB6612 与 PWM 控制
      ├─ bsp_eeprom.c       Flash 参数持久化
      └─ OLED / USART       显示与运行遥测
             ↓
       STM32F1 HAL / CMSIS
```

主要任务周期为：按键扫描 5 ms、温控 50 ms、NTC 采样 100 ms、OLED 刷新 500 ms、USART 遥测 1 s。

## 目录

```text
.
├─ Hardware/               嘉立创 EDA 工程与原理图
├─ src/
│  ├─ stairwayB.ioc        STM32CubeMX 配置
│  ├─ Core/Inc             应用与 BSP 头文件
│  ├─ Core/Src             应用、驱动与生成代码
│  ├─ Drivers              STM32F1 HAL / CMSIS
│  └─ MDK-ARM              Keil 工程
├─ README.md
└─ README_EN.md
```

## 构建

1. 使用 Keil MDK-ARM 5 打开 `src/MDK-ARM/stairwayB.uvprojx`
2. 选择 `stairwayB` Target 并 Rebuild
3. 使用 ST-Link 烧录生成的程序

如需修改 GPIO、ADC、TIM 或 USART 配置，优先修改 `src/stairwayB.ioc` 后通过 STM32CubeMX 重新生成代码。
