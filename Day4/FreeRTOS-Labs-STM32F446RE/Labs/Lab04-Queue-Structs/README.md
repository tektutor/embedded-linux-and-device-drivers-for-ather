# Lab 04: Queue of Structs

## Board
STM32 Nucleo-F446RE

## Concept

A queue is a FIFO buffer managed by the kernel. FreeRTOS queues copy data, so the sender
and receiver do not share memory. This makes them inherently thread-safe.

```
    ┌──────────┐
    │ Temp     │──push──┐
    │ Task     │  1.0s  │
    └──────────┘        │     ┌───────────────────────┐
                        ├────►│  Queue (10 slots)     │────►┌──────────┐
    ┌──────────┐        │     │  [SensorData structs] │     │ Logger   │
    │ Pressure │──push──┤     └───────────────────────┘     │ Task     │
    │ Task     │  1.5s  │                                   │ prints   │
    └──────────┘        │                                   │ each one │
                        │                                   └──────────┘
    ┌──────────┐        │
    │ Voltage  │──push──┘
    │ Task     │  0.8s
    └──────────┘

    struct SensorData {
        SensorType type;       // TEMP, PRESSURE, VOLTAGE
        uint32_t   value_x100; // fixed-point: 2345 = 23.45
        uint32_t   sequence;   // reading number
        uint32_t   timestamp;  // tick when taken
    };
```

### API
```c
QueueHandle_t q = xQueueCreate(10, sizeof(SensorData));
xQueueSend(q, &reading, portMAX_DELAY);     // blocks if full
xQueueReceive(q, &received, portMAX_DELAY); // blocks if empty
```

## How to Build and Run

```bash
cd Lab04-Queue-Structs
make clean
make
make flash
```

Reset: press the black RESET button or run `st-flash reset`.

```bash
minicom -D /dev/ttyACM0 -b 115200
```

## Expected Output
```
[TEMP] Sensor started. Interval: 1000 ms
[PRES] Sensor started. Interval: 1500 ms
[VOLT] Sensor started. Interval: 800 ms
[LOG ] Logger started. Waiting for sensor data...

[LOG ] VOLT  seq=  1  value=3.20 V     (tick=800)
[LOG ] TEMP  seq=  1  value=22.00 C    (tick=1000)
[LOG ] PRES  seq=  1  value=101.00 kPa (tick=1500)
[LOG ] VOLT  seq=  2  value=3.36 V     (tick=1600)
[LOG ] TEMP  seq=  2  value=22.00 C    (tick=2000)
[LOG ] VOLT  seq=  3  value=3.20 V     (tick=2400)
```

Readings arrive in FIFO order. Voltage (800 ms) appears most often, pressure (1500 ms)
least often. The logger processes each struct as it arrives, blocking when the queue is empty.

## Complete Source Code

```c
   1  /**
   2    * Lab 4: Queue of Structs
   3    * ------------------------
   4    * Three producer tasks simulate sensors (temperature, pressure, voltage).
   5    * Each pushes a SensorData struct into a shared queue.
   6    * One logger task reads the queue and prints each reading.
   7    *
   8    * Demonstrates typed message passing through a FreeRTOS queue.
   9    *
  10    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  11    */
  12  
  13  #include "main.h"
  14  #include "FreeRTOS.h"
  15  #include "task.h"
  16  #include "queue.h"
  17  #include <stdio.h>
  18  #include <string.h>
  19  
  20  #define LED_PIN      GPIO_PIN_5
  21  #define LED_PORT     GPIOA
  22  
  23  UART_HandleTypeDef huart2;
  24  
  25  /* ---- The struct that travels through the queue ---- */
  26  typedef enum {
  27      SENSOR_TEMPERATURE,
  28      SENSOR_PRESSURE,
  29      SENSOR_VOLTAGE
  30  } SensorType;
  31  
  32  typedef struct {
  33      SensorType type;
  34      uint32_t   value_x100;    /* fixed-point: 2345 means 23.45 */
  35      uint32_t   sequence;
  36      uint32_t   timestamp;
  37  } SensorData;
  38  
  39  QueueHandle_t sensorQueue;
  40  
  41  static void uart_print(const char *msg)
  42  {
  43      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  44  }
  45  
  46  static const char* sensor_name(SensorType t)
  47  {
  48      switch (t) {
  49          case SENSOR_TEMPERATURE: return "TEMP";
  50          case SENSOR_PRESSURE:    return "PRES";
  51          case SENSOR_VOLTAGE:     return "VOLT";
  52          default:                 return "????";
  53      }
  54  }
  55  
  56  static const char* sensor_unit(SensorType t)
  57  {
  58      switch (t) {
  59          case SENSOR_TEMPERATURE: return "C";
  60          case SENSOR_PRESSURE:    return "kPa";
  61          case SENSOR_VOLTAGE:     return "V";
  62          default:                 return "";
  63      }
  64  }
  65  
  66  /* ---- Producer task (parameterized) ---- */
  67  
  68  typedef struct {
  69      SensorType type;
  70      uint32_t   base_value;   /* base value x100 */
  71      uint32_t   variation;    /* random-ish variation x100 */
  72      uint32_t   interval_ms;
  73  } SensorConfig;
  74  
  75  static void Sensor_Task(void *arg)
  76  {
  77      SensorConfig *cfg = (SensorConfig *)arg;
  78      SensorData reading;
  79      uint32_t seq = 0;
  80      char buf[80];
  81  
  82      snprintf(buf, sizeof(buf),
  83               "[%s] Sensor started. Interval: %lu ms\r\n",
  84               sensor_name(cfg->type), (unsigned long)cfg->interval_ms);
  85      uart_print(buf);
  86  
  87      for (;;)
  88      {
  89          seq++;
  90          reading.type      = cfg->type;
  91          reading.sequence  = seq;
  92          reading.timestamp = HAL_GetTick();
  93          /* Simulate varying readings */
  94          reading.value_x100 = cfg->base_value +
  95                               (HAL_GetTick() % cfg->variation);
  96  
  97          if (xQueueSend(sensorQueue, &reading, pdMS_TO_TICKS(100)) != pdPASS)
  98          {
  99              uart_print("[WARN] Queue full! Reading dropped.\r\n");
 100          }
 101  
 102          vTaskDelay(pdMS_TO_TICKS(cfg->interval_ms));
 103      }
 104  }
 105  
 106  /* ---- Logger task (consumer) ---- */
 107  
 108  static void Logger_Task(void *arg)
 109  {
 110      SensorData reading;
 111      char buf[120];
 112      (void)arg;
 113  
 114      uart_print("[LOG ] Logger started. Waiting for sensor data...\r\n\r\n");
 115  
 116      for (;;)
 117      {
 118          /* Block until data arrives in the queue */
 119          if (xQueueReceive(sensorQueue, &reading, portMAX_DELAY) == pdPASS)
 120          {
 121              uint32_t whole = reading.value_x100 / 100;
 122              uint32_t frac  = reading.value_x100 % 100;
 123  
 124              snprintf(buf, sizeof(buf),
 125                       "[LOG ] %s  seq=%3lu  value=%lu.%02lu %s  (tick=%lu)\r\n",
 126                       sensor_name(reading.type),
 127                       (unsigned long)reading.sequence,
 128                       (unsigned long)whole,
 129                       (unsigned long)frac,
 130                       sensor_unit(reading.type),
 131                       (unsigned long)reading.timestamp);
 132              uart_print(buf);
 133  
 134              HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 135          }
 136      }
 137  }
 138  
 139  /* ---- Sensor configurations (static so they persist) ---- */
 140  static SensorConfig tempConfig = {
 141      .type       = SENSOR_TEMPERATURE,
 142      .base_value = 2200,      /* 22.00 C base */
 143      .variation  = 500,       /* up to +5.00 C variation */
 144      .interval_ms = 1000      /* every 1 second */
 145  };
 146  
 147  static SensorConfig pressConfig = {
 148      .type       = SENSOR_PRESSURE,
 149      .base_value = 10100,     /* 101.00 kPa base */
 150      .variation  = 200,       /* up to +2.00 kPa */
 151      .interval_ms = 1500      /* every 1.5 seconds */
 152  };
 153  
 154  static SensorConfig voltConfig = {
 155      .type       = SENSOR_VOLTAGE,
 156      .base_value = 320,       /* 3.20 V base */
 157      .variation  = 30,        /* up to +0.30 V */
 158      .interval_ms = 800       /* every 0.8 seconds */
 159  };
 160  
 161  /* ---- Peripheral init ---- */
 162  static void GPIO_Init(void);
 163  static void UART2_Init(void);
 164  static void SystemClock_Config(void);
 165  
 166  int main(void)
 167  {
 168      HAL_Init();
 169      SystemClock_Config();
 170      GPIO_Init();
 171      UART2_Init();
 172  
 173      uart_print("\r\n=============================================\r\n");
 174      uart_print("  Lab 4: Queue of Structs\r\n");
 175      uart_print("=============================================\r\n");
 176      uart_print("  3 sensor tasks -> shared queue -> 1 logger\r\n");
 177      uart_print("  Queue depth: 10 items\r\n");
 178      uart_print("  Struct size: SensorData (type+value+seq+tick)\r\n");
 179      uart_print("=============================================\r\n\r\n");
 180  
 181      /* Queue holds 10 SensorData structs */
 182      sensorQueue = xQueueCreate(10, sizeof(SensorData));
 183  
 184      xTaskCreate(Sensor_Task, "Temp",   256, &tempConfig,  1, NULL);
 185      xTaskCreate(Sensor_Task, "Press",  256, &pressConfig, 1, NULL);
 186      xTaskCreate(Sensor_Task, "Volt",   256, &voltConfig,  1, NULL);
 187      xTaskCreate(Logger_Task, "Logger", 512, NULL,          2, NULL);
 188  
 189      vTaskStartScheduler();
 190      for (;;);
 191  }
 192  
 193  static void GPIO_Init(void)
 194  {
 195      GPIO_InitTypeDef gpio = {0};
 196      __HAL_RCC_GPIOA_CLK_ENABLE();
 197      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 198      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 199      HAL_GPIO_Init(LED_PORT, &gpio);
 200  }
 201  
 202  static void UART2_Init(void)
 203  {
 204      GPIO_InitTypeDef gpio = {0};
 205      __HAL_RCC_USART2_CLK_ENABLE();
 206      __HAL_RCC_GPIOA_CLK_ENABLE();
 207      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 208      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 209      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 210      HAL_GPIO_Init(GPIOA, &gpio);
 211      huart2.Instance = USART2;
 212      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 213      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 214      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 215      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 216      HAL_UART_Init(&huart2);
 217  }
 218  
 219  static void SystemClock_Config(void)
 220  {
 221      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 222      __HAL_RCC_PWR_CLK_ENABLE();
 223      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 224      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 225      osc.HSEState = RCC_HSE_BYPASS;
 226      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 227      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 228      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 229      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 230      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 231      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 232      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 233      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 234      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 235  }
 236  
 237  #ifdef USE_FULL_ASSERT
 238  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 239  #endif
 240  
```
