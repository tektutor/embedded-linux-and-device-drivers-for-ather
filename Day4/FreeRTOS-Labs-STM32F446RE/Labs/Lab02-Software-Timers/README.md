# Lab 02: Software Timers

## Board
STM32 Nucleo-F446RE

## Concept

A software timer calls a function at a scheduled time without needing a dedicated task.
FreeRTOS runs all timer callbacks inside a hidden "timer daemon task" that the kernel
creates automatically.

```
    ┌──────────────────────────────────────────────┐
    │          FreeRTOS Timer Daemon Task           │
    │   (created internally by the kernel)         │
    │                                              │
    │   ┌─────────┐ ┌──────────┐ ┌─────────────┐  │
    │   │ LED     │ │ Print    │ │ One-shot    │  │
    │   │ 500ms   │ │ 2000ms   │ │ fires once  │  │
    │   │ periodic│ │ periodic │ │ at 5000ms   │  │
    │   └─────────┘ └──────────┘ └─────────────┘  │
    └──────────────────────────────────────────────┘
          │              │              │
          ▼              ▼              ▼
     toggle LED    print count    print message
```

### Two Timer Types
- **Periodic** (auto-reload = pdTRUE): fires repeatedly at a fixed interval.
- **One-shot** (auto-reload = pdFALSE): fires once, then stops.

### Why Use Timers Instead of Tasks?
Each task needs its own stack (512+ bytes of RAM). Timers share the daemon task's
stack. Ten timers cost almost nothing compared to ten tasks. The tradeoff: timer
callbacks must be short and must not block (no vTaskDelay, no xSemaphoreTake).

### API
```c
// Create periodic timer
TimerHandle_t t = xTimerCreate("name", pdMS_TO_TICKS(500),
                                pdTRUE, NULL, callback);
// Create one-shot timer
TimerHandle_t t = xTimerCreate("name", pdMS_TO_TICKS(5000),
                                pdFALSE, NULL, callback);
xTimerStart(t, 0);
xTimerStop(t, 0);
xTimerChangePeriod(t, pdMS_TO_TICKS(1000), 0);
```

## How to Build and Run

```bash
cd Lab02-Software-Timers
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
[MAIN] All timers started. No tasks created.
[MAIN] Everything runs in the timer daemon task.

[PERIODIC] Tick #1 at 2000 ms
[PERIODIC] Tick #2 at 4000 ms

[ONE-SHOT] *** Timer fired! (count=1, tick=5000) ***
[ONE-SHOT] Next one-shot in 5000 ms...

[PERIODIC] Tick #3 at 6000 ms
[PERIODIC] Tick #4 at 8000 ms
[PERIODIC] Tick #5 at 10000 ms

[ONE-SHOT] *** Timer fired! (count=2, tick=10000) ***
[ONE-SHOT] Next one-shot in 7000 ms...
```

The LED blinks every 500 ms (you will see it but there is no UART output for it).
The periodic print timer fires every 2 seconds. The one-shot timer fires at 5 seconds,
then chains itself with increasing delays.

## Why No Tasks Were Created

The main() function creates zero tasks with xTaskCreate. All three timer callbacks
run inside the FreeRTOS timer daemon task. This task is created internally by
vTaskStartScheduler when configUSE_TIMERS is set to 1 in FreeRTOSConfig.h.

## Complete Source Code

```c
   1  /**
   2    * Lab 2: Software Timers
   3    * -----------------------
   4    * Three software timers, zero dedicated tasks:
   5    *
   6    *   1. Periodic LED timer  - toggles LD2 every 500 ms
   7    *   2. Periodic print timer - prints a counter every 2 seconds
   8    *   3. One-shot timer       - fires once, 5 seconds after boot
   9    *
  10    * All callbacks run inside the FreeRTOS timer daemon task.
  11    *
  12    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  13    */
  14  
  15  #include "main.h"
  16  #include "FreeRTOS.h"
  17  #include "task.h"
  18  #include "timers.h"
  19  #include <stdio.h>
  20  #include <string.h>
  21  
  22  #define LED_PIN      GPIO_PIN_5
  23  #define LED_PORT     GPIOA
  24  
  25  UART_HandleTypeDef huart2;
  26  
  27  static void uart_print(const char *msg)
  28  {
  29      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  30  }
  31  
  32  /* ---- Timer callbacks ---- */
  33  
  34  /**
  35    * Called every 500 ms. Toggles the LED.
  36    * This is a periodic timer (auto-reload = pdTRUE).
  37    */
  38  static void LED_TimerCallback(TimerHandle_t xTimer)
  39  {
  40      (void)xTimer;
  41      HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
  42  }
  43  
  44  /**
  45    * Called every 2000 ms. Prints a counter.
  46    * This is a periodic timer.
  47    */
  48  static uint32_t printCounter = 0;
  49  static void Print_TimerCallback(TimerHandle_t xTimer)
  50  {
  51      char buf[80];
  52      (void)xTimer;
  53      printCounter++;
  54      snprintf(buf, sizeof(buf),
  55               "[PERIODIC] Tick #%lu at %lu ms\r\n",
  56               (unsigned long)printCounter,
  57               (unsigned long)HAL_GetTick());
  58      uart_print(buf);
  59  }
  60  
  61  /**
  62    * Fires once, 5 seconds after boot.
  63    * This is a one-shot timer (auto-reload = pdFALSE).
  64    * After firing, it starts a new one-shot timer to demonstrate chaining.
  65    */
  66  static TimerHandle_t oneShotTimer;
  67  static uint32_t oneShotCount = 0;
  68  static void OneShot_TimerCallback(TimerHandle_t xTimer)
  69  {
  70      char buf[120];
  71      oneShotCount++;
  72  
  73      snprintf(buf, sizeof(buf),
  74               "\r\n[ONE-SHOT] *** Timer fired! (count=%lu, tick=%lu) ***\r\n",
  75               (unsigned long)oneShotCount,
  76               (unsigned long)HAL_GetTick());
  77      uart_print(buf);
  78  
  79      if (oneShotCount < 3)
  80      {
  81          /* Restart the one-shot with a different period each time */
  82          uint32_t nextDelay = 3000 + (oneShotCount * 2000);
  83          snprintf(buf, sizeof(buf),
  84                   "[ONE-SHOT] Next one-shot in %lu ms...\r\n\r\n",
  85                   (unsigned long)nextDelay);
  86          uart_print(buf);
  87          xTimerChangePeriod(xTimer, pdMS_TO_TICKS(nextDelay), 0);
  88          /* ChangePeriod on a one-shot timer restarts it */
  89      }
  90      else
  91      {
  92          uart_print("[ONE-SHOT] No more one-shot timers. Periodic timers continue.\r\n\r\n");
  93      }
  94  }
  95  
  96  /* ---- Peripheral init ---- */
  97  static void GPIO_Init(void);
  98  static void UART2_Init(void);
  99  static void SystemClock_Config(void);
 100  
 101  int main(void)
 102  {
 103      HAL_Init();
 104      SystemClock_Config();
 105      GPIO_Init();
 106      UART2_Init();
 107  
 108      uart_print("\r\n=============================================\r\n");
 109      uart_print("  Lab 2: Software Timers\r\n");
 110      uart_print("=============================================\r\n");
 111      uart_print("  Timer 1: LED toggle every 500 ms (periodic)\r\n");
 112      uart_print("  Timer 2: Print counter every 2 s (periodic)\r\n");
 113      uart_print("  Timer 3: One-shot fires at 5 s after boot\r\n");
 114      uart_print("=============================================\r\n\r\n");
 115  
 116      /* Periodic timer: LED blink, 500 ms */
 117      TimerHandle_t ledTimer = xTimerCreate(
 118          "LED",                      /* name (debug only)        */
 119          pdMS_TO_TICKS(500),         /* period                   */
 120          pdTRUE,                     /* auto-reload = periodic   */
 121          NULL,                       /* timer ID (unused)        */
 122          LED_TimerCallback           /* callback function        */
 123      );
 124  
 125      /* Periodic timer: print, 2000 ms */
 126      TimerHandle_t printTimer = xTimerCreate(
 127          "Print",
 128          pdMS_TO_TICKS(2000),
 129          pdTRUE,                     /* periodic */
 130          NULL,
 131          Print_TimerCallback
 132      );
 133  
 134      /* One-shot timer: fires once at 5000 ms */
 135      oneShotTimer = xTimerCreate(
 136          "OneShot",
 137          pdMS_TO_TICKS(5000),
 138          pdFALSE,                    /* one-shot */
 139          NULL,
 140          OneShot_TimerCallback
 141      );
 142  
 143      /* Start all timers */
 144      xTimerStart(ledTimer, 0);
 145      xTimerStart(printTimer, 0);
 146      xTimerStart(oneShotTimer, 0);
 147  
 148      uart_print("[MAIN] All timers started. No tasks created.\r\n");
 149      uart_print("[MAIN] Everything runs in the timer daemon task.\r\n\r\n");
 150  
 151      vTaskStartScheduler();
 152      for (;;);
 153  }
 154  
 155  /* ---- Standard peripheral init ---- */
 156  
 157  static void GPIO_Init(void)
 158  {
 159      GPIO_InitTypeDef gpio = {0};
 160      __HAL_RCC_GPIOA_CLK_ENABLE();
 161      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 162      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 163      HAL_GPIO_Init(LED_PORT, &gpio);
 164  }
 165  
 166  static void UART2_Init(void)
 167  {
 168      GPIO_InitTypeDef gpio = {0};
 169      __HAL_RCC_USART2_CLK_ENABLE();
 170      __HAL_RCC_GPIOA_CLK_ENABLE();
 171      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 172      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 173      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 174      HAL_GPIO_Init(GPIOA, &gpio);
 175      huart2.Instance = USART2;
 176      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 177      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 178      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 179      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 180      HAL_UART_Init(&huart2);
 181  }
 182  
 183  static void SystemClock_Config(void)
 184  {
 185      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 186      __HAL_RCC_PWR_CLK_ENABLE();
 187      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 188      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 189      osc.HSEState = RCC_HSE_BYPASS;
 190      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 191      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 192      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 193      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 194      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 195      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 196      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 197      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 198      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 199  }
 200  
 201  #ifdef USE_FULL_ASSERT
 202  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 203  #endif
 204  
```
