# Lab 05: Task Notifications

## Board
STM32 Nucleo-F446RE

## Concept

Task notifications are a lightweight alternative to semaphores and queues. Every FreeRTOS
task has a built-in 32-bit notification value.

```
    Notification (direct, fast)      Queue (indirect, flexible)

    ┌──────────┐  xTaskNotify  ┌──────────┐     ┌──────────┐ ┌─────┐ ┌──────────┐
    │ Producer ├──────────────►│ Consumer │     │ Producer ├►│Queue├►│ Consumer │
    └──────────┘  (one step)   └──────────┘     └──────────┘ └─────┘ └──────────┘
                                                             (two steps)
    ~45% faster                                 More flexible
    No RAM allocation                           Any task can read
    Must know receiver                          Multiple senders OK
```

### API
```c
xTaskNotify(taskHandle, value, eSetValueWithOverwrite);  // send
xTaskNotifyWait(0, 0xFFFFFFFF, &value, portMAX_DELAY);   // receive
```

## How to Build and Run

```bash
cd Lab05-Task-Notifications
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
[NOTIFY] Starting benchmark: sending 1000 values...
[NOTIFY] Benchmark done: 1000 values in 23 ms

[QUEUE ] Starting benchmark: sending 1000 values...
[QUEUE ] Benchmark done: 1000 values in 38 ms

=== RESULTS ===
  Notification: 23 ms for 1000 values
  Queue:        38 ms for 1000 values
  Notifications were 39% faster.

[NOTIFY] Received event #1 at tick=5230
[NOTIFY] Received event #2 at tick=6230
```

The exact numbers vary, but notifications are consistently faster. After the benchmark,
the notification pair runs continuously so you can see each event arriving.

## Complete Source Code

```c
   1  /**
   2    * Lab 5: Task Notifications
   3    * ---------------------------
   4    * Two pairs of producer/consumer:
   5    *   Pair A: uses xTaskNotify / xTaskNotifyWait  (lightweight)
   6    *   Pair B: uses xQueueSend  / xQueueReceive    (standard)
   7    *
   8    * Both pairs exchange 1000 values. The tick count for each
   9    * approach is printed so you can compare performance.
  10    *
  11    * Then the notification pair runs continuously so you can see
  12    * the values flowing on UART.
  13    *
  14    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  15    */
  16  
  17  #include "main.h"
  18  #include "FreeRTOS.h"
  19  #include "task.h"
  20  #include "queue.h"
  21  #include <stdio.h>
  22  #include <string.h>
  23  
  24  #define LED_PIN      GPIO_PIN_5
  25  #define LED_PORT     GPIOA
  26  #define BENCH_COUNT  1000
  27  
  28  UART_HandleTypeDef huart2;
  29  
  30  TaskHandle_t hNotifyConsumer;
  31  TaskHandle_t hQueueProducer, hQueueConsumer;
  32  QueueHandle_t benchQueue;
  33  
  34  volatile uint32_t notifyDone = 0;
  35  volatile uint32_t queueDone  = 0;
  36  volatile uint32_t notifyTicks = 0;
  37  volatile uint32_t queueTicks  = 0;
  38  
  39  static void uart_print(const char *msg)
  40  {
  41      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  42  }
  43  
  44  /* ================================================================
  45   * Pair A: Task Notifications
  46   * ================================================================ */
  47  
  48  static void Notify_Producer(void *arg)
  49  {
  50      char buf[100];
  51      uint32_t t0, t1;
  52      (void)arg;
  53  
  54      /* Wait for consumer to be ready */
  55      vTaskDelay(pdMS_TO_TICKS(100));
  56  
  57      uart_print("[NOTIFY] Starting benchmark: sending 1000 values...\r\n");
  58      t0 = HAL_GetTick();
  59  
  60      for (uint32_t i = 1; i <= BENCH_COUNT; i++)
  61      {
  62          xTaskNotify(hNotifyConsumer, i, eSetValueWithOverwrite);
  63          /* Tiny yield so consumer can process */
  64          taskYIELD();
  65      }
  66  
  67      t1 = HAL_GetTick();
  68      notifyTicks = t1 - t0;
  69      notifyDone = 1;
  70  
  71      snprintf(buf, sizeof(buf),
  72               "[NOTIFY] Benchmark done: %lu values in %lu ms\r\n\r\n",
  73               (unsigned long)BENCH_COUNT, (unsigned long)notifyTicks);
  74      uart_print(buf);
  75  
  76      /* Now run continuously for demonstration */
  77      uart_print("[NOTIFY] Continuous mode: sending event every second...\r\n\r\n");
  78      uint32_t event = 0;
  79      for (;;)
  80      {
  81          event++;
  82          xTaskNotify(hNotifyConsumer, event, eSetValueWithOverwrite);
  83          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
  84          vTaskDelay(pdMS_TO_TICKS(1000));
  85      }
  86  }
  87  
  88  static void Notify_Consumer(void *arg)
  89  {
  90      uint32_t value;
  91      uint32_t received = 0;
  92      char buf[100];
  93      (void)arg;
  94  
  95      /* Benchmark phase: receive 1000 values silently */
  96      while (received < BENCH_COUNT)
  97      {
  98          if (xTaskNotifyWait(0, 0xFFFFFFFF, &value, pdMS_TO_TICKS(500)) == pdTRUE)
  99          {
 100              received++;
 101          }
 102      }
 103  
 104      /* Continuous phase: print each value */
 105      for (;;)
 106      {
 107          if (xTaskNotifyWait(0, 0xFFFFFFFF, &value, portMAX_DELAY) == pdTRUE)
 108          {
 109              snprintf(buf, sizeof(buf),
 110                       "[NOTIFY] Received event #%lu at tick=%lu\r\n",
 111                       (unsigned long)value,
 112                       (unsigned long)HAL_GetTick());
 113              uart_print(buf);
 114          }
 115      }
 116  }
 117  
 118  /* ================================================================
 119   * Pair B: Queue (for comparison)
 120   * ================================================================ */
 121  
 122  static void Queue_Producer(void *arg)
 123  {
 124      uint32_t t0, t1;
 125      char buf[100];
 126      (void)arg;
 127  
 128      /* Wait for notify benchmark to finish */
 129      while (!notifyDone) vTaskDelay(pdMS_TO_TICKS(100));
 130  
 131      vTaskDelay(pdMS_TO_TICKS(500));
 132      uart_print("[QUEUE ] Starting benchmark: sending 1000 values...\r\n");
 133      t0 = HAL_GetTick();
 134  
 135      for (uint32_t i = 1; i <= BENCH_COUNT; i++)
 136      {
 137          xQueueSend(benchQueue, &i, portMAX_DELAY);
 138          taskYIELD();
 139      }
 140  
 141      t1 = HAL_GetTick();
 142      queueTicks = t1 - t0;
 143      queueDone = 1;
 144  
 145      snprintf(buf, sizeof(buf),
 146               "[QUEUE ] Benchmark done: %lu values in %lu ms\r\n\r\n",
 147               (unsigned long)BENCH_COUNT, (unsigned long)queueTicks);
 148      uart_print(buf);
 149  
 150      /* Print comparison */
 151      snprintf(buf, sizeof(buf),
 152               "=== RESULTS ===\r\n"
 153               "  Notification: %lu ms for %lu values\r\n",
 154               (unsigned long)notifyTicks, (unsigned long)BENCH_COUNT);
 155      uart_print(buf);
 156      snprintf(buf, sizeof(buf),
 157               "  Queue:        %lu ms for %lu values\r\n",
 158               (unsigned long)queueTicks, (unsigned long)BENCH_COUNT);
 159      uart_print(buf);
 160  
 161      if (queueTicks > 0 && notifyTicks > 0)
 162      {
 163          if (notifyTicks < queueTicks)
 164          {
 165              snprintf(buf, sizeof(buf),
 166                       "  Notifications were %lu%% faster.\r\n\r\n",
 167                       (unsigned long)((queueTicks - notifyTicks) * 100 / queueTicks));
 168          }
 169          else
 170          {
 171              uart_print("  Results too close to measure at 1 ms tick resolution.\r\n\r\n");
 172          }
 173      }
 174      uart_print("Notification producer continues running above...\r\n\r\n");
 175  
 176      /* This task is done */
 177      vTaskDelete(NULL);
 178  }
 179  
 180  static void Queue_Consumer(void *arg)
 181  {
 182      uint32_t value;
 183      uint32_t received = 0;
 184      (void)arg;
 185  
 186      while (received < BENCH_COUNT)
 187      {
 188          if (xQueueReceive(benchQueue, &value, pdMS_TO_TICKS(500)) == pdPASS)
 189          {
 190              received++;
 191          }
 192      }
 193  
 194      vTaskDelete(NULL);
 195  }
 196  
 197  /* ---- Peripheral init ---- */
 198  static void GPIO_Init(void);
 199  static void UART2_Init(void);
 200  static void SystemClock_Config(void);
 201  
 202  int main(void)
 203  {
 204      HAL_Init();
 205      SystemClock_Config();
 206      GPIO_Init();
 207      UART2_Init();
 208  
 209      uart_print("\r\n=============================================\r\n");
 210      uart_print("  Lab 5: Task Notifications vs Queue\r\n");
 211      uart_print("=============================================\r\n");
 212      uart_print("  Benchmark: 1000 values through each method\r\n");
 213      uart_print("  Then continuous notification demo\r\n");
 214      uart_print("=============================================\r\n\r\n");
 215  
 216      benchQueue = xQueueCreate(10, sizeof(uint32_t));
 217  
 218      /* Create notification pair */
 219      xTaskCreate(Notify_Consumer,  "NotCon", 512, NULL, 2, &hNotifyConsumer);
 220      xTaskCreate(Notify_Producer,  "NotPro", 512, NULL, 1, NULL);
 221  
 222      /* Create queue pair (starts after notify benchmark) */
 223      xTaskCreate(Queue_Consumer,   "QueCon", 512, NULL, 2, &hQueueConsumer);
 224      xTaskCreate(Queue_Producer,   "QuePro", 512, NULL, 1, &hQueueProducer);
 225  
 226      vTaskStartScheduler();
 227      for (;;);
 228  }
 229  
 230  static void GPIO_Init(void)
 231  {
 232      GPIO_InitTypeDef gpio = {0};
 233      __HAL_RCC_GPIOA_CLK_ENABLE();
 234      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 235      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 236      HAL_GPIO_Init(LED_PORT, &gpio);
 237  }
 238  
 239  static void UART2_Init(void)
 240  {
 241      GPIO_InitTypeDef gpio = {0};
 242      __HAL_RCC_USART2_CLK_ENABLE();
 243      __HAL_RCC_GPIOA_CLK_ENABLE();
 244      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 245      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 246      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 247      HAL_GPIO_Init(GPIOA, &gpio);
 248      huart2.Instance = USART2;
 249      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 250      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 251      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 252      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 253      HAL_UART_Init(&huart2);
 254  }
 255  
 256  static void SystemClock_Config(void)
 257  {
 258      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 259      __HAL_RCC_PWR_CLK_ENABLE();
 260      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 261      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 262      osc.HSEState = RCC_HSE_BYPASS;
 263      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 264      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 265      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 266      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 267      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 268      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 269      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 270      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 271      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 272  }
 273  
 274  #ifdef USE_FULL_ASSERT
 275  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 276  #endif
 277  
```
