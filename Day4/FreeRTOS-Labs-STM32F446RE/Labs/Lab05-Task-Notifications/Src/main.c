/**
  * Lab 5: Task Notifications
  * ---------------------------
  * Two pairs of producer/consumer:
  *   Pair A: uses xTaskNotify / xTaskNotifyWait  (lightweight)
  *   Pair B: uses xQueueSend  / xQueueReceive    (standard)
  *
  * Both pairs exchange 1000 values. The tick count for each
  * approach is printed so you can compare performance.
  *
  * Then the notification pair runs continuously so you can see
  * the values flowing on UART.
  *
  * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA
#define BENCH_COUNT  1000

UART_HandleTypeDef huart2;

TaskHandle_t hNotifyConsumer;
TaskHandle_t hQueueProducer, hQueueConsumer;
QueueHandle_t benchQueue;

volatile uint32_t notifyDone = 0;
volatile uint32_t queueDone  = 0;
volatile uint32_t notifyTicks = 0;
volatile uint32_t queueTicks  = 0;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ================================================================
 * Pair A: Task Notifications
 * ================================================================ */

static void Notify_Producer(void *arg)
{
    char buf[100];
    uint32_t t0, t1;
    (void)arg;

    /* Wait for consumer to be ready */
    vTaskDelay(pdMS_TO_TICKS(100));

    uart_print("[NOTIFY] Starting benchmark: sending 1000 values...\r\n");
    t0 = HAL_GetTick();

    for (uint32_t i = 1; i <= BENCH_COUNT; i++)
    {
        xTaskNotify(hNotifyConsumer, i, eSetValueWithOverwrite);
        /* Tiny yield so consumer can process */
        taskYIELD();
    }

    t1 = HAL_GetTick();
    notifyTicks = t1 - t0;
    notifyDone = 1;

    snprintf(buf, sizeof(buf),
             "[NOTIFY] Benchmark done: %lu values in %lu ms\r\n\r\n",
             (unsigned long)BENCH_COUNT, (unsigned long)notifyTicks);
    uart_print(buf);

    /* Now run continuously for demonstration */
    uart_print("[NOTIFY] Continuous mode: sending event every second...\r\n\r\n");
    uint32_t event = 0;
    for (;;)
    {
        event++;
        xTaskNotify(hNotifyConsumer, event, eSetValueWithOverwrite);
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void Notify_Consumer(void *arg)
{
    uint32_t value;
    uint32_t received = 0;
    char buf[100];
    (void)arg;

    /* Benchmark phase: receive 1000 values silently */
    while (received < BENCH_COUNT)
    {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &value, pdMS_TO_TICKS(500)) == pdTRUE)
        {
            received++;
        }
    }

    /* Continuous phase: print each value */
    for (;;)
    {
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &value, portMAX_DELAY) == pdTRUE)
        {
            snprintf(buf, sizeof(buf),
                     "[NOTIFY] Received event #%lu at tick=%lu\r\n",
                     (unsigned long)value,
                     (unsigned long)HAL_GetTick());
            uart_print(buf);
        }
    }
}

/* ================================================================
 * Pair B: Queue (for comparison)
 * ================================================================ */

static void Queue_Producer(void *arg)
{
    uint32_t t0, t1;
    char buf[100];
    (void)arg;

    /* Wait for notify benchmark to finish */
    while (!notifyDone) vTaskDelay(pdMS_TO_TICKS(100));

    vTaskDelay(pdMS_TO_TICKS(500));
    uart_print("[QUEUE ] Starting benchmark: sending 1000 values...\r\n");
    t0 = HAL_GetTick();

    for (uint32_t i = 1; i <= BENCH_COUNT; i++)
    {
        xQueueSend(benchQueue, &i, portMAX_DELAY);
        taskYIELD();
    }

    t1 = HAL_GetTick();
    queueTicks = t1 - t0;
    queueDone = 1;

    snprintf(buf, sizeof(buf),
             "[QUEUE ] Benchmark done: %lu values in %lu ms\r\n\r\n",
             (unsigned long)BENCH_COUNT, (unsigned long)queueTicks);
    uart_print(buf);

    /* Print comparison */
    snprintf(buf, sizeof(buf),
             "=== RESULTS ===\r\n"
             "  Notification: %lu ms for %lu values\r\n",
             (unsigned long)notifyTicks, (unsigned long)BENCH_COUNT);
    uart_print(buf);
    snprintf(buf, sizeof(buf),
             "  Queue:        %lu ms for %lu values\r\n",
             (unsigned long)queueTicks, (unsigned long)BENCH_COUNT);
    uart_print(buf);

    if (queueTicks > 0 && notifyTicks > 0)
    {
        if (notifyTicks < queueTicks)
        {
            snprintf(buf, sizeof(buf),
                     "  Notifications were %lu%% faster.\r\n\r\n",
                     (unsigned long)((queueTicks - notifyTicks) * 100 / queueTicks));
        }
        else
        {
            uart_print("  Results too close to measure at 1 ms tick resolution.\r\n\r\n");
        }
    }
    uart_print("Notification producer continues running above...\r\n\r\n");

    /* This task is done */
    vTaskDelete(NULL);
}

static void Queue_Consumer(void *arg)
{
    uint32_t value;
    uint32_t received = 0;
    (void)arg;

    while (received < BENCH_COUNT)
    {
        if (xQueueReceive(benchQueue, &value, pdMS_TO_TICKS(500)) == pdPASS)
        {
            received++;
        }
    }

    vTaskDelete(NULL);
}

/* ---- Peripheral init ---- */
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
    uart_print("  Lab 5: Task Notifications vs Queue\r\n");
    uart_print("=============================================\r\n");
    uart_print("  Benchmark: 1000 values through each method\r\n");
    uart_print("  Then continuous notification demo\r\n");
    uart_print("=============================================\r\n\r\n");

    benchQueue = xQueueCreate(10, sizeof(uint32_t));

    /* Create notification pair */
    xTaskCreate(Notify_Consumer,  "NotCon", 512, NULL, 2, &hNotifyConsumer);
    xTaskCreate(Notify_Producer,  "NotPro", 512, NULL, 1, NULL);

    /* Create queue pair (starts after notify benchmark) */
    xTaskCreate(Queue_Consumer,   "QueCon", 512, NULL, 2, &hQueueConsumer);
    xTaskCreate(Queue_Producer,   "QuePro", 512, NULL, 1, &hQueueProducer);

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
