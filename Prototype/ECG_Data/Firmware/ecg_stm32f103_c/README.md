# STM32F103C8 ECG firmware in C

This converts the supplied `ecg_stm32f103/src/main.cpp` to native STM32 HAL C. It targets the STM32F103C8T6 Blue Pill, not the STM32WBA52 project discussed earlier. The application contains no Arduino or C++ dependency.

## Files

- `Core/Src/main.c`: peripheral initialization, DMA acquisition, packet queue, HM-10 startup commands, and LED sequence.
- `Core/Src/ecg_signal.c`: fixed-point baseline removal and compatible binary framing.
- `Core/Src/stm32f1xx_hal_msp.c`: peripheral clocks, GPIO alternate functions, DMA links, and interrupt priorities.
- `Core/Src/stm32f1xx_it.c`: required DMA/UART/SysTick interrupt handlers.
- `Core/Inc/`: application declarations, configuration, and enabled HAL modules.
- `Core/Src/startup_support.c`: weak initialization hooks for the standalone GNU build.
- `STM32F103C8_FLASH.ld`: 64 KiB flash/20 KiB RAM memory layout, with 1 KiB minimum stack space checked by the linker.
- `build.py`: reproducible standalone cross-build against an installed STM32CubeF1 package.
- `platformio.ini`: STM32Cube configuration replacing the original Arduino framework.
- `tests/`: host tests; these must not be added to the embedded target.
- `firmware/`: the verified build's ELF, BIN, and HEX files.

ST HAL/CMSIS drivers are dependencies supplied by STM32CubeF1; they are not copied into this ZIP. Application comments are kept minimal.

## Connections

| Function | STM32 pin | Configuration |
|---|---|---|
| ECG analog input | PA7 | ADC1 channel 7 |
| Frontend SDN | PA4 | Active-high enable, as in the original |
| External status LED | PB11 | Active-high output |
| HM-10 RX | PA9 | USART1 TX, 9600 baud, 8N1 |
| HM-10 TX | PA10 | USART1 RX |
| Optional USB-to-UART RX | PA2 | USART2 debug TX, 115200 baud, 8N1 |
| Optional USB-to-UART TX | PA3 | USART2 debug RX |

Use a common ground and suitable 3.3 V signal levels. PB11 is an external LED connection, not the Blue Pill's usual onboard PC13 LED.

The supplied Blue Pill Arduino configuration maps the default `Serial` port to USART1. The original also uses `Serial1` for the HM-10, creating a potential shared-peripheral conflict. This implementation explicitly uses USART1 for the HM-10 and USART2 for debug output. Debug output is hardware UART, not USB CDC.

## Preserved behavior

- Nominal sampling rate: 250 samples/s.
- Five samples per 14-byte frame.
- Frame format: `AA 55 sequence sample0_lo sample0_hi ... sample4_lo sample4_hi checksum`.
- Samples are signed 16-bit, little endian.
- Checksum is the low eight bits of the sum of the sequence and ten payload bytes; the sync bytes are excluded.
- Sequence rolls from 255 to 0. It advances even when the local queue drops a packet, allowing receiver-side gap detection modulo 256.
- Baseline coefficient remains 0.0063. This is DC removal, not a new clinical ECG bandpass filter.
- LED blinks with 250 ms toggles for 10 seconds, stays on for 10 seconds, then stays off.
- The original HM-10 AT command strings and startup waits are retained.

By default, the hardware ADC's 12-bit readings are reduced to the original Arduino default 10-bit scale before baseline removal. Set `ECG_OUTPUT_ADC_BITS` to `12` in `ecg_config.h` if your receiver should use the full ADC resolution; the numerical scale then changes by approximately four times. Values are ADC counts, not calibrated millivolts.

## Optimizations and fault handling

TIM3 produces the ADC trigger directly every 4 ms, removing main-loop scheduling drift. ADC1 transfers results through DMA1 channel 1 into a ten-sample circular buffer. Each half-buffer interrupt processes five samples while DMA fills the other half. Interrupt service must remain responsive within the next 20 ms.

The baseline uses 32-bit fixed-point arithmetic with a carried fractional remainder. The default firmware contains no software floating-point helper routines and uses no dynamic allocation. A single-producer/single-consumer frame queue separates acquisition from USART1 DMA transmission on DMA1 channel 4. Interrupt handlers and DMA buffer lifetimes are included.

The eight-slot queue holds seven waiting frames, plus one active transmission. A 9600-baud 8N1 connection can carry 960 bytes/s; this stream requires 700 bytes/s before the HM-10's wireless behavior is considered. USART completion confirms local UART transmission, not delivery to a BLE client.

Queue overflow drops the newest complete frame. A stuck UART transmission is aborted after 100 ms and counted as dropped; a partial received frame must be discarded using sync/checksum validation. ADC errors or 100 ms without a completed ADC block enter `Error_Handler`, stop TIM3, disable the analog frontend, and leave PB11 high. No automatic ADC restart is attempted.

Inspect `ecg_diagnostics` in the debugger for sample, packet, ADC rail, queue drop, and error counters. An ADC rail count identifies a potential clipped input; the original sample stream is preserved rather than silently replaced.

## HM-10 configuration

The module must already communicate at 9600 baud and accept the original firmware's AT syntax, including CRLF. Responses are forwarded to USART2 during startup. Successful command transmission does not prove that the module accepted a command; check its responses. HM-10-compatible modules vary in AT syntax and command-mode requirements.

The original name and passcode commands are retained. Set `ECG_CONFIGURE_HM10` to `0` to skip startup AT commands when the module is already configured or should remain connected.

## Standalone build

Install STM32CubeF1 and GNU Arm Embedded GCC, or use the packages already installed by PlatformIO/STM32CubeIDE. Python 3 is needed only for the build script; the firmware is C.

```powershell
python build.py --cube "C:\path\to\STM32Cube_FW_F1_V1.8.6" --gcc "C:\path\to\arm-none-eabi-gcc.exe"
```

The Cube directory must contain `Drivers/`. The script also recognizes `STM32CUBE_F1_DIR`; its default is the PlatformIO `framework-stm32cubef1` package under your home directory. Output goes to `build/`. ST's system/startup source files and required HAL drivers are compiled from that dependency.

For PlatformIO, open this folder as the project and build with the included `platformio.ini`. It selects `framework = stm32cube`. The standalone GNU build was verified; the PlatformIO configuration was not separately built.

## STM32CubeIDE integration

Create a separate C project for **STM32F103C8Tx**. Do not paste these files into a WBA52 project. The cleanest route is to generate an STM32CubeF1 project with ADC1 channel 7, USART1, USART2, TIM3, and their required HAL driver files enabled; then replace its matching application files with `Core/` from this package. Keep ST's generated `system_stm32f1xx.c`, the F103xB startup assembly, HAL/CMSIS drivers, and standard system-call support.

Do not retain duplicate GPIO/ADC/UART initialization functions, MSP callbacks, or interrupt handlers from a different project. If CubeMX generates peripheral source files separately, replace those initializers consistently rather than compiling two implementations. Use the configuration below when regenerating:

| Setting | Value |
|---|---|
| MCU | STM32F103C8T6, `STM32F103xB` preprocessor symbol |
| HSE | External 8 MHz crystal |
| SYSCLK / HCLK | 72 MHz from HSE × 9 |
| APB1 / APB2 | 36 MHz / 72 MHz |
| ADC clock | APB2 / 6 = 12 MHz |
| TIM3 | 72 MHz timer clock, prescaler 7199, period 39, update TRGO |
| ADC1 | Channel 7, single regular conversion, external TIM3 TRGO, 71.5-cycle sample time |
| ADC DMA | DMA1 channel 1, circular, halfword, increment memory, high priority |
| USART1 TX DMA | DMA1 channel 4, normal, byte, increment memory, low priority |
| NVIC | ADC DMA priority 1; UART DMA and USART1 priority 3; SysTick priority 15 |

Enable both the USART1 and DMA1 channel 4 interrupts: the UART transfer-complete interrupt is required for HAL to finish a DMA transmit. The implementation supplies both handlers. Enable `USE_HAL_DRIVER` and the HAL modules from the included configuration header.

The included binaries use an 8 MHz HSE crystal and a flash start of `0x08000000`, intended for ST-Link programming without an application bootloader offset. Verify your board's crystal, exact MCU, wiring, and bootloader arrangement before using them. No board was connected for on-hardware testing.

## Validation

The standalone target was compiled and linked using GNU Arm GCC 12.3.1 and the installed STM32CubeF1 package 1.8.6, with `-O2`. Application C files compile with `-Wall -Wextra -Werror`. Host tests cover 250,000 pseudorandom samples against a floating-point reference in both output scales (maximum difference one count), DC settling, negative sample encoding, every sequence/checksum, DMA block handling, queue overflow/order, sequence rollover, UART retry/timeout, ADC fault/rail accounting, and the one-shot LED sequence.

Reproduce the signal tests using a host C compiler:

```text
gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -ICore/Inc Core/Src/ecg_signal.c tests/test_signal.c -o tests/test_signal.exe
tests/test_signal.exe
python tests/test_pipeline.py
```

Repeat the first command with `-DECG_OUTPUT_ADC_BITS=12` for the full-resolution path. Peripheral pipeline tests use a mocked HAL; they do not measure analog quality, physical DMA timing, or HM-10 interoperability.

Clock, trigger, and DMA routing were checked against ST's [STM32F1 reference manual RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) and the installed STM32CubeF1 HAL sources.
