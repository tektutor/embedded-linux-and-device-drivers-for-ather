# ------------------------------------------------------------------
# Lab 06: Deadlock
# STM32 Nucleo-F446RE + FreeRTOS
# ------------------------------------------------------------------
TARGET   = lab06_deadlock
BUILD    = build

CC       = arm-none-eabi-gcc
AS       = arm-none-eabi-gcc -x assembler-with-cpp
CP       = arm-none-eabi-objcopy
SZ       = arm-none-eabi-size

ROOT     = ../..

CPU      = -mcpu=cortex-m4
FPU      = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
MCU      = $(CPU) -mthumb $(FPU)

DEFS     = -DSTM32F446xx -DUSE_HAL_DRIVER

INCS  = -I$(ROOT)/Shared/Inc
INCS += -I$(ROOT)/Drivers/CMSIS/Include
INCS += -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32F4xx/Include
INCS += -I$(ROOT)/Drivers/STM32F4xx_HAL_Driver/Inc
INCS += -I$(ROOT)/Middlewares/FreeRTOS/Source/include
INCS += -I$(ROOT)/Middlewares/FreeRTOS/Source/CMSIS_RTOS
INCS += -I$(ROOT)/Middlewares/FreeRTOS/Source/portable/GCC/ARM_CM4F

# Application sources
C_SRC  = Src/main.c
C_SRC += $(ROOT)/Shared/Src/stm32f4xx_it.c
C_SRC += $(ROOT)/Shared/Src/system_stm32f4xx.c
C_SRC += $(ROOT)/Shared/Src/freertos_hooks.c

# HAL drivers
HAL = $(ROOT)/Drivers/STM32F4xx_HAL_Driver/Src
C_SRC += $(HAL)/stm32f4xx_hal.c
C_SRC += $(HAL)/stm32f4xx_hal_cortex.c
C_SRC += $(HAL)/stm32f4xx_hal_rcc.c
C_SRC += $(HAL)/stm32f4xx_hal_rcc_ex.c
C_SRC += $(HAL)/stm32f4xx_hal_pwr.c
C_SRC += $(HAL)/stm32f4xx_hal_pwr_ex.c
C_SRC += $(HAL)/stm32f4xx_hal_gpio.c
C_SRC += $(HAL)/stm32f4xx_hal_dma.c
C_SRC += $(HAL)/stm32f4xx_hal_uart.c

# FreeRTOS kernel
RTOS = $(ROOT)/Middlewares/FreeRTOS/Source
C_SRC += $(RTOS)/tasks.c
C_SRC += $(RTOS)/queue.c
C_SRC += $(RTOS)/list.c
C_SRC += $(RTOS)/timers.c
C_SRC += $(RTOS)/croutine.c
C_SRC += $(RTOS)/event_groups.c
C_SRC += $(RTOS)/portable/MemMang/heap_4.c
C_SRC += $(RTOS)/portable/GCC/ARM_CM4F/port.c
C_SRC += $(RTOS)/CMSIS_RTOS/cmsis_os.c

AS_SRC = $(ROOT)/Startup/startup_stm32f446retx.s

CFLAGS  = $(MCU) $(DEFS) $(INCS) -Wall -fdata-sections -ffunction-sections -Os -std=gnu11
ASFLAGS = $(MCU) -Wall -fdata-sections -ffunction-sections

LDSCRIPT = $(ROOT)/STM32F446RETX_FLASH.ld
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) -Wl,--gc-sections -lc -lm -lnosys

OBJECTS  = $(addprefix $(BUILD)/,$(notdir $(C_SRC:.c=.o)))
OBJECTS += $(addprefix $(BUILD)/,$(notdir $(AS_SRC:.s=.o)))
vpath %.c $(sort $(dir $(C_SRC)))
vpath %.s $(sort $(dir $(AS_SRC)))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin
	$(SZ) $(BUILD)/$(TARGET).elf

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(CP) -O binary $< $@

$(BUILD):
	mkdir -p $@

flash: $(BUILD)/$(TARGET).bin
	st-flash write $< 0x08000000

flash-ocd: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	  -c "program $< verify reset exit"

reset:
	st-flash reset

clean:
	rm -rf $(BUILD)

.PHONY: all clean flash flash-ocd reset
