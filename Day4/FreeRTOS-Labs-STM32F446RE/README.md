# FreeRTOS Labs for STM32 Nucleo-F446RE

## 12 Hands-On Labs with Complete Source Code

This package is fully self-contained. No external dependencies, no setup scripts.
Install the ARM toolchain and start building.

## Prerequisites

```bash
sudo apt install gcc-arm-none-eabi stlink-tools minicom
```

## Directory Structure

```
FreeRTOS-Labs-STM32F446RE/
├── README.md                   ← you are here
├── Drivers/                    ← STM32 HAL and CMSIS (shared)
├── Middlewares/                ← FreeRTOS kernel (shared)
├── Shared/                    ← common headers and configs
├── Startup/                   ← startup assembly file
├── STM32F446RETX_FLASH.ld     ← linker script
└── Labs/
    ├── Lab01-Mutex/
    │   ├── README.md          ← theory + code walkthrough + expected output
    │   ├── Src/main.c         ← lab source code
    │   └── Makefile           ← build rules
    ├── Lab02-Software-Timers/
    ├── Lab03-Semaphore-ISR/
    ├── Lab04-Queue-Structs/
    ├── Lab05-Task-Notifications/
    ├── Lab06-Deadlock/
    ├── Lab07-Watchdog/
    ├── Lab08-Queue-Scheduling/
    ├── Lab09-Priority-Inversion/
    ├── Lab10-Event-Groups/
    ├── Lab11-Counting-Semaphore/
    └── Lab12-Heap-Monitor/
```

## Building Any Lab

```bash
cd Labs/Lab01-Mutex      # or any lab folder
make clean
make
make flash               # flash to board via ST-Link
```

## Serial Monitor

```bash
minicom -D /dev/ttyACM0 -b 115200
```

First time setup in minicom:
1. Press Ctrl+A, then O
2. Select Serial port setup
3. Press F to toggle Hardware Flow Control to No
4. Select Save setup as dfl
5. Press Escape

## Reset the Board

After flashing, open minicom first, then reset to catch output from the start:

- **Physical**: press the black RESET button on the Nucleo board
- **Command**: `st-flash reset` or `make reset`

## Lab Summary

| Lab | Concept | Toggle / Action |
|-----|---------|-----------------|
| 01 | Mutex | `USE_MUTEX` 0/1 |
| 02 | Software Timers | Watch LED + UART |
| 03 | Semaphore from ISR | Press blue button |
| 04 | Queue of Structs | Watch 3 sensors |
| 05 | Task Notifications | Auto benchmark |
| 06 | Deadlock | `CONSISTENT_ORDER` 0/1 |
| 07 | Watchdog | Watch at ~10 sec |
| 08 | Queue + Scheduling | Watch dot interruptions |
| 09 | Priority Inversion | `USE_MUTEX` 0/1 |
| 10 | Event Groups | Watch 3 subsystem init |
| 11 | Counting Semaphore | Watch 5 clients, 3 slots |
| 12 | Heap Monitor | Read memory reports |

Labs 01, 06, and 09 have a `#define` toggle. Build and flash twice (broken then fixed)
to see both behaviors.

## Hardware

- **Board**: STM32 Nucleo-F446RE
- **MCU**: STM32F446RET6 (Cortex-M4, 180 MHz, 512 KB Flash, 128 KB RAM)
- **LED**: LD2 on PA5 (green, active high)
- **Button**: Blue user button on PC13 (active low, external pull-up)
- **UART**: USART2 on PA2 (TX) / PA3 (RX), routed to ST-Link virtual COM port
- **Clock**: 8 MHz HSE bypass from ST-Link MCO output, PLL to 180 MHz

## FreeRTOS Configuration

Key settings in `Shared/Inc/FreeRTOSConfig.h`:

| Setting | Value | Meaning |
|---------|-------|---------|
| configCPU_CLOCK_HZ | 180 MHz | System clock speed |
| configTICK_RATE_HZ | 1000 | 1 ms tick period |
| configTOTAL_HEAP_SIZE | 15 KB | RAM for kernel objects |
| configMINIMAL_STACK_SIZE | 128 words | 512 bytes minimum stack |
| configMAX_PRIORITIES | 7 | Priority levels 0-6 |
| configUSE_MUTEXES | 1 | Mutex support enabled |
| configUSE_COUNTING_SEMAPHORES | 1 | Counting semaphore support |
| configUSE_TIMERS | 1 | Software timer support |
| configCHECK_FOR_STACK_OVERFLOW | 2 | Stack overflow detection |

## License

STM32 HAL drivers and FreeRTOS source: see LICENSE files in their respective directories.
Lab source code and documentation: freely usable for training and education.
