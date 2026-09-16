# Lab 01: Mutex (Mutual Exclusion)

## Board
STM32 Nucleo-F446RE

## Concept

When two tasks write to the same UART at the same time, their output gets mixed together.
Task A prints "Temperature: 23.5" and Task B prints "Status: OK". On the terminal you see
"TemStatus: Operature: 23.5K" because the scheduler switches between them mid-message.

A mutex prevents this. Before writing to UART, a task takes the mutex. If another task
already holds it, the requesting task sleeps until the mutex is free. After writing, the
task gives the mutex back.

```
    Without Mutex                      With Mutex
    ┌─────────┐  ┌─────────┐          ┌─────────┐  ┌─────────┐
    │ Task A  │  │ Task B  │          │ Task A  │  │ Task B  │
    └────┬────┘  └────┬────┘          └────┬────┘  └────┬────┘
         │            │                     │            │
    write "Temp│      │               TAKE mutex         │
         │       write "Stat│              │            │
    write "erat│      │               write "Temp"      │
         │       write "us:"│              │        BLOCKED
    write "ure:"      │               write "erature"   │
         │            │                     │            │
    GARBLED on UART                   GIVE mutex         │
                                           │        TAKE mutex
                                           │        write "Status"
                                           │        GIVE mutex
                                      CLEAN on UART
```

### Key Rules
- Only the task that took the mutex can give it back (ownership).
- FreeRTOS mutexes have priority inheritance built in.
- Never use a mutex inside an ISR.

### API

```c
SemaphoreHandle_t lock = xSemaphoreCreateMutex();
xSemaphoreTake(lock, portMAX_DELAY);   // acquire (blocks if held)
// ... use shared resource ...
xSemaphoreGive(lock);                  // release
```

## How to Build and Run

```bash
cd Lab01-Mutex
make clean
make
make flash
```

### Reset the Board
- **Physical**: Press the black RESET button on the Nucleo board.
- **Command**: `st-flash reset` (requires stlink-tools).

### Open Serial Monitor
```bash
minicom -D /dev/ttyACM0 -b 115200
```
First time: press Ctrl+A, then O, select Serial port setup, press F to disable hardware flow control, save as default.

## Toggle Between Broken and Fixed

Edit `Src/main.c` line 22:
```c
#define USE_MUTEX  0    // garbled output (broken)
#define USE_MUTEX  1    // clean output (fixed)
```
Build and flash each version to compare.

## Expected Output

### With USE_MUTEX 0 (broken):
```
[SENSOR] --- Reading #1 ---
[STATUS] === System Status[SENSOR] Temperature: 21.1 C
 #1 ===[SENSOR]
Humidity:    41%[STATUS] Uptime: 0 seconds

[STATUS] Free heap: 5824 bytes
```
The lines are interleaved. [STATUS] messages appear in the middle of [SENSOR] messages.

### With USE_MUTEX 1 (fixed):
```
[SENSOR] --- Reading #1 ---
[SENSOR] Temperature: 21.1 C
[SENSOR] Humidity:    41%

[STATUS] === System Status #1 ===
[STATUS] Uptime: 0 seconds
[STATUS] Free heap: 5824 bytes
```
Each block of 3 lines prints as a complete unit. No interleaving.

## Why the Output Differs

Without the mutex, both tasks have the same priority and run on a 1 ms time slice.
The scheduler can switch from Task_Sensor to Task_Status between any two lines.
The small busy-loop delay between lines in uart_print_block increases the chance
of a context switch happening at that exact moment.

With the mutex, xSemaphoreTake blocks the second task until the first task
finishes all three lines and calls xSemaphoreGive. The scheduler can still
switch tasks, but the second task immediately blocks on the mutex and does
not print anything until the first task releases it.

## Complete Source Code

```c
   1  /**
   2    * Lab 1: Mutex (Mutual Exclusion)
   3    * --------------------------------
   4    * Two tasks write multi-line messages to UART simultaneously.
   5    *
   6    * #define USE_MUTEX 0  -> output is garbled (no protection)
   7    * #define USE_MUTEX 1  -> output is clean (mutex protects UART)
   8    *
   9    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  10    */
  11  
  12  #include "main.h"
  13  #include "FreeRTOS.h"
  14  #include "task.h"
  15  #include "semphr.h"
  16  #include <stdio.h>
  17  #include <string.h>
  18  
  19  /* ================================================================
  20   * CHANGE THIS TO SEE THE DIFFERENCE
  21   *   0 = no mutex (garbled output)
  22   *   1 = mutex protects UART (clean output)
  23   * ================================================================ */
  24  #define USE_MUTEX  0
  25  
  26  #define LED_PIN      GPIO_PIN_5
  27  #define LED_PORT     GPIOA
  28  
  29  UART_HandleTypeDef huart2;
  30  SemaphoreHandle_t uartMutex;
  31  
  32  static void Task_Sensor(void *arg);
  33  static void Task_Status(void *arg);
  34  static void SystemClock_Config(void);
  35  static void GPIO_Init(void);
  36  static void UART2_Init(void);
  37  
  38  static void uart_print(const char *msg)
  39  {
  40      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  41  }
  42  
  43  /* Protected print: takes mutex, prints multiple lines, gives mutex */
  44  static void uart_print_block(const char *lines[], int count)
  45  {
  46  #if USE_MUTEX
  47      xSemaphoreTake(uartMutex, portMAX_DELAY);
  48  #endif
  49  
  50      for (int i = 0; i < count; i++)
  51      {
  52          uart_print(lines[i]);
  53          /* Small delay between lines to make interleaving visible
  54             when mutex is OFF */
  55          for (volatile int j = 0; j < 50000; j++);
  56      }
  57  
  58  #if USE_MUTEX
  59      xSemaphoreGive(uartMutex);
  60  #endif
  61  }
  62  
  63  int main(void)
  64  {
  65      HAL_Init();
  66      SystemClock_Config();
  67      GPIO_Init();
  68      UART2_Init();
  69  
  70      uart_print("\r\n=============================================\r\n");
  71      uart_print("  Lab 1: Mutex\r\n");
  72  #if USE_MUTEX
  73      uart_print("  Mode: MUTEX ON (output should be clean)\r\n");
  74  #else
  75      uart_print("  Mode: MUTEX OFF (output will be garbled)\r\n");
  76  #endif
  77      uart_print("=============================================\r\n\r\n");
  78  
  79      uartMutex = xSemaphoreCreateMutex();
  80  
  81      xTaskCreate(Task_Sensor, "Sensor", 512, NULL, 1, NULL);
  82      xTaskCreate(Task_Status, "Status", 512, NULL, 1, NULL);
  83  
  84      vTaskStartScheduler();
  85      for (;;);
  86  }
  87  
  88  /**
  89    * Task_Sensor: prints a 3-line sensor report every 500 ms
  90    */
  91  static void Task_Sensor(void *arg)
  92  {
  93      uint32_t count = 0;
  94      char line1[60], line2[60], line3[60];
  95      (void)arg;
  96  
  97      for (;;)
  98      {
  99          count++;
 100          snprintf(line1, sizeof(line1),
 101                   "[SENSOR] --- Reading #%lu ---\r\n", (unsigned long)count);
 102          snprintf(line2, sizeof(line2),
 103                   "[SENSOR] Temperature: %lu.%lu C\r\n",
 104                   (unsigned long)(20 + count % 10),
 105                   (unsigned long)(count % 10));
 106          snprintf(line3, sizeof(line3),
 107                   "[SENSOR] Humidity:    %lu%%\r\n\r\n",
 108                   (unsigned long)(40 + count % 30));
 109  
 110          const char *lines[] = { line1, line2, line3 };
 111          uart_print_block(lines, 3);
 112  
 113          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 114          vTaskDelay(pdMS_TO_TICKS(500));
 115      }
 116  }
 117  
 118  /**
 119    * Task_Status: prints a 3-line status report every 500 ms
 120    */
 121  static void Task_Status(void *arg)
 122  {
 123      uint32_t count = 0;
 124      char line1[60], line2[60], line3[60];
 125      (void)arg;
 126  
 127      for (;;)
 128      {
 129          count++;
 130          snprintf(line1, sizeof(line1),
 131                   "[STATUS] === System Status #%lu ===\r\n",
 132                   (unsigned long)count);
 133          snprintf(line2, sizeof(line2),
 134                   "[STATUS] Uptime: %lu seconds\r\n",
 135                   (unsigned long)(HAL_GetTick() / 1000));
 136          snprintf(line3, sizeof(line3),
 137                   "[STATUS] Free heap: %u bytes\r\n\r\n",
 138                   (unsigned)xPortGetFreeHeapSize());
 139  
 140          const char *lines[] = { line1, line2, line3 };
 141          uart_print_block(lines, 3);
 142  
 143          vTaskDelay(pdMS_TO_TICKS(500));
 144      }
 145  }
 146  
 147  /* ---- Peripheral init (same as previous labs) ---- */
 148  
 149  static void GPIO_Init(void)
 150  {
 151      GPIO_InitTypeDef gpio = {0};
 152      __HAL_RCC_GPIOA_CLK_ENABLE();
 153      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 154      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 155      HAL_GPIO_Init(LED_PORT, &gpio);
 156  }
 157  
 158  static void UART2_Init(void)
 159  {
 160      GPIO_InitTypeDef gpio = {0};
 161      __HAL_RCC_USART2_CLK_ENABLE();
 162      __HAL_RCC_GPIOA_CLK_ENABLE();
 163      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 164      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 165      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 166      HAL_GPIO_Init(GPIOA, &gpio);
 167      huart2.Instance = USART2;
 168      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 169      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 170      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 171      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 172      HAL_UART_Init(&huart2);
 173  }
 174  
 175  static void SystemClock_Config(void)
 176  {
 177      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 178      __HAL_RCC_PWR_CLK_ENABLE();
 179      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 180      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 181      osc.HSEState = RCC_HSE_BYPASS;
 182      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 183      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 184      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 185      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 186      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 187      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 188      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 189      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 190      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 191  }
 192  
 193  #ifdef USE_FULL_ASSERT
 194  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 195  #endif
 196  
```
