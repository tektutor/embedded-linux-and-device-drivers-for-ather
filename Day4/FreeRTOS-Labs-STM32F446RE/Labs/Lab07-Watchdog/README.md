# Lab 07: Watchdog Task (Software Supervisor)

## Board
STM32 Nucleo-F446RE

## Concept

A watchdog task monitors other tasks for stalls. Each worker sends periodic heartbeats.
If a heartbeat is missed, the watchdog detects it and takes recovery action.

```
    ┌──────────┐     heartbeat      ┌──────────────┐
    │ Worker A ├──────every 1s─────►│              │
    └──────────┘                    │              │
    ┌──────────┐     heartbeat      │   Watchdog   │──check every 3s──►
    │ Worker B ├──────every 1s─────►│   Task       │  "all OK" or
    └──────────┘                    │              │  "ALERT: X stalled"
    ┌──────────┐     heartbeat      │              │
    │ Worker C ├──────every 1s─────►│              │
    │ (stalls  │     stops at 10s   │              │──► delete + restart
    │  at 10s) │                    └──────────────┘
    └──────────┘
```

## How to Build and Run

```bash
cd Lab07-Watchdog
make clean
make
make flash
```

Reset: press the black RESET button or run `st-flash reset`.

## Expected Output
```
[WorkerA] Heartbeat #1 (tick=100)
[WorkerB] Heartbeat #1 (tick=100)
[WorkerC] Heartbeat #1 (tick=100)
...
[WDG ] --- Health check ---
[WDG ] WorkerA OK (last beat 900 ms ago, beats=3)
[WDG ] WorkerB OK (last beat 900 ms ago, beats=3)
[WDG ] WorkerC OK (last beat 900 ms ago, beats=3)
...
[WorkerC] !!! STALLING NOW (simulated bug) !!!
...
[WDG ] --- Health check ---
[WDG ] WorkerA OK (last beat 800 ms ago, beats=13)
[WDG ] WorkerB OK (last beat 800 ms ago, beats=13)
[WDG ] ALERT: WorkerC missed heartbeat! Last beat 3200 ms ago (timeout=2500 ms)
[WDG ] Deleting stalled task: WorkerC
[WDG ] RECOVERED: WorkerC recreated at tick=16000

[WorkerC] Started at tick=16000
[WorkerC] Heartbeat #1 (tick=17000)
```

Worker C stalls after ~10 seconds. The watchdog detects the missing heartbeat at
its next 3-second check and recreates the task. Worker C resumes with fresh state.

## Complete Source Code

```c
   1  /**
   2    * Lab 7: Watchdog Task (Software Supervisor)
   3    * --------------------------------------------
   4    * Three worker tasks send periodic heartbeats to a watchdog task.
   5    * Worker C is programmed to stall after 10 seconds (simulates a bug).
   6    * The watchdog detects the missing heartbeat, prints an alert,
   7    * deletes the stalled task, and recreates it.
   8    *
   9    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  10    */
  11  
  12  #include "main.h"
  13  #include "FreeRTOS.h"
  14  #include "task.h"
  15  #include <stdio.h>
  16  #include <string.h>
  17  
  18  #define LED_PIN      GPIO_PIN_5
  19  #define LED_PORT     GPIOA
  20  #define NUM_WORKERS  3
  21  
  22  UART_HandleTypeDef huart2;
  23  
  24  /* ---- Worker tracking ---- */
  25  typedef struct {
  26      TaskHandle_t handle;
  27      const char  *name;
  28      uint32_t     lastBeat;     /* tick of last heartbeat */
  29      uint32_t     interval_ms;  /* expected heartbeat interval */
  30      uint32_t     beatCount;
  31      uint8_t      shouldStall;  /* 1 = this worker will stall */
  32  } WorkerInfo;
  33  
  34  static WorkerInfo workers[NUM_WORKERS];
  35  TaskHandle_t watchdogHandle;
  36  
  37  static void uart_print(const char *msg)
  38  {
  39      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  40  }
  41  
  42  /* ================================================================
  43   * Worker task
  44   *
  45   * Sends a heartbeat (xTaskNotifyGive) to the watchdog every second.
  46   * Worker C stalls after 10 seconds to simulate a bug.
  47   * ================================================================ */
  48  static void Worker_Task(void *arg)
  49  {
  50      WorkerInfo *info = (WorkerInfo *)arg;
  51      char buf[100];
  52      uint32_t startTick = HAL_GetTick();
  53  
  54      snprintf(buf, sizeof(buf),
  55               "[%s] Started at tick=%lu\r\n",
  56               info->name, (unsigned long)startTick);
  57      uart_print(buf);
  58  
  59      for (;;)
  60      {
  61          /* Check if this worker should stall */
  62          if (info->shouldStall && (HAL_GetTick() - startTick > 10000))
  63          {
  64              snprintf(buf, sizeof(buf),
  65                       "[%s] !!! STALLING NOW (simulated bug) !!!\r\n",
  66                       info->name);
  67              uart_print(buf);
  68  
  69              /* Infinite loop = stalled task */
  70              for (;;)
  71              {
  72                  vTaskDelay(portMAX_DELAY);
  73              }
  74          }
  75  
  76          /* Normal operation: send heartbeat */
  77          info->beatCount++;
  78          info->lastBeat = HAL_GetTick();
  79  
  80          /* Notify the watchdog */
  81          xTaskNotifyGive(watchdogHandle);
  82  
  83          snprintf(buf, sizeof(buf),
  84                   "[%s] Heartbeat #%lu (tick=%lu)\r\n",
  85                   info->name,
  86                   (unsigned long)info->beatCount,
  87                   (unsigned long)info->lastBeat);
  88          uart_print(buf);
  89  
  90          vTaskDelay(pdMS_TO_TICKS(info->interval_ms));
  91      }
  92  }
  93  
  94  /* ================================================================
  95   * Watchdog task
  96   *
  97   * Checks every 3 seconds whether each worker sent a heartbeat
  98   * within its expected interval (with some tolerance).
  99   * If a worker missed its deadline, the watchdog logs an alert,
 100   * deletes the stalled task, and recreates it.
 101   * ================================================================ */
 102  static void Watchdog_Task(void *arg)
 103  {
 104      char buf[120];
 105      uint32_t checkInterval = 3000;  /* check every 3 seconds */
 106      (void)arg;
 107  
 108      uart_print("[WDG ] Watchdog started. Checking every 3 seconds.\r\n\r\n");
 109  
 110      /* Let workers start first */
 111      vTaskDelay(pdMS_TO_TICKS(2000));
 112  
 113      for (;;)
 114      {
 115          vTaskDelay(pdMS_TO_TICKS(checkInterval));
 116  
 117          uint32_t now = HAL_GetTick();
 118          uart_print("[WDG ] --- Health check ---\r\n");
 119  
 120          for (int i = 0; i < NUM_WORKERS; i++)
 121          {
 122              uint32_t elapsed = now - workers[i].lastBeat;
 123              /* Allow 2x the interval as tolerance */
 124              uint32_t timeout = workers[i].interval_ms * 2 + 500;
 125  
 126              if (elapsed > timeout)
 127              {
 128                  /* STALLED! */
 129                  snprintf(buf, sizeof(buf),
 130                           "[WDG ] ALERT: %s missed heartbeat! "
 131                           "Last beat %lu ms ago (timeout=%lu ms)\r\n",
 132                           workers[i].name,
 133                           (unsigned long)elapsed,
 134                           (unsigned long)timeout);
 135                  uart_print(buf);
 136  
 137                  /* Delete the stalled task */
 138                  if (workers[i].handle != NULL)
 139                  {
 140                      snprintf(buf, sizeof(buf),
 141                               "[WDG ] Deleting stalled task: %s\r\n",
 142                               workers[i].name);
 143                      uart_print(buf);
 144                      vTaskDelete(workers[i].handle);
 145                      workers[i].handle = NULL;
 146                  }
 147  
 148                  /* Recreate the worker */
 149                  workers[i].lastBeat    = HAL_GetTick();
 150                  workers[i].beatCount   = 0;
 151                  workers[i].shouldStall = 0;  /* don't stall again */
 152  
 153                  xTaskCreate(Worker_Task,
 154                              workers[i].name,
 155                              256,
 156                              &workers[i],
 157                              1,
 158                              &workers[i].handle);
 159  
 160                  snprintf(buf, sizeof(buf),
 161                           "[WDG ] RECOVERED: %s recreated at tick=%lu\r\n\r\n",
 162                           workers[i].name,
 163                           (unsigned long)HAL_GetTick());
 164                  uart_print(buf);
 165  
 166                  HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 167              }
 168              else
 169              {
 170                  snprintf(buf, sizeof(buf),
 171                           "[WDG ] %s OK (last beat %lu ms ago, beats=%lu)\r\n",
 172                           workers[i].name,
 173                           (unsigned long)elapsed,
 174                           (unsigned long)workers[i].beatCount);
 175                  uart_print(buf);
 176              }
 177          }
 178          uart_print("\r\n");
 179      }
 180  }
 181  
 182  /* ---- Peripheral init ---- */
 183  static void GPIO_Init(void);
 184  static void UART2_Init(void);
 185  static void SystemClock_Config(void);
 186  
 187  int main(void)
 188  {
 189      HAL_Init();
 190      SystemClock_Config();
 191      GPIO_Init();
 192      UART2_Init();
 193  
 194      uart_print("\r\n=============================================\r\n");
 195      uart_print("  Lab 7: Watchdog Task\r\n");
 196      uart_print("=============================================\r\n");
 197      uart_print("  3 workers send heartbeats every 1 second\r\n");
 198      uart_print("  Watchdog checks health every 3 seconds\r\n");
 199      uart_print("  Worker C stalls at 10 seconds (bug)\r\n");
 200      uart_print("  Watchdog detects, kills, and restarts it\r\n");
 201      uart_print("=============================================\r\n\r\n");
 202  
 203      /* Configure workers */
 204      workers[0] = (WorkerInfo){
 205          .name = "WorkerA", .interval_ms = 1000,
 206          .shouldStall = 0, .lastBeat = 0, .beatCount = 0
 207      };
 208      workers[1] = (WorkerInfo){
 209          .name = "WorkerB", .interval_ms = 1000,
 210          .shouldStall = 0, .lastBeat = 0, .beatCount = 0
 211      };
 212      workers[2] = (WorkerInfo){
 213          .name = "WorkerC", .interval_ms = 1000,
 214          .shouldStall = 1, .lastBeat = 0, .beatCount = 0
 215          /* ^^^ This worker WILL stall after 10 seconds */
 216      };
 217  
 218      /* Create watchdog first (highest priority) */
 219      xTaskCreate(Watchdog_Task, "Watchdog", 512, NULL, 3, &watchdogHandle);
 220  
 221      /* Create workers */
 222      for (int i = 0; i < NUM_WORKERS; i++)
 223      {
 224          xTaskCreate(Worker_Task,
 225                      workers[i].name,
 226                      256,
 227                      &workers[i],
 228                      1,
 229                      &workers[i].handle);
 230      }
 231  
 232      vTaskStartScheduler();
 233      for (;;);
 234  }
 235  
 236  static void GPIO_Init(void)
 237  {
 238      GPIO_InitTypeDef gpio = {0};
 239      __HAL_RCC_GPIOA_CLK_ENABLE();
 240      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 241      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 242      HAL_GPIO_Init(LED_PORT, &gpio);
 243  }
 244  
 245  static void UART2_Init(void)
 246  {
 247      GPIO_InitTypeDef gpio = {0};
 248      __HAL_RCC_USART2_CLK_ENABLE();
 249      __HAL_RCC_GPIOA_CLK_ENABLE();
 250      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 251      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 252      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 253      HAL_GPIO_Init(GPIOA, &gpio);
 254      huart2.Instance = USART2;
 255      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 256      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 257      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 258      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 259      HAL_UART_Init(&huart2);
 260  }
 261  
 262  static void SystemClock_Config(void)
 263  {
 264      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 265      __HAL_RCC_PWR_CLK_ENABLE();
 266      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 267      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 268      osc.HSEState = RCC_HSE_BYPASS;
 269      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 270      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 271      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 272      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 273      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 274      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 275      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 276      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 277      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 278  }
 279  
 280  #ifdef USE_FULL_ASSERT
 281  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 282  #endif
 283  
```
