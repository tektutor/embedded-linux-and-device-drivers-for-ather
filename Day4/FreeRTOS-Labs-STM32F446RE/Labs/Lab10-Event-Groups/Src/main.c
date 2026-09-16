/**
  * Lab 10: Event Groups
  * ---------------------
  * Three worker tasks each complete a stage of initialization.
  * Each sets a different bit in an event group when done.
  * A coordinator task waits for ALL three bits before proceeding.
  *
  * Demonstrates:
  *   - xEventGroupSetBits   (worker signals completion)
  *   - xEventGroupWaitBits  (coordinator waits for all)
  *   - Synchronization barrier pattern
  *
  * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <stdio.h>
#include <string.h>

#define LED_PIN      GPIO_PIN_5
#define LED_PORT     GPIOA

UART_HandleTypeDef huart2;

/* Each worker sets one bit in the event group */
#define SENSOR_READY   (1 << 0)   /* bit 0 */
#define NETWORK_READY  (1 << 1)   /* bit 1 */
#define STORAGE_READY  (1 << 2)   /* bit 2 */
#define ALL_READY      (SENSOR_READY | NETWORK_READY | STORAGE_READY)

EventGroupHandle_t startupEvents;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/**
  * Simulates sensor subsystem initialization.
  * Takes 2 seconds, then sets SENSOR_READY bit.
  */
static void Sensor_Init_Task(void *arg)
{
    char buf[100];
    (void)arg;

    for (;;)
    {
        uart_print("[SENSOR ] Initializing sensors...\r\n");
        vTaskDelay(pdMS_TO_TICKS(2000));  /* simulate 2 sec init */

        snprintf(buf, sizeof(buf),
                 "[SENSOR ] Sensors ready at tick=%lu. Setting bit 0.\r\n",
                 (unsigned long)HAL_GetTick());
        uart_print(buf);

        xEventGroupSetBits(startupEvents, SENSOR_READY);

        /* Wait before next cycle */
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

/**
  * Simulates network subsystem initialization.
  * Takes 4 seconds (slowest), then sets NETWORK_READY bit.
  */
static void Network_Init_Task(void *arg)
{
    char buf[100];
    (void)arg;

    for (;;)
    {
        uart_print("[NETWORK] Connecting to network...\r\n");
        vTaskDelay(pdMS_TO_TICKS(4000));  /* simulate 4 sec init */

        snprintf(buf, sizeof(buf),
                 "[NETWORK] Network connected at tick=%lu. Setting bit 1.\r\n",
                 (unsigned long)HAL_GetTick());
        uart_print(buf);

        xEventGroupSetBits(startupEvents, NETWORK_READY);

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

/**
  * Simulates storage subsystem initialization.
  * Takes 3 seconds, then sets STORAGE_READY bit.
  */
static void Storage_Init_Task(void *arg)
{
    char buf[100];
    (void)arg;

    for (;;)
    {
        uart_print("[STORAGE] Mounting filesystem...\r\n");
        vTaskDelay(pdMS_TO_TICKS(3000));  /* simulate 3 sec init */

        snprintf(buf, sizeof(buf),
                 "[STORAGE] Storage mounted at tick=%lu. Setting bit 2.\r\n",
                 (unsigned long)HAL_GetTick());
        uart_print(buf);

        xEventGroupSetBits(startupEvents, STORAGE_READY);

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

/**
  * Coordinator: waits for ALL three subsystems before starting.
  */
static void Coordinator_Task(void *arg)
{
    char buf[120];
    EventBits_t bits;
    uint32_t cycle = 0;
    (void)arg;

    for (;;)
    {
        cycle++;
        snprintf(buf, sizeof(buf),
                 "\r\n[COORD  ] === Cycle %lu: Waiting for all subsystems... ===\r\n",
                 (unsigned long)cycle);
        uart_print(buf);

        uint32_t t0 = HAL_GetTick();

        /*
         * Wait for ALL three bits to be set.
         *   pdTRUE  = clear bits after returning (reset for next cycle)
         *   pdTRUE  = wait for ALL bits (AND), not ANY (OR)
         */
        bits = xEventGroupWaitBits(
            startupEvents,
            ALL_READY,      /* bits to wait for */
            pdTRUE,         /* clear on exit    */
            pdTRUE,         /* wait for ALL     */
            portMAX_DELAY   /* wait forever     */
        );

        uint32_t t1 = HAL_GetTick();

        snprintf(buf, sizeof(buf),
                 "[COORD  ] ALL READY! bits=0x%02lX, waited %lu ms\r\n",
                 (unsigned long)bits, (unsigned long)(t1 - t0));
        uart_print(buf);

        uart_print("[COORD  ] System fully operational. Running main loop.\r\n");

        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);

        /* Simulate main application work */
        for (int i = 0; i < 5; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            snprintf(buf, sizeof(buf),
                     "[COORD  ] Main loop iteration %d/5\r\n", i + 1);
            uart_print(buf);
        }

        uart_print("[COORD  ] Cycle complete. Restarting init sequence...\r\n\r\n");
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
    uart_print("  Lab 10: Event Groups\r\n");
    uart_print("=============================================\r\n");
    uart_print("  3 subsystems init at different speeds\r\n");
    uart_print("  Coordinator waits for ALL to be ready\r\n");
    uart_print("  Bit 0=Sensor  Bit 1=Network  Bit 2=Storage\r\n");
    uart_print("=============================================\r\n\r\n");

    startupEvents = xEventGroupCreate();

    xTaskCreate(Sensor_Init_Task,  "Sensor",  256, NULL, 1, NULL);
    xTaskCreate(Network_Init_Task, "Network", 256, NULL, 1, NULL);
    xTaskCreate(Storage_Init_Task, "Storage", 256, NULL, 1, NULL);
    xTaskCreate(Coordinator_Task,  "Coord",   512, NULL, 2, NULL);

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
