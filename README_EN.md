[中文](README.md) | **English**

# STM32 Intelligent Temperature Control System

A bare-metal intelligent temperature control project based on the **STM32F103C8T6**. It uses an NTC thermistor to measure ambient temperature, an OLED for display, buttons for threshold configuration, and a TB6612 motor driver with PWM to automatically regulate fan speed according to temperature. The firmware is developed with STM32CubeMX/HAL and Keil MDK, and the repository also includes the JLCEDA hardware project.

## Features

- 10 kΩ NTC + ADC temperature acquisition, with temperature calculated using the Steinhart-Hart equation
- 5-sample median filter + 30-sample moving average, with piecewise linear calibration support
- OLED display for real-time temperature, upper/lower thresholds, fan status, and sensor faults
- Button-based high/low temperature threshold configuration; parameters are written to Flash with delayed saving and verified using CRC16
- TB6612 fan driver with linear PWM speed control from 10% to 100% over the 25–50 °C range
- LED + buzzer alarm when temperature exceeds configured limits; automatic fan control is stopped when an NTC fault is detected
- USART1 outputs ADC value, temperature, NTC resistance, fan duty cycle, and fault code once per second
- Cooperative periodic scheduling implemented with `HAL_GetTick()` to avoid long blocking operations in the main loop

## Hardware

- MCU: STM32F103C8T6, 72 MHz
- Temperature sensor: 10 kΩ NTC, PA6 / ADC1_IN6
- Fan driver: TB6612FNG, PB0 / TIM3_CH3 PWM
- OLED: PB6 / PB7 software I²C
- Debug UART: USART1, 115200-8-N-1
- Other peripherals: 3 setting buttons, thermostat switch, red/green LEDs, buzzer

![Hardware Schematic](Hardware/原理图.png)

## Software Architecture

```text
main.c
  └─ app.c                  Application state machine and periodic scheduling
      ├─ app_button.c       Button debouncing and event handling
      ├─ thermostat.c       Temperature control and fan control strategy
      ├─ bsp_ntc.c          ADC, temperature conversion, filtering, and fault detection
      ├─ bsp_motor.c        TB6612 and PWM control
      ├─ bsp_eeprom.c       Flash parameter persistence
      └─ OLED / USART       Display and runtime telemetry
             ↓
       STM32F1 HAL / CMSIS
```

Main task periods: button scan 5 ms, temperature control 50 ms, NTC sampling 100 ms, OLED refresh 500 ms, and USART telemetry 1 s.

## Directory Structure

```text
.
├─ Hardware/               JLCEDA project and schematic
├─ src/
│  ├─ stairwayB.ioc        STM32CubeMX configuration
│  ├─ Core/Inc             Application and BSP headers
│  ├─ Core/Src             Application, drivers, and generated code
│  ├─ Drivers              STM32F1 HAL / CMSIS
│  └─ MDK-ARM              Keil project
├─ README.md
└─ README_EN.md
```

## Build

1. Open `src/MDK-ARM/stairwayB.uvprojx` with Keil MDK-ARM 5
2. Select the `stairwayB` target and rebuild the project
3. Flash the generated firmware using ST-Link

To modify GPIO, ADC, TIM, or USART configuration, edit `src/stairwayB.ioc` first and regenerate the project with STM32CubeMX.
