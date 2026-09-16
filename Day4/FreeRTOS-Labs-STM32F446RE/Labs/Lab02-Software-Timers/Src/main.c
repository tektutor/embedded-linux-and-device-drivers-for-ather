/**
  * Lab 2: Software Timers
  * -----------------------
  * Three software timers, zero dedicated tasks:
  *
  *   1. Periodic LED timer  - toggles LD2 every 500 ms
  *   2. Periodic print timer - prints a counter every 2 seconds
  *   3. One-shot timer       - fires once, 5 seconds after boot
  *
  * All callbacks run inside the FreeRTOS timer daemon task.
  *
  * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdio.h>
#include <string.h>

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

UART_HandleTypeDef huart2;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ---- Timer callbacks ---- */

/**
  * Called every 500 ms. Toggles the LED.
  * This is a periodic timer (auto-reload = pdTRUE).
  */
static void LED_TimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

/**
  * Called every 2000 ms. Prints a counter.
  * This is a periodic timer.
  */
static uint32_t printCounter = 0;
static void Print_TimerCallback(TimerHandle_t xTimer)
{
    char buf[80];
    (void)xTimer;
    printCounter++;
    snprintf(buf, sizeof(buf),
             "[PERIODIC] Tick #%lu at %lu ms\r\n",
             (unsigned long)printCounter,
             (unsigned long)HAL_GetTick());
    uart_print(buf);
}

/**
  * Fires once, 5 seconds after boot.
  * This is a one-shot timer (auto-reload = pdFALSE).
  * After firing, it starts a new one-shot timer to demonstrate chaining.
  */
static TimerHandle_t oneShotTimer;
static uint32_t oneShotCount = 0;
static void OneShot_TimerCallback(TimerHandle_t xTimer)
{
    char buf[120];
    oneShotCount++;

    snprintf(buf, sizeof(buf),
             "\r\n[ONE-SHOT] *** Timer fired! (count=%lu, tick=%lu) ***\r\n",
             (unsigned long)oneShotCount,
             (unsigned long)HAL_GetTick());
    uart_print(buf);

    if (oneShotCount < 3)
    {
        /* Restart the one-shot with a different period each time */
        uint32_t nextDelay = 3000 + (oneShotCount * 2000);
        snprintf(buf, sizeof(buf),
                 "[ONE-SHOT] Next one-shot in %lu ms...\r\n\r\n",
                 (unsigned long)nextDelay);
        uart_print(buf);
        xTimerChangePeriod(xTimer, pdMS_TO_TICKS(nextDelay), 0);
        /* ChangePeriod on a one-shot timer restarts it */
    }
    else
    {
        uart_print("[ONE-SHOT] No more one-shot timers. Periodic timers continue.\r\n\r\n");
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
    uart_print("  Lab 2: Software Timers\r\n");
    uart_print("=============================================\r\n");
    uart_print("  Timer 1: LED toggle every 500 ms (periodic)\r\n");
    uart_print("  Timer 2: Print counter every 2 s (periodic)\r\n");
    uart_print("  Timer 3: One-shot fires at 5 s after boot\r\n");
    uart_print("=============================================\r\n\r\n");

    /* Periodic timer: LED blink, 500 ms */
    TimerHandle_t ledTimer = xTimerCreate(
        "LED",                      /* name (debug only)        */
        pdMS_TO_TICKS(500),         /* period                   */
        pdTRUE,                     /* auto-reload = periodic   */
        NULL,                       /* timer ID (unused)        */
        LED_TimerCallback           /* callback function        */
    );

    /* Periodic timer: print, 2000 ms */
    TimerHandle_t printTimer = xTimerCreate(
        "Print",
        pdMS_TO_TICKS(2000),
        pdTRUE,                     /* periodic */
        NULL,
        Print_TimerCallback
    );

    /* One-shot timer: fires once at 5000 ms */
    oneShotTimer = xTimerCreate(
        "OneShot",
        pdMS_TO_TICKS(5000),
        pdFALSE,                    /* one-shot */
        NULL,
        OneShot_TimerCallback
    );

    /* Start all timers */
    xTimerStart(ledTimer, 0);
    xTimerStart(printTimer, 0);
    xTimerStart(oneShotTimer, 0);

    uart_print("[MAIN] All timers started. No tasks created.\r\n");
    uart_print("[MAIN] Everything runs in the timer daemon task.\r\n\r\n");

    vTaskStartScheduler();
    for (;;);
}

/* ---- Standard peripheral init ---- */

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
