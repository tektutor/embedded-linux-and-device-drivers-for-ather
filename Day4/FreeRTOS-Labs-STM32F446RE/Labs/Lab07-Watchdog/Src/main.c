/**
  * Lab 7: Watchdog Task (Software Supervisor)
  * --------------------------------------------
  * Three worker tasks send periodic heartbeats to a watchdog task.
  * Worker C is programmed to stall after 10 seconds (simulates a bug).
  * The watchdog detects the missing heartbeat, prints an alert,
  * deletes the stalled task, and recreates it.
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
#define NUM_WORKERS  3

UART_HandleTypeDef huart2;

/* ---- Worker tracking ---- */
typedef struct {
    TaskHandle_t handle;
    const char  *name;
    uint32_t     lastBeat;     /* tick of last heartbeat */
    uint32_t     interval_ms;  /* expected heartbeat interval */
    uint32_t     beatCount;
    uint8_t      shouldStall;  /* 1 = this worker will stall */
} WorkerInfo;

static WorkerInfo workers[NUM_WORKERS];
TaskHandle_t watchdogHandle;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

/* ================================================================
 * Worker task
 *
 * Sends a heartbeat (xTaskNotifyGive) to the watchdog every second.
 * Worker C stalls after 10 seconds to simulate a bug.
 * ================================================================ */
static void Worker_Task(void *arg)
{
    WorkerInfo *info = (WorkerInfo *)arg;
    char buf[100];
    uint32_t startTick = HAL_GetTick();

    snprintf(buf, sizeof(buf),
             "[%s] Started at tick=%lu\r\n",
             info->name, (unsigned long)startTick);
    uart_print(buf);

    for (;;)
    {
        /* Check if this worker should stall */
        if (info->shouldStall && (HAL_GetTick() - startTick > 10000))
        {
            snprintf(buf, sizeof(buf),
                     "[%s] !!! STALLING NOW (simulated bug) !!!\r\n",
                     info->name);
            uart_print(buf);

            /* Infinite loop = stalled task */
            for (;;)
            {
                vTaskDelay(portMAX_DELAY);
            }
        }

        /* Normal operation: send heartbeat */
        info->beatCount++;
        info->lastBeat = HAL_GetTick();

        /* Notify the watchdog */
        xTaskNotifyGive(watchdogHandle);

        snprintf(buf, sizeof(buf),
                 "[%s] Heartbeat #%lu (tick=%lu)\r\n",
                 info->name,
                 (unsigned long)info->beatCount,
                 (unsigned long)info->lastBeat);
        uart_print(buf);

        vTaskDelay(pdMS_TO_TICKS(info->interval_ms));
    }
}

/* ================================================================
 * Watchdog task
 *
 * Checks every 3 seconds whether each worker sent a heartbeat
 * within its expected interval (with some tolerance).
 * If a worker missed its deadline, the watchdog logs an alert,
 * deletes the stalled task, and recreates it.
 * ================================================================ */
static void Watchdog_Task(void *arg)
{
    char buf[120];
    uint32_t checkInterval = 3000;  /* check every 3 seconds */
    (void)arg;

    uart_print("[WDG ] Watchdog started. Checking every 3 seconds.\r\n\r\n");

    /* Let workers start first */
    vTaskDelay(pdMS_TO_TICKS(2000));

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(checkInterval));

        uint32_t now = HAL_GetTick();
        uart_print("[WDG ] --- Health check ---\r\n");

        for (int i = 0; i < NUM_WORKERS; i++)
        {
            uint32_t elapsed = now - workers[i].lastBeat;
            /* Allow 2x the interval as tolerance */
            uint32_t timeout = workers[i].interval_ms * 2 + 500;

            if (elapsed > timeout)
            {
                /* STALLED! */
                snprintf(buf, sizeof(buf),
                         "[WDG ] ALERT: %s missed heartbeat! "
                         "Last beat %lu ms ago (timeout=%lu ms)\r\n",
                         workers[i].name,
                         (unsigned long)elapsed,
                         (unsigned long)timeout);
                uart_print(buf);

                /* Delete the stalled task */
                if (workers[i].handle != NULL)
                {
                    snprintf(buf, sizeof(buf),
                             "[WDG ] Deleting stalled task: %s\r\n",
                             workers[i].name);
                    uart_print(buf);
                    vTaskDelete(workers[i].handle);
                    workers[i].handle = NULL;
                }

                /* Recreate the worker */
                workers[i].lastBeat    = HAL_GetTick();
                workers[i].beatCount   = 0;
                workers[i].shouldStall = 0;  /* don't stall again */

                xTaskCreate(Worker_Task,
                            workers[i].name,
                            256,
                            &workers[i],
                            1,
                            &workers[i].handle);

                snprintf(buf, sizeof(buf),
                         "[WDG ] RECOVERED: %s recreated at tick=%lu\r\n\r\n",
                         workers[i].name,
                         (unsigned long)HAL_GetTick());
                uart_print(buf);

                HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            }
            else
            {
                snprintf(buf, sizeof(buf),
                         "[WDG ] %s OK (last beat %lu ms ago, beats=%lu)\r\n",
                         workers[i].name,
                         (unsigned long)elapsed,
                         (unsigned long)workers[i].beatCount);
                uart_print(buf);
            }
        }
        uart_print("\r\n");
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
    uart_print("  Lab 7: Watchdog Task\r\n");
    uart_print("=============================================\r\n");
    uart_print("  3 workers send heartbeats every 1 second\r\n");
    uart_print("  Watchdog checks health every 3 seconds\r\n");
    uart_print("  Worker C stalls at 10 seconds (bug)\r\n");
    uart_print("  Watchdog detects, kills, and restarts it\r\n");
    uart_print("=============================================\r\n\r\n");

    /* Configure workers */
    workers[0] = (WorkerInfo){
        .name = "WorkerA", .interval_ms = 1000,
        .shouldStall = 0, .lastBeat = 0, .beatCount = 0
    };
    workers[1] = (WorkerInfo){
        .name = "WorkerB", .interval_ms = 1000,
        .shouldStall = 0, .lastBeat = 0, .beatCount = 0
    };
    workers[2] = (WorkerInfo){
        .name = "WorkerC", .interval_ms = 1000,
        .shouldStall = 1, .lastBeat = 0, .beatCount = 0
        /* ^^^ This worker WILL stall after 10 seconds */
    };

    /* Create watchdog first (highest priority) */
    xTaskCreate(Watchdog_Task, "Watchdog", 512, NULL, 3, &watchdogHandle);

    /* Create workers */
    for (int i = 0; i < NUM_WORKERS; i++)
    {
        xTaskCreate(Worker_Task,
                    workers[i].name,
                    256,
                    &workers[i],
                    1,
                    &workers[i].handle);
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
