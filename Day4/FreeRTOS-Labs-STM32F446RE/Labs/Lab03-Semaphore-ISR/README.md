# Lab 03: Binary Semaphore from ISR

## Board
STM32 Nucleo-F446RE

## Concept

An ISR (Interrupt Service Routine) runs in hardware interrupt context. It must be fast.
You cannot call blocking RTOS functions from an ISR. The pattern: the ISR does the
minimum work and signals a task to handle the rest.

```
    HARDWARE                          SOFTWARE

    ┌──────────┐                   ┌──────────────┐
    │  Button  │───press───►       │ EXTI ISR     │
    │  (PC13)  │  falling edge     │              │
    └──────────┘                   │ GiveFromISR()│
                                   └──────┬───────┘
                                          │ semaphore
                                          ▼
                                   ┌──────────────┐
                                   │ Button Task  │
                                   │              │
                                   │ Take()       │
                                   │ print count  │
                                   │ toggle LED   │
                                   └──────────────┘
```

### ISR-Safe Functions
Regular xSemaphoreGive() must NOT be called from an ISR. Use the FromISR variant:

```c
// In the ISR:
BaseType_t woken = pdFALSE;
xSemaphoreGiveFromISR(sem, &woken);
portYIELD_FROM_ISR(woken);

// In the task:
xSemaphoreTake(sem, portMAX_DELAY);  // blocks until ISR gives
```

### NVIC Priority Rule
The ISR priority must be numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5).
We use priority 6. Using priority 4 or lower corrupts the FreeRTOS kernel.

## How to Build and Run

```bash
cd Lab03-Semaphore-ISR
make clean
make
make flash
```

Reset: press the black RESET button or run `st-flash reset`.

```bash
minicom -D /dev/ttyACM0 -b 115200
```

## How to Use
Press the **blue user button** on the Nucleo board. Each press triggers an interrupt
that wakes the task.

## Expected Output
```
[TASK] Waiting for button press...

[IDLE] System running... press the blue button.
[TASK] Button press #1 detected at tick=3245
[IDLE] System running... press the blue button.
[IDLE] System running... press the blue button.
[TASK] Button press #2 detected at tick=8102
[TASK] Button press #3 detected at tick=8834
[IDLE] System running... press the blue button.
```

The [IDLE] messages prove the system is alive between presses. Each press
increments the counter. The LED toggles on each press. The 200 ms debounce
delay prevents counting a single press multiple times due to mechanical bounce.

## Complete Source Code

```c
   1  /**
   2    * Lab 3: Binary Semaphore from ISR
   3    * ----------------------------------
   4    * Blue user button (PC13) generates an EXTI interrupt.
   5    * The ISR gives a binary semaphore.
   6    * A task waits on the semaphore, prints the event, toggles LED.
   7    *
   8    * Press the blue button on the Nucleo to see ISR -> task signaling.
   9    *
  10    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  11    */
  12  
  13  #include "main.h"
  14  #include "FreeRTOS.h"
  15  #include "task.h"
  16  #include "semphr.h"
  17  #include <stdio.h>
  18  #include <string.h>
  19  
  20  #define LED_PIN      GPIO_PIN_5
  21  #define LED_PORT     GPIOA
  22  #define BTN_PIN      GPIO_PIN_13
  23  #define BTN_PORT     GPIOC
  24  
  25  UART_HandleTypeDef huart2;
  26  SemaphoreHandle_t buttonSem;
  27  
  28  static void uart_print(const char *msg)
  29  {
  30      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  31  }
  32  
  33  /**
  34    * EXTI interrupt handler for PC13.
  35    * Called by the hardware when the blue button is pressed.
  36    * This runs in ISR context, so we use the FromISR variant.
  37    */
  38  void EXTI15_10_IRQHandler(void)
  39  {
  40      if (__HAL_GPIO_EXTI_GET_IT(BTN_PIN) != RESET)
  41      {
  42          __HAL_GPIO_EXTI_CLEAR_IT(BTN_PIN);
  43  
  44          BaseType_t woken = pdFALSE;
  45          xSemaphoreGiveFromISR(buttonSem, &woken);
  46          portYIELD_FROM_ISR(woken);
  47      }
  48  }
  49  
  50  /**
  51    * Button handler task.
  52    * Blocks on the semaphore with zero CPU cost.
  53    * Wakes instantly when the ISR gives the semaphore.
  54    */
  55  static void Button_Task(void *arg)
  56  {
  57      uint32_t pressCount = 0;
  58      char buf[100];
  59      (void)arg;
  60  
  61      uart_print("[TASK] Waiting for button press...\r\n\r\n");
  62  
  63      for (;;)
  64      {
  65          /* Block until ISR gives the semaphore */
  66          xSemaphoreTake(buttonSem, portMAX_DELAY);
  67  
  68          pressCount++;
  69          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
  70  
  71          snprintf(buf, sizeof(buf),
  72                   "[TASK] Button press #%lu detected at tick=%lu\r\n",
  73                   (unsigned long)pressCount,
  74                   (unsigned long)HAL_GetTick());
  75          uart_print(buf);
  76  
  77          /* Simple debounce: ignore presses for 200 ms */
  78          vTaskDelay(pdMS_TO_TICKS(200));
  79      }
  80  }
  81  
  82  /**
  83    * Background task that prints periodically.
  84    * Shows that the system is alive between button presses.
  85    */
  86  static void Idle_Task(void *arg)
  87  {
  88      (void)arg;
  89  
  90      for (;;)
  91      {
  92          uart_print("[IDLE] System running... press the blue button.\r\n");
  93          vTaskDelay(pdMS_TO_TICKS(3000));
  94      }
  95  }
  96  
  97  /* ---- Peripheral init ---- */
  98  static void GPIO_Init(void);
  99  static void Button_Init(void);
 100  static void UART2_Init(void);
 101  static void SystemClock_Config(void);
 102  
 103  int main(void)
 104  {
 105      HAL_Init();
 106      SystemClock_Config();
 107      GPIO_Init();
 108      Button_Init();
 109      UART2_Init();
 110  
 111      uart_print("\r\n=============================================\r\n");
 112      uart_print("  Lab 3: Binary Semaphore from ISR\r\n");
 113      uart_print("=============================================\r\n");
 114      uart_print("  Blue button (PC13) -> EXTI ISR\r\n");
 115      uart_print("  ISR gives semaphore -> Task wakes up\r\n");
 116      uart_print("  Task prints event and toggles LED\r\n");
 117      uart_print("=============================================\r\n\r\n");
 118  
 119      buttonSem = xSemaphoreCreateBinary();
 120  
 121      xTaskCreate(Button_Task, "Button", 512, NULL, 2, NULL);
 122      xTaskCreate(Idle_Task,   "Idle",   256, NULL, 1, NULL);
 123  
 124      vTaskStartScheduler();
 125      for (;;);
 126  }
 127  
 128  static void GPIO_Init(void)
 129  {
 130      GPIO_InitTypeDef gpio = {0};
 131      __HAL_RCC_GPIOA_CLK_ENABLE();
 132      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 133      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 134      HAL_GPIO_Init(LED_PORT, &gpio);
 135  }
 136  
 137  static void Button_Init(void)
 138  {
 139      GPIO_InitTypeDef gpio = {0};
 140      __HAL_RCC_GPIOC_CLK_ENABLE();
 141  
 142      /* PC13: input with falling edge interrupt (button press = falling) */
 143      gpio.Pin  = BTN_PIN;
 144      gpio.Mode = GPIO_MODE_IT_FALLING;
 145      gpio.Pull = GPIO_NOPULL;  /* Nucleo has external pull-up on PC13 */
 146      HAL_GPIO_Init(BTN_PORT, &gpio);
 147  
 148      /* Enable EXTI15_10 interrupt in NVIC */
 149      HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
 150      HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
 151      /*
 152       * Priority 6 is important: FreeRTOS requires ISR priorities to be
 153       * at or below configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)
 154       * numerically. Priority 6 is LOWER priority than 5, so it is
 155       * safe to call FromISR functions from this handler.
 156       */
 157  }
 158  
 159  static void UART2_Init(void)
 160  {
 161      GPIO_InitTypeDef gpio = {0};
 162      __HAL_RCC_USART2_CLK_ENABLE();
 163      __HAL_RCC_GPIOA_CLK_ENABLE();
 164      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 165      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 166      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 167      HAL_GPIO_Init(GPIOA, &gpio);
 168      huart2.Instance = USART2;
 169      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 170      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 171      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 172      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 173      HAL_UART_Init(&huart2);
 174  }
 175  
 176  static void SystemClock_Config(void)
 177  {
 178      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 179      __HAL_RCC_PWR_CLK_ENABLE();
 180      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 181      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 182      osc.HSEState = RCC_HSE_BYPASS;
 183      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 184      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 185      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 186      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 187      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 188      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 189      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 190      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 191      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 192  }
 193  
 194  #ifdef USE_FULL_ASSERT
 195  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 196  #endif
 197  
```
