# Lab 06: Deadlock

## Board
STM32 Nucleo-F446RE

## Concept

Deadlock occurs when two tasks each hold a resource the other needs, and neither can proceed.

```
    DEADLOCK (opposite lock order):

    Task 1                    Task 2
    ──────                    ──────
    take Mutex A  ✓           take Mutex B  ✓
       │                         │
    take Mutex B  ✗ BLOCKED   take Mutex A  ✗ BLOCKED
       │                         │
       └─── waiting for T2 ─────┘
             FOREVER

    FIXED (same lock order):

    Task 1                    Task 2
    ──────                    ──────
    take Mutex A  ✓           take Mutex A  ✗ waits...
    take Mutex B  ✓              │
    use both                     │
    give Mutex B              ...wakes up
    give Mutex A              take Mutex A  ✓
                              take Mutex B  ✓
                              use both
                              give Mutex B
                              give Mutex A
```

### Four Conditions (ALL must be true):
1. Mutual exclusion: resources used by one task at a time
2. Hold and wait: hold one resource while waiting for another
3. No preemption: cannot forcibly take a resource
4. Circular wait: T1 waits for T2, T2 waits for T1

### Fix: break any one condition. Easiest: always lock in the same order.

## How to Build and Run

```bash
cd Lab06-Deadlock
make clean
make
make flash
```

Reset: press the black RESET button or run `st-flash reset`.

## Toggle Between Deadlock and Fixed

Edit `Src/main.c` line 22:
```c
#define CONSISTENT_ORDER  0    // deadlock (output will freeze)
#define CONSISTENT_ORDER  1    // fixed (runs forever)
```

## Expected Output

### With CONSISTENT_ORDER 0 (deadlock):
```
[T1] Cycle 1: taking Mutex A...
[T1] Got Mutex A.
[T2] Cycle 1: taking Mutex B... (OPPOSITE order!)
[T2] Got Mutex B.
[T1] Taking Mutex B...
[T2] Taking Mutex A...
```
**Output stops here.** No more text appears. LED freezes. The [BEAT] heartbeat
messages stop. That silence IS the deadlock. Both tasks are blocked forever.

### With CONSISTENT_ORDER 1 (fixed):
```
[T1] Cycle 1: taking Mutex A...
[T1] Got Mutex A.
[T1] Taking Mutex B...
[T1] Got Mutex B.
[T1] *** Working with both resources (cycle 1) ***
[T1] Released both mutexes.

[T2] Cycle 1: taking Mutex A... (same order as T1)
[T2] Got Mutex A.
[T2] Taking Mutex B...
[T2] Got Mutex B.
[T2] *** Working with both resources (cycle 1) ***
[T2] Released both mutexes.

[BEAT] System alive.
```
Output continues indefinitely. The heartbeat keeps printing.

## Complete Source Code

```c
   1  /**
   2    * Lab 6: Deadlock
   3    * ----------------
   4    * Two tasks, two mutexes, opposite locking order = deadlock.
   5    *
   6    * #define CONSISTENT_ORDER 0  -> deadlock (system freezes)
   7    * #define CONSISTENT_ORDER 1  -> fixed (same lock order, runs forever)
   8    *
   9    * When deadlock occurs, UART output stops and LED freezes.
  10    * That silence IS the result.
  11    *
  12    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  13    */
  14  
  15  #include "main.h"
  16  #include "FreeRTOS.h"
  17  #include "task.h"
  18  #include "semphr.h"
  19  #include <stdio.h>
  20  #include <string.h>
  21  
  22  /* ================================================================
  23   * CHANGE THIS TO SWITCH BETWEEN DEADLOCK AND FIXED
  24   *   0 = opposite lock order (DEADLOCK)
  25   *   1 = same lock order     (FIXED)
  26   * ================================================================ */
  27  #define CONSISTENT_ORDER  0
  28  
  29  #define LED_PIN      GPIO_PIN_5
  30  #define LED_PORT     GPIOA
  31  
  32  UART_HandleTypeDef huart2;
  33  SemaphoreHandle_t mutexA;
  34  SemaphoreHandle_t mutexB;
  35  
  36  static void uart_print(const char *msg)
  37  {
  38      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  39  }
  40  
  41  /**
  42    * Task 1: locks A then B (always)
  43    */
  44  static void Task1(void *arg)
  45  {
  46      uint32_t count = 0;
  47      char buf[100];
  48      (void)arg;
  49  
  50      for (;;)
  51      {
  52          count++;
  53          snprintf(buf, sizeof(buf),
  54                   "[T1] Cycle %lu: taking Mutex A...\r\n",
  55                   (unsigned long)count);
  56          uart_print(buf);
  57  
  58          xSemaphoreTake(mutexA, portMAX_DELAY);
  59          uart_print("[T1] Got Mutex A.\r\n");
  60  
  61          /* Small delay to increase chance of interleaving */
  62          vTaskDelay(pdMS_TO_TICKS(10));
  63  
  64          uart_print("[T1] Taking Mutex B...\r\n");
  65          xSemaphoreTake(mutexB, portMAX_DELAY);
  66          uart_print("[T1] Got Mutex B.\r\n");
  67  
  68          /* Critical section: use both resources */
  69          snprintf(buf, sizeof(buf),
  70                   "[T1] *** Working with both resources (cycle %lu) ***\r\n",
  71                   (unsigned long)count);
  72          uart_print(buf);
  73          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
  74  
  75          xSemaphoreGive(mutexB);
  76          xSemaphoreGive(mutexA);
  77          uart_print("[T1] Released both mutexes.\r\n\r\n");
  78  
  79          vTaskDelay(pdMS_TO_TICKS(200));
  80      }
  81  }
  82  
  83  /**
  84    * Task 2:
  85    *   CONSISTENT_ORDER = 0 -> locks B then A (OPPOSITE order = deadlock)
  86    *   CONSISTENT_ORDER = 1 -> locks A then B (SAME order = safe)
  87    */
  88  static void Task2(void *arg)
  89  {
  90      uint32_t count = 0;
  91      char buf[100];
  92      (void)arg;
  93  
  94      for (;;)
  95      {
  96          count++;
  97  
  98  #if CONSISTENT_ORDER
  99          /* FIXED: same order as Task 1 (A first, then B) */
 100          snprintf(buf, sizeof(buf),
 101                   "[T2] Cycle %lu: taking Mutex A... (same order as T1)\r\n",
 102                   (unsigned long)count);
 103          uart_print(buf);
 104  
 105          xSemaphoreTake(mutexA, portMAX_DELAY);
 106          uart_print("[T2] Got Mutex A.\r\n");
 107  
 108          vTaskDelay(pdMS_TO_TICKS(10));
 109  
 110          uart_print("[T2] Taking Mutex B...\r\n");
 111          xSemaphoreTake(mutexB, portMAX_DELAY);
 112          uart_print("[T2] Got Mutex B.\r\n");
 113  #else
 114          /* BROKEN: opposite order (B first, then A) */
 115          snprintf(buf, sizeof(buf),
 116                   "[T2] Cycle %lu: taking Mutex B... (OPPOSITE order!)\r\n",
 117                   (unsigned long)count);
 118          uart_print(buf);
 119  
 120          xSemaphoreTake(mutexB, portMAX_DELAY);
 121          uart_print("[T2] Got Mutex B.\r\n");
 122  
 123          vTaskDelay(pdMS_TO_TICKS(10));
 124  
 125          uart_print("[T2] Taking Mutex A...\r\n");
 126          xSemaphoreTake(mutexA, portMAX_DELAY);
 127          uart_print("[T2] Got Mutex A.\r\n");
 128  #endif
 129  
 130          snprintf(buf, sizeof(buf),
 131                   "[T2] *** Working with both resources (cycle %lu) ***\r\n",
 132                   (unsigned long)count);
 133          uart_print(buf);
 134  
 135          xSemaphoreGive(mutexB);
 136          xSemaphoreGive(mutexA);
 137          uart_print("[T2] Released both mutexes.\r\n\r\n");
 138  
 139          vTaskDelay(pdMS_TO_TICKS(200));
 140      }
 141  }
 142  
 143  /* ---- Heartbeat task: proves the system is alive ---- */
 144  static void Heartbeat_Task(void *arg)
 145  {
 146      (void)arg;
 147      for (;;)
 148      {
 149          uart_print("[BEAT] System alive.\r\n");
 150          vTaskDelay(pdMS_TO_TICKS(2000));
 151      }
 152  }
 153  
 154  /* ---- Peripheral init ---- */
 155  static void GPIO_Init(void);
 156  static void UART2_Init(void);
 157  static void SystemClock_Config(void);
 158  
 159  int main(void)
 160  {
 161      HAL_Init();
 162      SystemClock_Config();
 163      GPIO_Init();
 164      UART2_Init();
 165  
 166      uart_print("\r\n=============================================\r\n");
 167      uart_print("  Lab 6: Deadlock\r\n");
 168  #if CONSISTENT_ORDER
 169      uart_print("  Mode: CONSISTENT ORDER (no deadlock)\r\n");
 170  #else
 171      uart_print("  Mode: OPPOSITE ORDER (deadlock will occur!)\r\n");
 172  #endif
 173      uart_print("=============================================\r\n");
 174      uart_print("  T1: locks A then B\r\n");
 175  #if CONSISTENT_ORDER
 176      uart_print("  T2: locks A then B (same order = safe)\r\n");
 177  #else
 178      uart_print("  T2: locks B then A (opposite = DEADLOCK)\r\n");
 179  #endif
 180      uart_print("  Heartbeat prints every 2 s (stops on deadlock)\r\n");
 181      uart_print("=============================================\r\n\r\n");
 182  
 183      mutexA = xSemaphoreCreateMutex();
 184      mutexB = xSemaphoreCreateMutex();
 185  
 186      xTaskCreate(Task1,          "T1",    512, NULL, 1, NULL);
 187      xTaskCreate(Task2,          "T2",    512, NULL, 1, NULL);
 188      xTaskCreate(Heartbeat_Task, "Beat",  256, NULL, 1, NULL);
 189  
 190      vTaskStartScheduler();
 191      for (;;);
 192  }
 193  
 194  static void GPIO_Init(void)
 195  {
 196      GPIO_InitTypeDef gpio = {0};
 197      __HAL_RCC_GPIOA_CLK_ENABLE();
 198      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 199      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 200      HAL_GPIO_Init(LED_PORT, &gpio);
 201  }
 202  
 203  static void UART2_Init(void)
 204  {
 205      GPIO_InitTypeDef gpio = {0};
 206      __HAL_RCC_USART2_CLK_ENABLE();
 207      __HAL_RCC_GPIOA_CLK_ENABLE();
 208      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 209      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 210      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 211      HAL_GPIO_Init(GPIOA, &gpio);
 212      huart2.Instance = USART2;
 213      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 214      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 215      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 216      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 217      HAL_UART_Init(&huart2);
 218  }
 219  
 220  static void SystemClock_Config(void)
 221  {
 222      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 223      __HAL_RCC_PWR_CLK_ENABLE();
 224      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 225      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 226      osc.HSEState = RCC_HSE_BYPASS;
 227      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 228      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 229      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 230      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 231      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 232      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 233      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 234      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 235      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 236  }
 237  
 238  #ifdef USE_FULL_ASSERT
 239  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 240  #endif
 241  
```
