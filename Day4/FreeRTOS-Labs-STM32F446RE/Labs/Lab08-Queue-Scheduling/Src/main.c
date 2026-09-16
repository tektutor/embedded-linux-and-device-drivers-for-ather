/**
  * Lab: Tasks and Scheduling
  * --------------------------
  * NUCLEO-F446RE + FreeRTOS
  *
  * Producer task (HIGH priority)  : writes values into a queue every 2 seconds.
  * Consumer task (LOW  priority)  : reads from the queue and logs each value.
  * Idle work    (inside Consumer) : busy-loops between reads, printing dots,
  *                                  so you can SEE when it gets preempted.
  *
  * UART2 (PA2/PA3) is connected to the ST-Link virtual COM port.
  * Open a terminal at 115200 baud to watch the output.
  *
  *   minicom -D /dev/ttyACM0 -b 115200
  *
  * What you will observe:
  *   - Consumer prints dots continuously (low-priority idle work).
  *   - Every 2 seconds the Producer PREEMPTS the Consumer, sends a value
  *     to the queue, and prints a "PRODUCED" message.
  *   - The Consumer immediately receives the value and prints "CONSUMED",
  *     then resumes printing dots until the next preemption.
  */

#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* ---- LED: PA5 (LD2) ---- */
#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

/* ---- UART2: PA2 (TX), PA3 (RX) -> ST-Link VCP ---- */
UART_HandleTypeDef huart2;

/* ---- FreeRTOS objects ---- */
osThreadId producerHandle, consumerHandle;
osMessageQId queueHandle;

/* ---- Prototypes ---- */
static void Producer_Task(void const *arg);
static void Consumer_Task(void const *arg);
static void SystemClock_Config(void);
static void GPIO_Init(void);
static void UART2_Init(void);
static void uart_print(const char *msg);

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART2_Init();

    uart_print("\r\n========================================\r\n");
    uart_print("  FreeRTOS Queue Lab - Nucleo-F446RE\r\n");
    uart_print("  Producer = HIGH priority\r\n");
    uart_print("  Consumer = LOW  priority\r\n");
    uart_print("========================================\r\n\r\n");

    /* Create a queue that holds up to 5 uint32_t values */
    osMessageQDef(myQueue, 5, uint32_t);
    queueHandle = osMessageCreate(osMessageQ(myQueue), NULL);

    /* Producer: higher priority -> preempts Consumer */
    osThreadDef(Prod, Producer_Task, osPriorityAboveNormal, 0, 256);
    producerHandle = osThreadCreate(osThread(Prod), NULL);

    /* Consumer: lower priority -> runs when Producer is blocked */
    osThreadDef(Cons, Consumer_Task, osPriorityNormal, 0, 256);
    consumerHandle = osThreadCreate(osThread(Cons), NULL);

    osKernelStart();

    for (;;);
}

/* ================================================================
 * Producer Task (HIGH priority)
 *
 * Wakes every 2 seconds, sends a counter value into the queue.
 * Because it has higher priority, it PREEMPTS the Consumer
 * the instant osDelay expires.
 * ================================================================ */
static void Producer_Task(void const *arg)
{
    uint32_t counter = 0;
    char buf[80];
    (void)arg;

    for (;;)
    {
        osDelay(2000);   /* sleep 2 s -> Consumer runs during this time */

        counter++;
        osMessagePut(queueHandle, counter, 0);

        /* Toggle LED so you see a blink each time Producer runs */
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

        snprintf(buf, sizeof(buf),
                 "\r\n>> PRODUCED  value=%lu  (tick=%lu)\r\n",
                 (unsigned long)counter,
                 (unsigned long)osKernelSysTick());
        uart_print(buf);
    }
}

/* ================================================================
 * Consumer Task (LOW priority)
 *
 * Tries to read from the queue (non-blocking).
 *   - If a value is available, it prints CONSUMED.
 *   - Otherwise, it prints a dot and does a short busy-wait,
 *     simulating low-priority background work.
 *
 * The dots let you SEE preemption: the dot stream pauses
 * whenever the Producer wakes up and takes the CPU.
 * ================================================================ */
static void Consumer_Task(void const *arg)
{
    osEvent evt;
    char buf[80];
    uint32_t dot_count = 0;
    (void)arg;

    for (;;)
    {
        /* Non-blocking read from the queue */
        evt = osMessageGet(queueHandle, 0);

        if (evt.status == osEventMessage)
        {
            snprintf(buf, sizeof(buf),
                     "\r\n<< CONSUMED value=%lu  (tick=%lu)\r\n",
                     (unsigned long)evt.value.v,
                     (unsigned long)osKernelSysTick());
            uart_print(buf);
            dot_count = 0;
        }
        else
        {
            /* Background work: print a dot every 100 ms */
            uart_print(".");
            dot_count++;
            if (dot_count % 40 == 0)
                uart_print("\r\n");   /* line wrap for readability */

            osDelay(100);
        }
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

    /* LD2 on PA5 */
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

    /* PA2 = USART2_TX, PA3 = USART2_RX */
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

/* ================================================================
 * System Clock: 180 MHz, HSE bypass (Nucleo ST-Link MCO)
 * ================================================================ */
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
