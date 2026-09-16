/**
  * Lab 1: Mutex (Mutual Exclusion)
  * --------------------------------
  * Two tasks write multi-line messages to UART simultaneously.
  *
  * #define USE_MUTEX 0  -> output is garbled (no protection)
  * #define USE_MUTEX 1  -> output is clean (mutex protects UART)
  *
  * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * CHANGE THIS TO SEE THE DIFFERENCE
 *   0 = no mutex (garbled output)
 *   1 = mutex protects UART (clean output)
 * ================================================================ */
#define USE_MUTEX  1

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

UART_HandleTypeDef huart2;
SemaphoreHandle_t uartMutex;

static void Task_Sensor(void *arg);
static void Task_Status(void *arg);
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void UART2_Init(void);

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* Protected print: takes mutex, prints multiple lines, gives mutex */
static void uart_print_block(const char *lines[], int count)
{
#if USE_MUTEX
    xSemaphoreTake(uartMutex, portMAX_DELAY);
#endif

    for (int i = 0; i < count; i++)
    {
        uart_print(lines[i]);
        /* Small delay between lines to make interleaving visible
           when mutex is OFF */
        for (volatile int j = 0; j < 50000; j++);
    }

#if USE_MUTEX
    xSemaphoreGive(uartMutex);
#endif
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART2_Init();

    uart_print("\r\n=============================================\r\n");
    uart_print("  Lab 1: Mutex\r\n");
#if USE_MUTEX
    uart_print("  Mode: MUTEX ON (output should be clean)\r\n");
#else
    uart_print("  Mode: MUTEX OFF (output will be garbled)\r\n");
#endif
    uart_print("=============================================\r\n\r\n");

    uartMutex = xSemaphoreCreateMutex();

    xTaskCreate(Task_Sensor, "Sensor", 512, NULL, 1, NULL);
    xTaskCreate(Task_Status, "Status", 512, NULL, 1, NULL);

    vTaskStartScheduler();
    for (;;);
}

/**
  * Task_Sensor: prints a 3-line sensor report every 500 ms
  */
static void Task_Sensor(void *arg)
{
    uint32_t count = 0;
    char line1[60], line2[60], line3[60];
    (void)arg;

    for (;;)
    {
        count++;
        snprintf(line1, sizeof(line1),
                 "[SENSOR] --- Reading #%lu ---\r\n", (unsigned long)count);
        snprintf(line2, sizeof(line2),
                 "[SENSOR] Temperature: %lu.%lu C\r\n",
                 (unsigned long)(20 + count % 10),
                 (unsigned long)(count % 10));
        snprintf(line3, sizeof(line3),
                 "[SENSOR] Humidity:    %lu%%\r\n\r\n",
                 (unsigned long)(40 + count % 30));

        const char *lines[] = { line1, line2, line3 };
        uart_print_block(lines, 3);

        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
  * Task_Status: prints a 3-line status report every 500 ms
  */
static void Task_Status(void *arg)
{
    uint32_t count = 0;
    char line1[60], line2[60], line3[60];
    (void)arg;

    for (;;)
    {
        count++;
        snprintf(line1, sizeof(line1),
                 "[STATUS] === System Status #%lu ===\r\n",
                 (unsigned long)count);
        snprintf(line2, sizeof(line2),
                 "[STATUS] Uptime: %lu seconds\r\n",
                 (unsigned long)(HAL_GetTick() / 1000));
        snprintf(line3, sizeof(line3),
                 "[STATUS] Free heap: %u bytes\r\n\r\n",
                 (unsigned)xPortGetFreeHeapSize());

        const char *lines[] = { line1, line2, line3 };
        uart_print_block(lines, 3);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ---- Peripheral init (same as previous labs) ---- */

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
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
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
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_BYPASS;
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
