/**
  * Lab 6: Deadlock
  * ----------------
  * Two tasks, two mutexes, opposite locking order = deadlock.
  *
  * #define CONSISTENT_ORDER 0  -> deadlock (system freezes)
  * #define CONSISTENT_ORDER 1  -> fixed (same lock order, runs forever)
  *
  * When deadlock occurs, UART output stops and LED freezes.
  * That silence IS the result.
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
 * CHANGE THIS TO SWITCH BETWEEN DEADLOCK AND FIXED
 *   0 = opposite lock order (DEADLOCK)
 *   1 = same lock order     (FIXED)
 * ================================================================ */
#define CONSISTENT_ORDER  0

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

UART_HandleTypeDef huart2;
SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
  * Task 1: locks A then B (always)
  */
static void Task1(void *arg)
{
    uint32_t count = 0;
    char buf[100];
    (void)arg;

    for (;;)
    {
        count++;
        snprintf(buf, sizeof(buf),
                 "[T1] Cycle %lu: taking Mutex A...\r\n",
                 (unsigned long)count);
        uart_print(buf);

        xSemaphoreTake(mutexA, portMAX_DELAY);
        uart_print("[T1] Got Mutex A.\r\n");

        /* Small delay to increase chance of interleaving */
        vTaskDelay(pdMS_TO_TICKS(10));

        uart_print("[T1] Taking Mutex B...\r\n");
        xSemaphoreTake(mutexB, portMAX_DELAY);
        uart_print("[T1] Got Mutex B.\r\n");

        /* Critical section: use both resources */
        snprintf(buf, sizeof(buf),
                 "[T1] *** Working with both resources (cycle %lu) ***\r\n",
                 (unsigned long)count);
        uart_print(buf);
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

        xSemaphoreGive(mutexB);
        xSemaphoreGive(mutexA);
        uart_print("[T1] Released both mutexes.\r\n\r\n");

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
  * Task 2:
  *   CONSISTENT_ORDER = 0 -> locks B then A (OPPOSITE order = deadlock)
  *   CONSISTENT_ORDER = 1 -> locks A then B (SAME order = safe)
  */
static void Task2(void *arg)
{
    uint32_t count = 0;
    char buf[100];
    (void)arg;

    for (;;)
    {
        count++;

#if CONSISTENT_ORDER
        /* FIXED: same order as Task 1 (A first, then B) */
        snprintf(buf, sizeof(buf),
                 "[T2] Cycle %lu: taking Mutex A... (same order as T1)\r\n",
                 (unsigned long)count);
        uart_print(buf);

        xSemaphoreTake(mutexA, portMAX_DELAY);
        uart_print("[T2] Got Mutex A.\r\n");

        vTaskDelay(pdMS_TO_TICKS(10));

        uart_print("[T2] Taking Mutex B...\r\n");
        xSemaphoreTake(mutexB, portMAX_DELAY);
        uart_print("[T2] Got Mutex B.\r\n");
#else
        /* BROKEN: opposite order (B first, then A) */
        snprintf(buf, sizeof(buf),
                 "[T2] Cycle %lu: taking Mutex B... (OPPOSITE order!)\r\n",
                 (unsigned long)count);
        uart_print(buf);

        xSemaphoreTake(mutexB, portMAX_DELAY);
        uart_print("[T2] Got Mutex B.\r\n");

        vTaskDelay(pdMS_TO_TICKS(10));

        uart_print("[T2] Taking Mutex A...\r\n");
        xSemaphoreTake(mutexA, portMAX_DELAY);
        uart_print("[T2] Got Mutex A.\r\n");
#endif

        snprintf(buf, sizeof(buf),
                 "[T2] *** Working with both resources (cycle %lu) ***\r\n",
                 (unsigned long)count);
        uart_print(buf);

        xSemaphoreGive(mutexB);
        xSemaphoreGive(mutexA);
        uart_print("[T2] Released both mutexes.\r\n\r\n");

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ---- Heartbeat task: proves the system is alive ---- */
static void Heartbeat_Task(void *arg)
{
    (void)arg;
    for (;;)
    {
        uart_print("[BEAT] System alive.\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
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
    uart_print("  Lab 6: Deadlock\r\n");
#if CONSISTENT_ORDER
    uart_print("  Mode: CONSISTENT ORDER (no deadlock)\r\n");
#else
    uart_print("  Mode: OPPOSITE ORDER (deadlock will occur!)\r\n");
#endif
    uart_print("=============================================\r\n");
    uart_print("  T1: locks A then B\r\n");
#if CONSISTENT_ORDER
    uart_print("  T2: locks A then B (same order = safe)\r\n");
#else
    uart_print("  T2: locks B then A (opposite = DEADLOCK)\r\n");
#endif
    uart_print("  Heartbeat prints every 2 s (stops on deadlock)\r\n");
    uart_print("=============================================\r\n\r\n");

    mutexA = xSemaphoreCreateMutex();
    mutexB = xSemaphoreCreateMutex();

    xTaskCreate(Task1,          "T1",    512, NULL, 1, NULL);
    xTaskCreate(Task2,          "T2",    512, NULL, 1, NULL);
    xTaskCreate(Heartbeat_Task, "Beat",  256, NULL, 1, NULL);

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
