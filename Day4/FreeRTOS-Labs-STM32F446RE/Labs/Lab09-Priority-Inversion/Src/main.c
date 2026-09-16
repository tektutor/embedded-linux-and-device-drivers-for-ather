/**
  * Lab: Priority Inversion
  * ------------------------
  * NUCLEO-F446RE + FreeRTOS
  *
  * Three tasks:
  *   Task_H  (HIGH   priority) - wants the shared lock
  *   Task_M  (MEDIUM priority) - CPU-bound work, no lock needed
  *   Task_L  (LOW    priority) - holds the shared lock, does slow work
  *
  * BUILD MODE:
  *   #define USE_MUTEX  0   -> binary semaphore, NO priority inheritance
  *                            (reproduces priority inversion)
  *   #define USE_MUTEX  1   -> mutex WITH priority inheritance
  *                            (fixes the inversion)
  *
  * Build both versions and compare the UART output.
  *
  * UART2 at 115200 baud on /dev/ttyACM0
  *   minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * CHANGE THIS TO SWITCH BETWEEN BROKEN AND FIXED BEHAVIOR
 *   0 = binary semaphore (priority inversion happens)
 *   1 = mutex with priority inheritance (inversion fixed)
 * ================================================================ */
#define USE_MUTEX  0

/* ---- LED: PA5 (LD2) ---- */
#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

/* ---- UART ---- */
UART_HandleTypeDef huart2;

/* ---- Shared lock ---- */
SemaphoreHandle_t sharedLock;

/* ---- Task handles ---- */
TaskHandle_t hTaskH, hTaskM, hTaskL;

/* ---- Prototypes ---- */
static void Task_High(void *arg);
static void Task_Medium(void *arg);
static void Task_Low(void *arg);
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void UART2_Init(void);
static void uart_print(const char *msg);
static void busy_wait_ms(uint32_t ms);

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART2_Init();

    uart_print("\r\n=============================================\r\n");
    uart_print("  FreeRTOS Priority Inversion Lab\r\n");
    uart_print("  Nucleo-F446RE\r\n");
#if USE_MUTEX
    uart_print("  Mode: MUTEX (priority inheritance ON)\r\n");
#else
    uart_print("  Mode: BINARY SEMAPHORE (NO inheritance)\r\n");
#endif
    uart_print("=============================================\r\n");
    uart_print("  Task_H = HIGH   priority (wants lock)\r\n");
    uart_print("  Task_M = MEDIUM priority (no lock, CPU hog)\r\n");
    uart_print("  Task_L = LOW    priority (holds lock)\r\n");
    uart_print("=============================================\r\n\r\n");

    /* Create the shared lock */
#if USE_MUTEX
    sharedLock = xSemaphoreCreateMutex();
    uart_print("[INIT] Created MUTEX (priority inheritance enabled)\r\n\r\n");
#else
    sharedLock = xSemaphoreCreateBinary();
    xSemaphoreGive(sharedLock);  /* binary semaphore starts empty, must give first */
    uart_print("[INIT] Created BINARY SEMAPHORE (no inheritance)\r\n\r\n");
#endif

    /*
     * Task_L starts first and grabs the lock.
     * Task_M and Task_H start after short delays so the
     * inversion scenario plays out in the right order.
     */
    xTaskCreate(Task_Low,    "Low",    512, NULL, 1, &hTaskL);   /* lowest  */
    xTaskCreate(Task_Medium, "Medium", 512, NULL, 2, &hTaskM);   /* medium  */
    xTaskCreate(Task_High,   "High",   512, NULL, 3, &hTaskH);   /* highest */

    vTaskStartScheduler();

    for (;;);
}

/* ================================================================
 * Task_Low (priority 1)
 *
 * Takes the shared lock, then does slow busy work for ~4 seconds
 * while holding it. In the broken case, Task_M preempts this
 * task, delaying lock release and starving Task_H.
 * ================================================================ */
static void Task_Low(void *arg)
{
    char buf[120];
    (void)arg;

    for (;;)
    {
        uart_print("[Low ] Trying to take lock...\r\n");
        xSemaphoreTake(sharedLock, portMAX_DELAY);

        uint32_t t0 = HAL_GetTick();
        snprintf(buf, sizeof(buf),
                 "[Low ] GOT LOCK at tick=%lu. Doing slow work (4 seconds)...\r\n",
                 (unsigned long)t0);
        uart_print(buf);

        /*
         * Simulate slow critical section work.
         * Print a status dot each second so you can see
         * when this task actually runs vs when it's preempted.
         */
        for (int i = 0; i < 4; i++)
        {
            busy_wait_ms(1000);
            snprintf(buf, sizeof(buf),
                     "[Low ] ...working (%d/4 sec done, tick=%lu)\r\n",
                     i + 1, (unsigned long)HAL_GetTick());
            uart_print(buf);
        }

        uint32_t t1 = HAL_GetTick();
        snprintf(buf, sizeof(buf),
                 "[Low ] RELEASING lock at tick=%lu (held %lu ms)\r\n\r\n",
                 (unsigned long)t1, (unsigned long)(t1 - t0));
        uart_print(buf);

        xSemaphoreGive(sharedLock);

        /* Wait before repeating the cycle */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ================================================================
 * Task_Medium (priority 2)
 *
 * Starts 500 ms after boot. Does NOT use the lock at all.
 * Burns CPU for ~5 seconds with busy work.
 *
 * In the broken case, this task preempts Task_Low because it
 * has higher priority, preventing Low from releasing the lock,
 * which in turn blocks Task_High. This IS the priority inversion.
 *
 * In the fixed case, Task_Low gets boosted to Task_High's
 * priority when High blocks on the mutex, so Medium CANNOT
 * preempt Low.
 * ================================================================ */
static void Task_Medium(void *arg)
{
    char buf[120];
    (void)arg;

    /* Let Task_Low grab the lock first */
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;)
    {
        uint32_t t0 = HAL_GetTick();
        snprintf(buf, sizeof(buf),
                 "[Med ] STARTING CPU-bound work at tick=%lu (5 seconds, no lock)\r\n",
                 (unsigned long)t0);
        uart_print(buf);

        for (int i = 0; i < 5; i++)
        {
            busy_wait_ms(1000);
            snprintf(buf, sizeof(buf),
                     "[Med ] ...running (%d/5 sec, tick=%lu)\r\n",
                     i + 1, (unsigned long)HAL_GetTick());
            uart_print(buf);
        }

        uint32_t t1 = HAL_GetTick();
        snprintf(buf, sizeof(buf),
                 "[Med ] DONE at tick=%lu (ran %lu ms)\r\n\r\n",
                 (unsigned long)t1, (unsigned long)(t1 - t0));
        uart_print(buf);

        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ================================================================
 * Task_High (priority 3)
 *
 * Starts 1000 ms after boot, when Task_Low already holds the lock
 * and Task_Medium is busy hogging the CPU.
 *
 * Tries to take the lock and measures how long it waits.
 *
 * WITHOUT inheritance: waits for Medium to finish + Low to finish
 * WITH    inheritance: Low gets boosted, preempts Medium, finishes fast
 * ================================================================ */
static void Task_High(void *arg)
{
    char buf[120];
    (void)arg;

    /* Let Task_Low grab the lock and Task_Medium start running */
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;)
    {
        uint32_t t0 = HAL_GetTick();
        snprintf(buf, sizeof(buf),
                 "[HIGH] REQUESTING lock at tick=%lu...\r\n",
                 (unsigned long)t0);
        uart_print(buf);

        /* This blocks until Task_Low releases the lock */
        xSemaphoreTake(sharedLock, portMAX_DELAY);

        uint32_t t1 = HAL_GetTick();
        snprintf(buf, sizeof(buf),
                 "[HIGH] *** GOT LOCK at tick=%lu.  WAITED %lu ms ***\r\n",
                 (unsigned long)t1, (unsigned long)(t1 - t0));
        uart_print(buf);

#if USE_MUTEX
        if ((t1 - t0) < 4000)
            uart_print("[HIGH] -> Priority inheritance worked! Low preempted Medium.\r\n\r\n");
        else
            uart_print("[HIGH] -> Unexpected long wait even with mutex.\r\n\r\n");
#else
        if ((t1 - t0) > 5000)
            uart_print("[HIGH] -> PRIORITY INVERSION! Medium delayed Low, which blocked us.\r\n\r\n");
        else
            uart_print("[HIGH] -> Got lock without much inversion.\r\n\r\n");
#endif

        xSemaphoreGive(sharedLock);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ================================================================
 * Busy-wait (does NOT yield the CPU)
 *
 * Uses HAL_GetTick which increments from the SysTick ISR.
 * The task stays on the CPU the whole time, which is what
 * makes preemption visible.
 * ================================================================ */
static void busy_wait_ms(uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < ms)
    {
        /* spin */
    }
}

/* ================================================================
 * UART helper
 * ================================================================ */
static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ================================================================
 * Peripheral init
 * ================================================================ */
static void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static void UART2_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clk = {0};
    RCC_OscInitTypeDef osc = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_BYPASS;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 360;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    osc.PLL.PLLR       = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) for (;;);
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) for (;;);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                          RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for (;;);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while (1); }
#endif
