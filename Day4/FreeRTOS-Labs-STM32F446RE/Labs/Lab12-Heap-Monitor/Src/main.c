/**
  * Lab 12: Heap and Stack Monitoring
  * -----------------------------------
  * Shows how to monitor FreeRTOS memory usage at runtime:
  *   - Total free heap (xPortGetFreeHeapSize)
  *   - Minimum ever free heap (xPortGetMinimumEverFreeHeapSize)
  *   - Per-task stack high water mark (uxTaskGetStackHighWaterMark)
  *
  * Three worker tasks with different stack usage patterns.
  * A monitor task prints a memory report every 3 seconds.
  *
  * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

UART_HandleTypeDef huart2;

TaskHandle_t hLightTask, hMediumTask, hHeavyTask, hMonitorTask;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
  * Light stack usage: small local variables.
  */
static void Light_Task(void *arg)
{
    uint32_t counter = 0;
    (void)arg;

    for (;;)
    {
        counter++;
        /* Just a small variable on stack */
        uint8_t flag = (counter % 2 == 0) ? 1 : 0;
        if (flag) HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
  * Medium stack usage: moderate local buffer.
  */
static void Medium_Task(void *arg)
{
    (void)arg;

    for (;;)
    {
        /* 64-byte buffer on stack */
        char buf[64];
        snprintf(buf, sizeof(buf), "Medium task running at %lu",
                 (unsigned long)HAL_GetTick());
        /* buf is used (printed), so the compiler does not optimize it away */
        uart_print("[MED   ] ");
        uart_print(buf);
        uart_print("\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
  * Heavy stack usage: large local arrays.
  * This task uses significant stack space on purpose.
  */
static void Heavy_Task(void *arg)
{
    (void)arg;

    for (;;)
    {
        /* 256-byte buffer on stack (uses significant stack) */
        char data[256];
        memset(data, 0, sizeof(data));
        snprintf(data, sizeof(data),
                 "Heavy task: tick=%lu, processing %d bytes of data",
                 (unsigned long)HAL_GetTick(), (int)sizeof(data));
        uart_print("[HEAVY ] ");
        uart_print(data);
        uart_print("\r\n");
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}

/**
  * Monitor task: prints heap and stack usage report.
  */
static void Monitor_Task(void *arg)
{
    char buf[120];
    (void)arg;

    uart_print("[MONITOR] Memory monitor started.\r\n\r\n");

    /* Print initial heap state */
    snprintf(buf, sizeof(buf),
             "[MONITOR] Initial free heap: %u bytes\r\n",
             (unsigned)xPortGetFreeHeapSize());
    uart_print(buf);
    snprintf(buf, sizeof(buf),
             "[MONITOR] Total heap (configTOTAL_HEAP_SIZE): %u bytes\r\n\r\n",
             (unsigned)configTOTAL_HEAP_SIZE);
    uart_print(buf);

    vTaskDelay(pdMS_TO_TICKS(2000));

    for (;;)
    {
        uint32_t freeHeap    = xPortGetFreeHeapSize();
        uint32_t minFreeHeap = xPortGetMinimumEverFreeHeapSize();
        uint32_t usedHeap    = configTOTAL_HEAP_SIZE - freeHeap;

        uart_print("============================================\r\n");
        uart_print("  MEMORY REPORT\r\n");
        uart_print("============================================\r\n");

        snprintf(buf, sizeof(buf),
                 "  Heap total:     %5u bytes\r\n",
                 (unsigned)configTOTAL_HEAP_SIZE);
        uart_print(buf);

        snprintf(buf, sizeof(buf),
                 "  Heap used:      %5lu bytes (%lu%%)\r\n",
                 (unsigned long)usedHeap,
                 (unsigned long)(usedHeap * 100 / configTOTAL_HEAP_SIZE));
        uart_print(buf);

        snprintf(buf, sizeof(buf),
                 "  Heap free:      %5lu bytes\r\n",
                 (unsigned long)freeHeap);
        uart_print(buf);

        snprintf(buf, sizeof(buf),
                 "  Heap min ever:  %5lu bytes (closest to full)\r\n",
                 (unsigned long)minFreeHeap);
        uart_print(buf);

        uart_print("--------------------------------------------\r\n");
        uart_print("  Task Stack High Water Marks (words free):\r\n");

        /*
         * uxTaskGetStackHighWaterMark returns the MINIMUM number
         * of free stack words since the task started.
         * Lower = closer to overflow. Zero = crashed or will crash.
         */
        UBaseType_t hwm;

        hwm = uxTaskGetStackHighWaterMark(hLightTask);
        snprintf(buf, sizeof(buf),
                 "    Light  (stack=256):  %3lu words free  %s\r\n",
                 (unsigned long)hwm,
                 hwm < 20 ? "*** WARNING ***" : "(OK)");
        uart_print(buf);

        hwm = uxTaskGetStackHighWaterMark(hMediumTask);
        snprintf(buf, sizeof(buf),
                 "    Medium (stack=256):  %3lu words free  %s\r\n",
                 (unsigned long)hwm,
                 hwm < 20 ? "*** WARNING ***" : "(OK)");
        uart_print(buf);

        hwm = uxTaskGetStackHighWaterMark(hHeavyTask);
        snprintf(buf, sizeof(buf),
                 "    Heavy  (stack=512):  %3lu words free  %s\r\n",
                 (unsigned long)hwm,
                 hwm < 20 ? "*** WARNING ***" : "(OK)");
        uart_print(buf);

        hwm = uxTaskGetStackHighWaterMark(hMonitorTask);
        snprintf(buf, sizeof(buf),
                 "    Monitor(stack=512):  %3lu words free  %s\r\n",
                 (unsigned long)hwm,
                 hwm < 20 ? "*** WARNING ***" : "(OK)");
        uart_print(buf);

        uart_print("============================================\r\n\r\n");

        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

static void GPIO_Init(void);
static void UART2_Init(void);
static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART2_Init();

    uart_print("\r\n=============================================\r\n");
    uart_print("  Lab 12: Heap and Stack Monitoring\r\n");
    uart_print("=============================================\r\n");
    uart_print("  3 workers with different stack usage\r\n");
    uart_print("  Monitor prints memory report every 3 sec\r\n");
    uart_print("=============================================\r\n\r\n");

    xTaskCreate(Light_Task,   "Light",   256, NULL, 1, &hLightTask);
    xTaskCreate(Medium_Task,  "Medium",  256, NULL, 1, &hMediumTask);
    xTaskCreate(Heavy_Task,   "Heavy",   512, NULL, 1, &hHeavyTask);
    xTaskCreate(Monitor_Task, "Monitor", 512, NULL, 2, &hMonitorTask);

    vTaskStartScheduler();
    for (;;);
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static void UART2_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE; osc.HSEState = RCC_HSE_BYPASS;
    osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while(1); }
#endif
