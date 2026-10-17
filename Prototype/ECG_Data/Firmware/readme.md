# Wearable ECG Monitoring System — Firmware

This directory contains two separate firmware projects for the Wearable ECG Monitoring System prototype. Each project targets a different STM32 microcontroller platform.

## Firmware Versions

| Directory | Target Platform | Description |
|---|---|---|
| [`ecg_stm32f103`](./ecg_stm32f103/) | STM32F103 | ECG firmware project for the STM32F103-based prototype. |
| [`STM32WBA52 files`](./STM32WBA52%20files/) | STM32WBA52 | Firmware files for checking Bluetooth Low Energy (BLE) communication on the STM32WBA52 platform. |

## Getting Started

Select the folder that matches your hardware platform. Before building or flashing the firmware, review its source files and project configuration to identify the required development tools, hardware connections, and communication settings.

The projects are maintained separately to support development and testing on their respective platforms.
