/**
  * Lab 4: Queue of Structs
  * ------------------------
  * Three producer tasks simulate sensors (temperature, pressure, voltage).
  * Each pushes a SensorData struct into a shared queue.
  * One logger task reads the queue and prints each reading.
  *
  * Demonstrates typed message passing through a FreeRTOS queue.
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

UART_HandleTypeDef huart2;

/* ---- The struct that travels through the queue ---- */
typedef enum {
    SENSOR_TEMPERATURE,
    SENSOR_PRESSURE,
    SENSOR_VOLTAGE
} SensorType;

typedef struct {
    SensorType type;
    uint32_t   value_x100;    /* fixed-point: 2345 means 23.45 */
    uint32_t   sequence;
    uint32_t   timestamp;
} SensorData;

QueueHandle_t sensorQueue;

static void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

static const char* sensor_name(SensorType t)
{
    switch (t) {
        case SENSOR_TEMPERATURE: return "TEMP";
        case SENSOR_PRESSURE:    return "PRES";
        case SENSOR_VOLTAGE:     return "VOLT";
        default:                 return "????";
    }
}

static const char* sensor_unit(SensorType t)
{
    switch (t) {
        case SENSOR_TEMPERATURE: return "C";
        case SENSOR_PRESSURE:    return "kPa";
        case SENSOR_VOLTAGE:     return "V";
        default:                 return "";
    }
}

/* ---- Producer task (parameterized) ---- */

typedef struct {
    SensorType type;
    uint32_t   base_value;   /* base value x100 */
    uint32_t   variation;    /* random-ish variation x100 */
    uint32_t   interval_ms;
} SensorConfig;

static void Sensor_Task(void *arg)
{
    SensorConfig *cfg = (SensorConfig *)arg;
    SensorData reading;
    uint32_t seq = 0;
    char buf[80];

    snprintf(buf, sizeof(buf),
             "[%s] Sensor started. Interval: %lu ms\r\n",
             sensor_name(cfg->type), (unsigned long)cfg->interval_ms);
    uart_print(buf);

    for (;;)
    {
        seq++;
        reading.type      = cfg->type;
        reading.sequence  = seq;
        reading.timestamp = HAL_GetTick();
        /* Simulate varying readings */
        reading.value_x100 = cfg->base_value +
                             (HAL_GetTick() % cfg->variation);

        if (xQueueSend(sensorQueue, &reading, pdMS_TO_TICKS(100)) != pdPASS)
        {
            uart_print("[WARN] Queue full! Reading dropped.\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(cfg->interval_ms));
    }
}

/* ---- Logger task (consumer) ---- */

static void Logger_Task(void *arg)
{
    SensorData reading;
    char buf[120];
    (void)arg;

    uart_print("[LOG ] Logger started. Waiting for sensor data...\r\n\r\n");

    for (;;)
    {
        /* Block until data arrives in the queue */
        if (xQueueReceive(sensorQueue, &reading, portMAX_DELAY) == pdPASS)
        {
            uint32_t whole = reading.value_x100 / 100;
            uint32_t frac  = reading.value_x100 % 100;

            snprintf(buf, sizeof(buf),
                     "[LOG ] %s  seq=%3lu  value=%lu.%02lu %s  (tick=%lu)\r\n",
                     sensor_name(reading.type),
                     (unsigned long)reading.sequence,
                     (unsigned long)whole,
                     (unsigned long)frac,
                     sensor_unit(reading.type),
                     (unsigned long)reading.timestamp);
            uart_print(buf);

            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        }
    }
}

/* ---- Sensor configurations (static so they persist) ---- */
static SensorConfig tempConfig = {
    .type       = SENSOR_TEMPERATURE,
    .base_value = 2200,      /* 22.00 C base */
    .variation  = 500,       /* up to +5.00 C variation */
    .interval_ms = 1000      /* every 1 second */
};

static SensorConfig pressConfig = {
    .type       = SENSOR_PRESSURE,
    .base_value = 10100,     /* 101.00 kPa base */
    .variation  = 200,       /* up to +2.00 kPa */
    .interval_ms = 1500      /* every 1.5 seconds */
};

static SensorConfig voltConfig = {
    .type       = SENSOR_VOLTAGE,
    .base_value = 320,       /* 3.20 V base */
    .variation  = 30,        /* up to +0.30 V */
    .interval_ms = 800       /* every 0.8 seconds */
};

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
    uart_print("  Lab 4: Queue of Structs\r\n");
    uart_print("=============================================\r\n");
    uart_print("  3 sensor tasks -> shared queue -> 1 logger\r\n");
    uart_print("  Queue depth: 10 items\r\n");
    uart_print("  Struct size: SensorData (type+value+seq+tick)\r\n");
    uart_print("=============================================\r\n\r\n");

    /* Queue holds 10 SensorData structs */
    sensorQueue = xQueueCreate(10, sizeof(SensorData));

    xTaskCreate(Sensor_Task, "Temp",   256, &tempConfig,  1, NULL);
    xTaskCreate(Sensor_Task, "Press",  256, &pressConfig, 1, NULL);
    xTaskCreate(Sensor_Task, "Volt",   256, &voltConfig,  1, NULL);
    xTaskCreate(Logger_Task, "Logger", 512, NULL,          2, NULL);

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
