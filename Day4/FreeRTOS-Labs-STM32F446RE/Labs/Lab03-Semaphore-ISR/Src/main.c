/**
  * Lab 3: Binary Semaphore from ISR
  * ----------------------------------
  * Blue user button (PC13) generates an EXTI interrupt.
  * The ISR gives a binary semaphore.
  * A task waits on the semaphore, prints the event, toggles LED.
  *
  * Press the blue button on the Nucleo to see ISR -> task signaling.
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
#define BTN_PIN      GPIO_PIN_13
#define BTN_PORT     GPIOC

UART_HandleTypeDef huart2;
SemaphoreHandle_t buttonSem;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
  * EXTI interrupt handler for PC13.
  * Called by the hardware when the blue button is pressed.
  * This runs in ISR context, so we use the FromISR variant.
  */
void EXTI15_10_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(BTN_PIN) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(BTN_PIN);

        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(buttonSem, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/**
  * Button handler task.
  * Blocks on the semaphore with zero CPU cost.
  * Wakes instantly when the ISR gives the semaphore.
  */
static void Button_Task(void *arg)
{
    uint32_t pressCount = 0;
    char buf[100];
    (void)arg;

    uart_print("[TASK] Waiting for button press...\r\n\r\n");

    for (;;)
    {
        /* Block until ISR gives the semaphore */
        xSemaphoreTake(buttonSem, portMAX_DELAY);

        pressCount++;
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

        snprintf(buf, sizeof(buf),
                 "[TASK] Button press #%lu detected at tick=%lu\r\n",
                 (unsigned long)pressCount,
                 (unsigned long)HAL_GetTick());
        uart_print(buf);

        /* Simple debounce: ignore presses for 200 ms */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
  * Background task that prints periodically.
  * Shows that the system is alive between button presses.
  */
static void Idle_Task(void *arg)
{
    (void)arg;

    for (;;)
    {
        uart_print("[IDLE] System running... press the blue button.\r\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ---- Peripheral init ---- */
static void GPIO_Init(void);
static void Button_Init(void);
static void UART2_Init(void);
static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    Button_Init();
    UART2_Init();

    uart_print("\r\n=============================================\r\n");
    uart_print("  Lab 3: Binary Semaphore from ISR\r\n");
    uart_print("=============================================\r\n");
    uart_print("  Blue button (PC13) -> EXTI ISR\r\n");
    uart_print("  ISR gives semaphore -> Task wakes up\r\n");
    uart_print("  Task prints event and toggles LED\r\n");
    uart_print("=============================================\r\n\r\n");

    buttonSem = xSemaphoreCreateBinary();

    xTaskCreate(Button_Task, "Button", 512, NULL, 2, NULL);
    xTaskCreate(Idle_Task,   "Idle",   256, NULL, 1, NULL);

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

static void Button_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PC13: input with falling edge interrupt (button press = falling) */
    gpio.Pin  = BTN_PIN;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_NOPULL;  /* Nucleo has external pull-up on PC13 */
    HAL_GPIO_Init(BTN_PORT, &gpio);

    /* Enable EXTI15_10 interrupt in NVIC */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    /*
     * Priority 6 is important: FreeRTOS requires ISR priorities to be
     * at or below configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)
     * numerically. Priority 6 is LOWER priority than 5, so it is
     * safe to call FromISR functions from this handler.
     */
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
