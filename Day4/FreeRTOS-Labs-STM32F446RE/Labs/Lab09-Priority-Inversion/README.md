# Lab 09: Priority Inversion

## Board
STM32 Nucleo-F446RE

## Concept

Priority inversion: a high-priority task waits for a low-priority task, but a
medium-priority task prevents the low-priority task from running.

```
    WITHOUT inheritance (broken):          WITH inheritance (fixed):

    Priority                               Priority
    HIGH ─┐ request lock                   HIGH ─┐ request lock
          │ BLOCKED (L holds it)                 │ BLOCKED → boost L
          │                                      │
    MED  ─┤ runs freely (no lock)          MED  ─┤ BLOCKED (L now has
          │ preempts L for 5 sec                 │ HIGH priority)
          │                                      │
    LOW  ─┤ holds lock                     LOW  ─┤ holds lock (boosted)
          │ cannot run (M preempts)              │ runs at HIGH priority
          │                                      │ finishes in 3 sec
          ▼                                      ▼
    H waits 7+ seconds                    H waits ~3 seconds
```

## How to Build and Run

```bash
cd Lab09-Priority-Inversion
make clean
make
make flash
```

## Toggle Between Broken and Fixed

Edit `Src/main.c` line 29:
```c
#define USE_MUTEX  0    // binary semaphore, NO inheritance (inversion)
#define USE_MUTEX  1    // mutex WITH inheritance (fixed)
```

## Expected Output

### With USE_MUTEX 0 (inversion):
```
[Low ] GOT LOCK at tick=2. Doing slow work...
[Low ] ...working (1/4 sec done)
[Med ] STARTING CPU-bound work (5 seconds)
[HIGH] REQUESTING lock at tick=1002...
[Med ] ...running (1/5 sec)
[Med ] ...running (2/5 sec)
[Med ] ...running (3/5 sec)
[Med ] ...running (4/5 sec)
[Med ] ...running (5/5 sec)
[Med ] DONE
[Low ] ...working (2/4 sec done)
[Low ] ...working (3/4 sec done)
[Low ] ...working (4/4 sec done)
[Low ] RELEASING lock (held 8500 ms)
[HIGH] *** GOT LOCK. WAITED 7500 ms ***
[HIGH] -> PRIORITY INVERSION!
```

### With USE_MUTEX 1 (fixed):
```
[Low ] GOT LOCK at tick=2. Doing slow work...
[Low ] ...working (1/4 sec done)
[HIGH] REQUESTING lock at tick=1002...
[Low ] ...working (2/4 sec done)
[Low ] ...working (3/4 sec done)
[Low ] ...working (4/4 sec done)
[Low ] RELEASING lock (held 4000 ms)
[HIGH] *** GOT LOCK. WAITED 3000 ms ***
[HIGH] -> Priority inheritance worked!
```

Notice that Medium does not appear between Low's progress messages in the fixed version.
Low was boosted to High priority and preempted Medium.

## Complete Source Code

```c
   1  /**
   2    * Lab: Priority Inversion
   3    * ------------------------
   4    * NUCLEO-F446RE + FreeRTOS
   5    *
   6    * Three tasks:
   7    *   Task_H  (HIGH   priority) - wants the shared lock
   8    *   Task_M  (MEDIUM priority) - CPU-bound work, no lock needed
   9    *   Task_L  (LOW    priority) - holds the shared lock, does slow work
  10    *
  11    * BUILD MODE:
  12    *   #define USE_MUTEX  0   -> binary semaphore, NO priority inheritance
  13    *                            (reproduces priority inversion)
  14    *   #define USE_MUTEX  1   -> mutex WITH priority inheritance
  15    *                            (fixes the inversion)
  16    *
  17    * Build both versions and compare the UART output.
  18    *
  19    * UART2 at 115200 baud on /dev/ttyACM0
  20    *   minicom -D /dev/ttyACM0 -b 115200
  21    */
  22  
  23  #include "main.h"
  24  #include "FreeRTOS.h"
  25  #include "task.h"
  26  #include "semphr.h"
  27  #include <stdio.h>
  28  #include <string.h>
  29  
  30  /* ================================================================
  31   * CHANGE THIS TO SWITCH BETWEEN BROKEN AND FIXED BEHAVIOR
  32   *   0 = binary semaphore (priority inversion happens)
  33   *   1 = mutex with priority inheritance (inversion fixed)
  34   * ================================================================ */
  35  #define USE_MUTEX  0
  36  
  37  /* ---- LED: PA5 (LD2) ---- */
  38  #define LED_PIN      GPIO_PIN_5
  39  #define LED_PORT     GPIOA
  40  
  41  /* ---- UART ---- */
  42  UART_HandleTypeDef huart2;
  43  
  44  /* ---- Shared lock ---- */
  45  SemaphoreHandle_t sharedLock;
  46  
  47  /* ---- Task handles ---- */
  48  TaskHandle_t hTaskH, hTaskM, hTaskL;
  49  
  50  /* ---- Prototypes ---- */
  51  static void Task_High(void *arg);
  52  static void Task_Medium(void *arg);
  53  static void Task_Low(void *arg);
  54  static void SystemClock_Config(void);
  55  static void GPIO_Init(void);
  56  static void UART2_Init(void);
  57  static void uart_print(const char *msg);
  58  static void busy_wait_ms(uint32_t ms);
  59  
  60  /* ================================================================
  61   * main
  62   * ================================================================ */
  63  int main(void)
  64  {
  65      HAL_Init();
  66      SystemClock_Config();
  67      GPIO_Init();
  68      UART2_Init();
  69  
  70      uart_print("\r\n=============================================\r\n");
  71      uart_print("  FreeRTOS Priority Inversion Lab\r\n");
  72      uart_print("  Nucleo-F446RE\r\n");
  73  #if USE_MUTEX
  74      uart_print("  Mode: MUTEX (priority inheritance ON)\r\n");
  75  #else
  76      uart_print("  Mode: BINARY SEMAPHORE (NO inheritance)\r\n");
  77  #endif
  78      uart_print("=============================================\r\n");
  79      uart_print("  Task_H = HIGH   priority (wants lock)\r\n");
  80      uart_print("  Task_M = MEDIUM priority (no lock, CPU hog)\r\n");
  81      uart_print("  Task_L = LOW    priority (holds lock)\r\n");
  82      uart_print("=============================================\r\n\r\n");
  83  
  84      /* Create the shared lock */
  85  #if USE_MUTEX
  86      sharedLock = xSemaphoreCreateMutex();
  87      uart_print("[INIT] Created MUTEX (priority inheritance enabled)\r\n\r\n");
  88  #else
  89      sharedLock = xSemaphoreCreateBinary();
  90      xSemaphoreGive(sharedLock);  /* binary semaphore starts empty, must give first */
  91      uart_print("[INIT] Created BINARY SEMAPHORE (no inheritance)\r\n\r\n");
  92  #endif
  93  
  94      /*
  95       * Task_L starts first and grabs the lock.
  96       * Task_M and Task_H start after short delays so the
  97       * inversion scenario plays out in the right order.
  98       */
  99      xTaskCreate(Task_Low,    "Low",    512, NULL, 1, &hTaskL);   /* lowest  */
 100      xTaskCreate(Task_Medium, "Medium", 512, NULL, 2, &hTaskM);   /* medium  */
 101      xTaskCreate(Task_High,   "High",   512, NULL, 3, &hTaskH);   /* highest */
 102  
 103      vTaskStartScheduler();
 104  
 105      for (;;);
 106  }
 107  
 108  /* ================================================================
 109   * Task_Low (priority 1)
 110   *
 111   * Takes the shared lock, then does slow busy work for ~4 seconds
 112   * while holding it. In the broken case, Task_M preempts this
 113   * task, delaying lock release and starving Task_H.
 114   * ================================================================ */
 115  static void Task_Low(void *arg)
 116  {
 117      char buf[120];
 118      (void)arg;
 119  
 120      for (;;)
 121      {
 122          uart_print("[Low ] Trying to take lock...\r\n");
 123          xSemaphoreTake(sharedLock, portMAX_DELAY);
 124  
 125          uint32_t t0 = HAL_GetTick();
 126          snprintf(buf, sizeof(buf),
 127                   "[Low ] GOT LOCK at tick=%lu. Doing slow work (4 seconds)...\r\n",
 128                   (unsigned long)t0);
 129          uart_print(buf);
 130  
 131          /*
 132           * Simulate slow critical section work.
 133           * Print a status dot each second so you can see
 134           * when this task actually runs vs when it's preempted.
 135           */
 136          for (int i = 0; i < 4; i++)
 137          {
 138              busy_wait_ms(1000);
 139              snprintf(buf, sizeof(buf),
 140                       "[Low ] ...working (%d/4 sec done, tick=%lu)\r\n",
 141                       i + 1, (unsigned long)HAL_GetTick());
 142              uart_print(buf);
 143          }
 144  
 145          uint32_t t1 = HAL_GetTick();
 146          snprintf(buf, sizeof(buf),
 147                   "[Low ] RELEASING lock at tick=%lu (held %lu ms)\r\n\r\n",
 148                   (unsigned long)t1, (unsigned long)(t1 - t0));
 149          uart_print(buf);
 150  
 151          xSemaphoreGive(sharedLock);
 152  
 153          /* Wait before repeating the cycle */
 154          vTaskDelay(pdMS_TO_TICKS(3000));
 155      }
 156  }
 157  
 158  /* ================================================================
 159   * Task_Medium (priority 2)
 160   *
 161   * Starts 500 ms after boot. Does NOT use the lock at all.
 162   * Burns CPU for ~5 seconds with busy work.
 163   *
 164   * In the broken case, this task preempts Task_Low because it
 165   * has higher priority, preventing Low from releasing the lock,
 166   * which in turn blocks Task_High. This IS the priority inversion.
 167   *
 168   * In the fixed case, Task_Low gets boosted to Task_High's
 169   * priority when High blocks on the mutex, so Medium CANNOT
 170   * preempt Low.
 171   * ================================================================ */
 172  static void Task_Medium(void *arg)
 173  {
 174      char buf[120];
 175      (void)arg;
 176  
 177      /* Let Task_Low grab the lock first */
 178      vTaskDelay(pdMS_TO_TICKS(500));
 179  
 180      for (;;)
 181      {
 182          uint32_t t0 = HAL_GetTick();
 183          snprintf(buf, sizeof(buf),
 184                   "[Med ] STARTING CPU-bound work at tick=%lu (5 seconds, no lock)\r\n",
 185                   (unsigned long)t0);
 186          uart_print(buf);
 187  
 188          for (int i = 0; i < 5; i++)
 189          {
 190              busy_wait_ms(1000);
 191              snprintf(buf, sizeof(buf),
 192                       "[Med ] ...running (%d/5 sec, tick=%lu)\r\n",
 193                       i + 1, (unsigned long)HAL_GetTick());
 194              uart_print(buf);
 195          }
 196  
 197          uint32_t t1 = HAL_GetTick();
 198          snprintf(buf, sizeof(buf),
 199                   "[Med ] DONE at tick=%lu (ran %lu ms)\r\n\r\n",
 200                   (unsigned long)t1, (unsigned long)(t1 - t0));
 201          uart_print(buf);
 202  
 203          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 204  
 205          vTaskDelay(pdMS_TO_TICKS(3000));
 206      }
 207  }
 208  
 209  /* ================================================================
 210   * Task_High (priority 3)
 211   *
 212   * Starts 1000 ms after boot, when Task_Low already holds the lock
 213   * and Task_Medium is busy hogging the CPU.
 214   *
 215   * Tries to take the lock and measures how long it waits.
 216   *
 217   * WITHOUT inheritance: waits for Medium to finish + Low to finish
 218   * WITH    inheritance: Low gets boosted, preempts Medium, finishes fast
 219   * ================================================================ */
 220  static void Task_High(void *arg)
 221  {
 222      char buf[120];
 223      (void)arg;
 224  
 225      /* Let Task_Low grab the lock and Task_Medium start running */
 226      vTaskDelay(pdMS_TO_TICKS(1000));
 227  
 228      for (;;)
 229      {
 230          uint32_t t0 = HAL_GetTick();
 231          snprintf(buf, sizeof(buf),
 232                   "[HIGH] REQUESTING lock at tick=%lu...\r\n",
 233                   (unsigned long)t0);
 234          uart_print(buf);
 235  
 236          /* This blocks until Task_Low releases the lock */
 237          xSemaphoreTake(sharedLock, portMAX_DELAY);
 238  
 239          uint32_t t1 = HAL_GetTick();
 240          snprintf(buf, sizeof(buf),
 241                   "[HIGH] *** GOT LOCK at tick=%lu.  WAITED %lu ms ***\r\n",
 242                   (unsigned long)t1, (unsigned long)(t1 - t0));
 243          uart_print(buf);
 244  
 245  #if USE_MUTEX
 246          if ((t1 - t0) < 4000)
 247              uart_print("[HIGH] -> Priority inheritance worked! Low preempted Medium.\r\n\r\n");
 248          else
 249              uart_print("[HIGH] -> Unexpected long wait even with mutex.\r\n\r\n");
 250  #else
 251          if ((t1 - t0) > 5000)
 252              uart_print("[HIGH] -> PRIORITY INVERSION! Medium delayed Low, which blocked us.\r\n\r\n");
 253          else
 254              uart_print("[HIGH] -> Got lock without much inversion.\r\n\r\n");
 255  #endif
 256  
 257          xSemaphoreGive(sharedLock);
 258  
 259          vTaskDelay(pdMS_TO_TICKS(5000));
 260      }
 261  }
 262  
 263  /* ================================================================
 264   * Busy-wait (does NOT yield the CPU)
 265   *
 266   * Uses HAL_GetTick which increments from the SysTick ISR.
 267   * The task stays on the CPU the whole time, which is what
 268   * makes preemption visible.
 269   * ================================================================ */
 270  static void busy_wait_ms(uint32_t ms)
 271  {
 272      uint32_t start = HAL_GetTick();
 273      while ((HAL_GetTick() - start) < ms)
 274      {
 275          /* spin */
 276      }
 277  }
 278  
 279  /* ================================================================
 280   * UART helper
 281   * ================================================================ */
 282  static void uart_print(const char *msg)
 283  {
 284      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
 285  }
 286  
 287  /* ================================================================
 288   * Peripheral init
 289   * ================================================================ */
 290  static void GPIO_Init(void)
 291  {
 292      GPIO_InitTypeDef gpio = {0};
 293      __HAL_RCC_GPIOA_CLK_ENABLE();
 294  
 295      gpio.Pin   = LED_PIN;
 296      gpio.Mode  = GPIO_MODE_OUTPUT_PP;
 297      gpio.Pull  = GPIO_NOPULL;
 298      gpio.Speed = GPIO_SPEED_FREQ_LOW;
 299      HAL_GPIO_Init(LED_PORT, &gpio);
 300  }
 301  
 302  static void UART2_Init(void)
 303  {
 304      GPIO_InitTypeDef gpio = {0};
 305  
 306      __HAL_RCC_USART2_CLK_ENABLE();
 307      __HAL_RCC_GPIOA_CLK_ENABLE();
 308  
 309      gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
 310      gpio.Mode      = GPIO_MODE_AF_PP;
 311      gpio.Pull      = GPIO_PULLUP;
 312      gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
 313      gpio.Alternate = GPIO_AF7_USART2;
 314      HAL_GPIO_Init(GPIOA, &gpio);
 315  
 316      huart2.Instance          = USART2;
 317      huart2.Init.BaudRate     = 115200;
 318      huart2.Init.WordLength   = UART_WORDLENGTH_8B;
 319      huart2.Init.StopBits     = UART_STOPBITS_1;
 320      huart2.Init.Parity       = UART_PARITY_NONE;
 321      huart2.Init.Mode         = UART_MODE_TX_RX;
 322      huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
 323      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 324      HAL_UART_Init(&huart2);
 325  }
 326  
 327  static void SystemClock_Config(void)
 328  {
 329      RCC_ClkInitTypeDef clk = {0};
 330      RCC_OscInitTypeDef osc = {0};
 331  
 332      __HAL_RCC_PWR_CLK_ENABLE();
 333      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 334  
 335      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 336      osc.HSEState       = RCC_HSE_BYPASS;
 337      osc.PLL.PLLState   = RCC_PLL_ON;
 338      osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
 339      osc.PLL.PLLM       = 8;
 340      osc.PLL.PLLN       = 360;
 341      osc.PLL.PLLP       = RCC_PLLP_DIV2;
 342      osc.PLL.PLLQ       = 7;
 343      osc.PLL.PLLR       = 2;
 344      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for (;;);
 345      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for (;;);
 346  
 347      clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
 348                            RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
 349      clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
 350      clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
 351      clk.APB1CLKDivider = RCC_HCLK_DIV4;
 352      clk.APB2CLKDivider = RCC_HCLK_DIV2;
 353      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for (;;);
 354  }
 355  
 356  #ifdef USE_FULL_ASSERT
 357  void assert_failed(uint8_t *file, uint32_t line) { while (1); }
 358  #endif
 359  
```
