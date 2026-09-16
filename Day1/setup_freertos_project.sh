#!/bin/bash
# Setup script: FreeRTOS_ThreadCreation for NUCLEO-F446RE
# Run from your home directory: bash setup_freertos_project.sh

set -e

CUBE=~/STM32CubeF4
PROJ=~/freertos-nucleo-f446re

echo "=== Creating project directory ==="
mkdir -p "$PROJ/Src" "$PROJ/Inc" "$PROJ/Startup"

# ---------------------------------------------------------------
# main.c  (adapted from EVAL example, using PA5 for Nucleo LD2)
# ---------------------------------------------------------------
cat > "$PROJ/Src/main.c" << 'EOF'
/**
  * FreeRTOS_ThreadCreation for NUCLEO-F446RE
  * Adapted from STM32446E_EVAL example.
  *
  * Two threads toggle the onboard LED (LD2, PA5) at different rates,
  * suspending and resuming each other in a 15-second cycle.
  */
#include "main.h"
#include "cmsis_os.h"

#define LED_PIN        GPIO_PIN_5
#define LED_PORT       GPIOA
#define LED_CLK_EN()   __HAL_RCC_GPIOA_CLK_ENABLE()

osThreadId LEDThread1Handle, LEDThread2Handle;

static void LED_Thread1(void const *argument);
static void LED_Thread2(void const *argument);
static void SystemClock_Config(void);
static void LED_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    osThreadDef(T1, LED_Thread1, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
    osThreadDef(T2, LED_Thread2, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);

    LEDThread1Handle = osThreadCreate(osThread(T1), NULL);
    LEDThread2Handle = osThreadCreate(osThread(T2), NULL);

    osKernelStart();
    for (;;);
}

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    LED_CLK_EN();
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static void LED_Thread1(void const *argument)
{
    uint32_t count = 0;
    (void)argument;

    for (;;)
    {
        count = osKernelSysTick() + 5000;
        while (count >= osKernelSysTick())
        {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            osDelay(200);
        }
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        osThreadSuspend(NULL);

        count = osKernelSysTick() + 5000;
        while (count >= osKernelSysTick())
        {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            osDelay(400);
        }
        osThreadResume(LEDThread2Handle);
    }
}

static void LED_Thread2(void const *argument)
{
    uint32_t count;
    (void)argument;

    for (;;)
    {
        count = osKernelSysTick() + 10000;
        while (count >= osKernelSysTick())
        {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            osDelay(500);
        }
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        osThreadResume(LEDThread1Handle);
        osThreadSuspend(NULL);
    }
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clk = {0};
    RCC_OscInitTypeDef osc = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 360;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    osc.PLL.PLLR       = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                          RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while (1); }
#endif
EOF

# ---------------------------------------------------------------
# stm32f4xx_it.c  (interrupt handlers)
# ---------------------------------------------------------------
cat > "$PROJ/Src/stm32f4xx_it.c" << 'EOF'
#include "main.h"
#include "stm32f4xx_it.h"
#include "cmsis_os.h"

extern void xPortSysTickHandler(void);

void NMI_Handler(void) { }
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void DebugMon_Handler(void) { }

void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}
EOF

# ---------------------------------------------------------------
# main.h
# ---------------------------------------------------------------
cat > "$PROJ/Inc/main.h" << 'EOF'
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"

#endif /* __MAIN_H */
EOF

# ---------------------------------------------------------------
# stm32f4xx_it.h
# ---------------------------------------------------------------
cat > "$PROJ/Inc/stm32f4xx_it.h" << 'EOF'
#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);
void SysTick_Handler(void);

#endif
EOF

# ---------------------------------------------------------------
# FreeRTOSConfig.h
# ---------------------------------------------------------------
cat > "$PROJ/Inc/FreeRTOSConfig.h" << 'EOF'
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M4 specific: handler names for FreeRTOS port */
#define xPortPendSVHandler   PendSV_Handler
#define vPortSVCHandler      SVC_Handler

#define configUSE_PREEMPTION                     1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       ((uint32_t)180000000)
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     (7)
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
#define configTOTAL_HEAP_SIZE                    ((size_t)(15 * 1024))
#define configMAX_TASK_NAME_LEN                  (16)
#define configUSE_TRACE_FACILITY                 1
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_MUTEXES                        1
#define configQUEUE_REGISTRY_SIZE                8
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_MALLOC_FAILED_HOOK             1
#define configUSE_APPLICATION_TASK_TAG            0
#define configUSE_COUNTING_SEMAPHORES            1

/* Co-routine definitions (unused, but required) */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          (2)

/* Software timer definitions */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (2)
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             (configMINIMAL_STACK_SIZE * 2)

/* Optional functions */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskCleanUpResources            0
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1

/* Cortex-M specific interrupt priority configuration */
#ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS 4
#endif
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
#define configKERNEL_INTERRUPT_PRIORITY        (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY   (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Hook function stubs */
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

#endif /* FREERTOS_CONFIG_H */
EOF

# ---------------------------------------------------------------
# stm32f4xx_hal_conf.h  (copy from Nucleo Templates)
# ---------------------------------------------------------------
echo "=== Copying HAL conf from Nucleo Templates ==="
cp "$CUBE/Projects/STM32446E-Nucleo/Templates/Inc/stm32f4xx_hal_conf.h" "$PROJ/Inc/"

# ---------------------------------------------------------------
# Startup file and linker script from Nucleo Templates
# ---------------------------------------------------------------
echo "=== Copying startup file and linker script ==="

STARTUP=$(find "$CUBE/Projects/STM32446E-Nucleo/Templates/STM32CubeIDE/" -name "*.s" | head -1)
LINKER=$(find "$CUBE/Projects/STM32446E-Nucleo/Templates/STM32CubeIDE/" -name "*.ld" | head -1)

if [ -z "$STARTUP" ]; then
    # Fallback: use CMSIS startup
    STARTUP=$(find "$CUBE/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/" -name "startup_stm32f446xx.s")
fi

if [ -n "$STARTUP" ]; then
    cp "$STARTUP" "$PROJ/Startup/"
    echo "  Startup: $(basename $STARTUP)"
else
    echo "  WARNING: No startup file found!"
fi

if [ -n "$LINKER" ]; then
    cp "$LINKER" "$PROJ/"
    LINKER_NAME=$(basename "$LINKER")
    echo "  Linker:  $LINKER_NAME"
else
    echo "  WARNING: No linker script found!"
    LINKER_NAME="STM32F446RETX_FLASH.ld"
fi

# ---------------------------------------------------------------
# system_stm32f4xx.c  (from CMSIS)
# ---------------------------------------------------------------
SYSTEM_C=$(find "$CUBE/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/" -maxdepth 1 -name "system_stm32f4xx.c" | head -1)
if [ -n "$SYSTEM_C" ]; then
    cp "$SYSTEM_C" "$PROJ/Src/"
    echo "  System:  system_stm32f4xx.c"
fi

# ---------------------------------------------------------------
# Makefile
# ---------------------------------------------------------------
cat > "$PROJ/Makefile" << MAKEEOF
# ------------------------------------------------------------------
# Makefile for FreeRTOS_ThreadCreation on NUCLEO-F446RE
# ------------------------------------------------------------------
TARGET   = freertos_threadcreation
BUILD    = build

# Toolchain
CC       = arm-none-eabi-gcc
AS       = arm-none-eabi-gcc -x assembler-with-cpp
CP       = arm-none-eabi-objcopy
SZ       = arm-none-eabi-size

# STM32CubeF4 root
CUBE     = \$(HOME)/STM32CubeF4

# MCU flags
CPU      = -mcpu=cortex-m4
FPU      = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
MCU      = \$(CPU) -mthumb \$(FPU)

# C defines
DEFS     = -DSTM32F446xx -DUSE_HAL_DRIVER

# Include paths
INCS  = -IInc
INCS += -I\$(CUBE)/Drivers/CMSIS/Include
INCS += -I\$(CUBE)/Drivers/CMSIS/Device/ST/STM32F4xx/Include
INCS += -I\$(CUBE)/Drivers/STM32F4xx_HAL_Driver/Inc
INCS += -I\$(CUBE)/Middlewares/Third_Party/FreeRTOS/Source/include
INCS += -I\$(CUBE)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
INCS += -I\$(CUBE)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F

# C sources - application
C_SRC  = Src/main.c
C_SRC += Src/stm32f4xx_it.c
C_SRC += Src/system_stm32f4xx.c

# C sources - HAL drivers (add more as needed)
HAL = \$(CUBE)/Drivers/STM32F4xx_HAL_Driver/Src
C_SRC += \$(HAL)/stm32f4xx_hal.c
C_SRC += \$(HAL)/stm32f4xx_hal_cortex.c
C_SRC += \$(HAL)/stm32f4xx_hal_rcc.c
C_SRC += \$(HAL)/stm32f4xx_hal_rcc_ex.c
C_SRC += \$(HAL)/stm32f4xx_hal_pwr.c
C_SRC += \$(HAL)/stm32f4xx_hal_pwr_ex.c
C_SRC += \$(HAL)/stm32f4xx_hal_gpio.c
C_SRC += \$(HAL)/stm32f4xx_hal_dma.c

# C sources - FreeRTOS kernel
RTOS = \$(CUBE)/Middlewares/Third_Party/FreeRTOS/Source
C_SRC += \$(RTOS)/tasks.c
C_SRC += \$(RTOS)/queue.c
C_SRC += \$(RTOS)/list.c
C_SRC += \$(RTOS)/timers.c
C_SRC += \$(RTOS)/croutine.c
C_SRC += \$(RTOS)/event_groups.c
C_SRC += \$(RTOS)/portable/MemMang/heap_4.c
C_SRC += \$(RTOS)/portable/GCC/ARM_CM4F/port.c
C_SRC += \$(RTOS)/CMSIS_RTOS/cmsis_os.c

# Assembly sources
AS_SRC = \$(wildcard Startup/*.s)

# Compiler flags
CFLAGS  = \$(MCU) \$(DEFS) \$(INCS) -Wall -fdata-sections -ffunction-sections -Os -std=gnu11
ASFLAGS = \$(MCU) -Wall -fdata-sections -ffunction-sections

# Linker
LDSCRIPT = ${LINKER_NAME}
LDFLAGS  = \$(MCU) -specs=nano.specs -T\$(LDSCRIPT) -Wl,--gc-sections -lc -lm -lnosys

# Object files
OBJECTS  = \$(addprefix \$(BUILD)/,\$(notdir \$(C_SRC:.c=.o)))
OBJECTS += \$(addprefix \$(BUILD)/,\$(notdir \$(AS_SRC:.s=.o)))
vpath %.c \$(sort \$(dir \$(C_SRC)))
vpath %.s \$(sort \$(dir \$(AS_SRC)))

# ----- Rules -----
all: \$(BUILD)/\$(TARGET).elf \$(BUILD)/\$(TARGET).bin
	\$(SZ) \$(BUILD)/\$(TARGET).elf

\$(BUILD)/%.o: %.c | \$(BUILD)
	\$(CC) \$(CFLAGS) -c \$< -o \$@

\$(BUILD)/%.o: %.s | \$(BUILD)
	\$(AS) \$(ASFLAGS) -c \$< -o \$@

\$(BUILD)/\$(TARGET).elf: \$(OBJECTS)
	\$(CC) \$(OBJECTS) \$(LDFLAGS) -o \$@

\$(BUILD)/\$(TARGET).bin: \$(BUILD)/\$(TARGET).elf
	\$(CP) -O binary \$< \$@

\$(BUILD):
	mkdir -p \$@

flash: \$(BUILD)/\$(TARGET).bin
	st-flash write \$< 0x08000000

flash-ocd: \$(BUILD)/\$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \\
	  -c "program \$< verify reset exit"

clean:
	rm -rf \$(BUILD)

.PHONY: all clean flash flash-ocd
MAKEEOF

# ---------------------------------------------------------------
# Hook stubs (required by FreeRTOS config)
# ---------------------------------------------------------------
cat > "$PROJ/Src/freertos_hooks.c" << 'EOF'
#include "FreeRTOS.h"
#include "task.h"

void vApplicationMallocFailedHook(void)   { taskDISABLE_INTERRUPTS(); for(;;); }
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
                                          { (void)xTask; (void)pcTaskName; for(;;); }
EOF

# Add hooks source to Makefile
sed -i 's|C_SRC += Src/system_stm32f4xx.c|C_SRC += Src/system_stm32f4xx.c\nC_SRC += Src/freertos_hooks.c|' "$PROJ/Makefile"

echo ""
echo "=== Project created at: $PROJ ==="
echo ""
echo "Next steps:"
echo "  cd ~/freertos-nucleo-f446re"
echo "  make"
echo "  make flash        # using st-flash"
echo "  make flash-ocd    # using openocd"
echo ""

# Show the directory tree
echo "=== Project structure ==="
find "$PROJ" -type f | sort | sed "s|$PROJ/||"
