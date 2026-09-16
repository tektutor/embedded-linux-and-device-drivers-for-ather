/**
  * Lab 11: Counting Semaphore
  * ---------------------------
  * A counting semaphore manages a pool of 3 "connection slots".
  * Five client tasks each try to acquire a slot, hold it for
  * a few seconds (simulating work), then release it.
  *
  * With only 3 slots and 5 clients, you see clients waiting
  * in line for a slot to free up.
  *
  * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA
#define MAX_SLOTS    3
#define NUM_CLIENTS  5

UART_HandleTypeDef huart2;
SemaphoreHandle_t connectionPool;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
  * Client task: acquires a connection slot, does work, releases it.
  */
static void Client_Task(void *arg)
{
    uint32_t id = (uint32_t)arg;
    uint32_t jobs = 0;
    char buf[120];

    /* Stagger start times so clients don't all rush at once */
    vTaskDelay(pdMS_TO_TICKS(id * 200));

    for (;;)
    {
        jobs++;
        uint32_t avail = uxSemaphoreGetCount(connectionPool);
        snprintf(buf, sizeof(buf),
                 "[Client%lu] Job #%lu: requesting slot... (%lu/%d available)\r\n",
                 (unsigned long)id, (unsigned long)jobs,
                 (unsigned long)avail, MAX_SLOTS);
        uart_print(buf);

        uint32_t t0 = HAL_GetTick();
        xSemaphoreTake(connectionPool, portMAX_DELAY);
        uint32_t t1 = HAL_GetTick();

        avail = uxSemaphoreGetCount(connectionPool);
        snprintf(buf, sizeof(buf),
                 "[Client%lu] GOT slot after %lu ms wait. (%lu/%d remaining)\r\n",
                 (unsigned long)id, (unsigned long)(t1 - t0),
                 (unsigned long)avail, MAX_SLOTS);
        uart_print(buf);

        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

        /* Simulate connection work: 2-4 seconds depending on client */
        uint32_t workTime = 2000 + (id * 500);
        vTaskDelay(pdMS_TO_TICKS(workTime));

        xSemaphoreGive(connectionPool);

        snprintf(buf, sizeof(buf),
                 "[Client%lu] Released slot after %lu ms work.\r\n\r\n",
                 (unsigned long)id, (unsigned long)workTime);
        uart_print(buf);

        /* Brief pause before next job */
        vTaskDelay(pdMS_TO_TICKS(500));
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
    uart_print("  Lab 11: Counting Semaphore\r\n");
    uart_print("=============================================\r\n");

    char buf[80];
    snprintf(buf, sizeof(buf),
             "  %d connection slots, %d clients\r\n", MAX_SLOTS, NUM_CLIENTS);
    uart_print(buf);
    uart_print("  Clients wait when all slots are taken\r\n");
    uart_print("=============================================\r\n\r\n");

    /*
     * Create a counting semaphore.
     *   Arg 1: maximum count (3 slots)
     *   Arg 2: initial count (all 3 available)
     */
    connectionPool = xSemaphoreCreateCounting(MAX_SLOTS, MAX_SLOTS);

    for (uint32_t i = 1; i <= NUM_CLIENTS; i++)
    {
        char name[12];
        snprintf(name, sizeof(name), "Client%lu", (unsigned long)i);
        xTaskCreate(Client_Task, name, 512, (void *)i, 1, NULL);
    }

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
