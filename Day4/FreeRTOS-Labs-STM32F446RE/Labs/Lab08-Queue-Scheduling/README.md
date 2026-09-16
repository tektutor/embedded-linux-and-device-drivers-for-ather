# Lab 08: Tasks and Scheduling with Queue

## Board
STM32 Nucleo-F446RE

## Concept

Two tasks at different priorities share a queue. The high-priority producer wakes every
2 seconds and preempts the low-priority consumer, which does background work (printing dots).

```
    Time ──────────────────────────────────────────────►

    Producer   sleep  │WAKE│  sleep  │WAKE│  sleep
    (HIGH)            │send│         │send│
                      │    │         │    │
    Consumer  .......│....│........│....│.........
    (LOW)     dots   │dots│  dots  │dots│  dots
                      │    │         │    │
    UART:    ........>>PRODUCED......>>PRODUCED......
                     <<CONSUMED     <<CONSUMED
```

Preemption is visible: the dot stream pauses the instant the producer wakes up.

## How to Build and Run

```bash
cd Lab08-Queue-Scheduling
make clean
make
make flash
```

Reset: press the black RESET button or run `st-flash reset`.

## Expected Output
```
........................................
>> PRODUCED  value=1  (tick=2000)
<< CONSUMED value=1  (tick=2000)
........................................
........................................
>> PRODUCED  value=2  (tick=4000)
<< CONSUMED value=2  (tick=4000)
```

The dots print every 100 ms (consumer's background work). Every 2 seconds,
the dot stream is interrupted by PRODUCED/CONSUMED messages. Both messages
show the same tick value, confirming zero scheduling delay.

## Complete Source Code

```c
   1  /**
   2    * Lab: Tasks and Scheduling
   3    * --------------------------
   4    * NUCLEO-F446RE + FreeRTOS
   5    *
   6    * Producer task (HIGH priority)  : writes values into a queue every 2 seconds.
   7    * Consumer task (LOW  priority)  : reads from the queue and logs each value.
   8    * Idle work    (inside Consumer) : busy-loops between reads, printing dots,
   9    *                                  so you can SEE when it gets preempted.
  10    *
  11    * UART2 (PA2/PA3) is connected to the ST-Link virtual COM port.
  12    * Open a terminal at 115200 baud to watch the output.
  13    *
  14    *   minicom -D /dev/ttyACM0 -b 115200
  15    *
  16    * What you will observe:
  17    *   - Consumer prints dots continuously (low-priority idle work).
  18    *   - Every 2 seconds the Producer PREEMPTS the Consumer, sends a value
  19    *     to the queue, and prints a "PRODUCED" message.
  20    *   - The Consumer immediately receives the value and prints "CONSUMED",
  21    *     then resumes printing dots until the next preemption.
  22    */
  23  
  24  #include "main.h"
  25  #include "cmsis_os.h"
  26  #include <stdio.h>
  27  #include <string.h>
  28  
  29  /* ---- LED: PA5 (LD2) ---- */
  30  #define LED_PIN      GPIO_PIN_5
  31  #define LED_PORT     GPIOA
  32  
  33  /* ---- UART2: PA2 (TX), PA3 (RX) -> ST-Link VCP ---- */
  34  UART_HandleTypeDef huart2;
  35  
  36  /* ---- FreeRTOS objects ---- */
  37  osThreadId producerHandle, consumerHandle;
  38  osMessageQId queueHandle;
  39  
  40  /* ---- Prototypes ---- */
  41  static void Producer_Task(void const *arg);
  42  static void Consumer_Task(void const *arg);
  43  static void SystemClock_Config(void);
  44  static void GPIO_Init(void);
  45  static void UART2_Init(void);
  46  static void uart_print(const char *msg);
  47  
  48  /* ================================================================
  49   * main
  50   * ================================================================ */
  51  int main(void)
  52  {
  53      HAL_Init();
  54      SystemClock_Config();
  55      GPIO_Init();
  56      UART2_Init();
  57  
  58      uart_print("\r\n========================================\r\n");
  59      uart_print("  FreeRTOS Queue Lab - Nucleo-F446RE\r\n");
  60      uart_print("  Producer = HIGH priority\r\n");
  61      uart_print("  Consumer = LOW  priority\r\n");
  62      uart_print("========================================\r\n\r\n");
  63  
  64      /* Create a queue that holds up to 5 uint32_t values */
  65      osMessageQDef(myQueue, 5, uint32_t);
  66      queueHandle = osMessageCreate(osMessageQ(myQueue), NULL);
  67  
  68      /* Producer: higher priority -> preempts Consumer */
  69      osThreadDef(Prod, Producer_Task, osPriorityAboveNormal, 0, 256);
  70      producerHandle = osThreadCreate(osThread(Prod), NULL);
  71  
  72      /* Consumer: lower priority -> runs when Producer is blocked */
  73      osThreadDef(Cons, Consumer_Task, osPriorityNormal, 0, 256);
  74      consumerHandle = osThreadCreate(osThread(Cons), NULL);
  75  
  76      osKernelStart();
  77  
  78      for (;;);
  79  }
  80  
  81  /* ================================================================
  82   * Producer Task (HIGH priority)
  83   *
  84   * Wakes every 2 seconds, sends a counter value into the queue.
  85   * Because it has higher priority, it PREEMPTS the Consumer
  86   * the instant osDelay expires.
  87   * ================================================================ */
  88  static void Producer_Task(void const *arg)
  89  {
  90      uint32_t counter = 0;
  91      char buf[80];
  92      (void)arg;
  93  
  94      for (;;)
  95      {
  96          osDelay(2000);   /* sleep 2 s -> Consumer runs during this time */
  97  
  98          counter++;
  99          osMessagePut(queueHandle, counter, 0);
 100  
 101          /* Toggle LED so you see a blink each time Producer runs */
 102          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 103  
 104          snprintf(buf, sizeof(buf),
 105                   "\r\n>> PRODUCED  value=%lu  (tick=%lu)\r\n",
 106                   (unsigned long)counter,
 107                   (unsigned long)osKernelSysTick());
 108          uart_print(buf);
 109      }
 110  }
 111  
 112  /* ================================================================
 113   * Consumer Task (LOW priority)
 114   *
 115   * Tries to read from the queue (non-blocking).
 116   *   - If a value is available, it prints CONSUMED.
 117   *   - Otherwise, it prints a dot and does a short busy-wait,
 118   *     simulating low-priority background work.
 119   *
 120   * The dots let you SEE preemption: the dot stream pauses
 121   * whenever the Producer wakes up and takes the CPU.
 122   * ================================================================ */
 123  static void Consumer_Task(void const *arg)
 124  {
 125      osEvent evt;
 126      char buf[80];
 127      uint32_t dot_count = 0;
 128      (void)arg;
 129  
 130      for (;;)
 131      {
 132          /* Non-blocking read from the queue */
 133          evt = osMessageGet(queueHandle, 0);
 134  
 135          if (evt.status == osEventMessage)
 136          {
 137              snprintf(buf, sizeof(buf),
 138                       "\r\n<< CONSUMED value=%lu  (tick=%lu)\r\n",
 139                       (unsigned long)evt.value.v,
 140                       (unsigned long)osKernelSysTick());
 141              uart_print(buf);
 142              dot_count = 0;
 143          }
 144          else
 145          {
 146              /* Background work: print a dot every 100 ms */
 147              uart_print(".");
 148              dot_count++;
 149              if (dot_count % 40 == 0)
 150                  uart_print("\r\n");   /* line wrap for readability */
 151  
 152              osDelay(100);
 153          }
 154      }
 155  }
 156  
 157  /* ================================================================
 158   * UART helper
 159   * ================================================================ */
 160  static void uart_print(const char *msg)
 161  {
 162      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
 163  }
 164  
 165  /* ================================================================
 166   * Peripheral init
 167   * ================================================================ */
 168  static void GPIO_Init(void)
 169  {
 170      GPIO_InitTypeDef gpio = {0};
 171      __HAL_RCC_GPIOA_CLK_ENABLE();
 172  
 173      /* LD2 on PA5 */
 174      gpio.Pin   = LED_PIN;
 175      gpio.Mode  = GPIO_MODE_OUTPUT_PP;
 176      gpio.Pull  = GPIO_NOPULL;
 177      gpio.Speed = GPIO_SPEED_FREQ_LOW;
 178      HAL_GPIO_Init(LED_PORT, &gpio);
 179  }
 180  
 181  static void UART2_Init(void)
 182  {
 183      GPIO_InitTypeDef gpio = {0};
 184  
 185      __HAL_RCC_USART2_CLK_ENABLE();
 186      __HAL_RCC_GPIOA_CLK_ENABLE();
 187  
 188      /* PA2 = USART2_TX, PA3 = USART2_RX */
 189      gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
 190      gpio.Mode      = GPIO_MODE_AF_PP;
 191      gpio.Pull      = GPIO_PULLUP;
 192      gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
 193      gpio.Alternate = GPIO_AF7_USART2;
 194      HAL_GPIO_Init(GPIOA, &gpio);
 195  
 196      huart2.Instance          = USART2;
 197      huart2.Init.BaudRate     = 115200;
 198      huart2.Init.WordLength   = UART_WORDLENGTH_8B;
 199      huart2.Init.StopBits     = UART_STOPBITS_1;
 200      huart2.Init.Parity       = UART_PARITY_NONE;
 201      huart2.Init.Mode         = UART_MODE_TX_RX;
 202      huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
 203      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 204      HAL_UART_Init(&huart2);
 205  }
 206  
 207  /* ================================================================
 208   * System Clock: 180 MHz, HSE bypass (Nucleo ST-Link MCO)
 209   * ================================================================ */
 210  static void SystemClock_Config(void)
 211  {
 212      RCC_ClkInitTypeDef clk = {0};
 213      RCC_OscInitTypeDef osc = {0};
 214  
 215      __HAL_RCC_PWR_CLK_ENABLE();
 216      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 217  
 218      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
 219      osc.HSEState       = RCC_HSE_BYPASS;
 220      osc.PLL.PLLState   = RCC_PLL_ON;
 221      osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
 222      osc.PLL.PLLM       = 8;
 223      osc.PLL.PLLN       = 360;
 224      osc.PLL.PLLP       = RCC_PLLP_DIV2;
 225      osc.PLL.PLLQ       = 7;
 226      osc.PLL.PLLR       = 2;
 227      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for (;;);
 228      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for (;;);
 229  
 230      clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
 231                            RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
 232      clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
 233      clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
 234      clk.APB1CLKDivider = RCC_HCLK_DIV4;
 235      clk.APB2CLKDivider = RCC_HCLK_DIV2;
 236      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for (;;);
 237  }
 238  
 239  #ifdef USE_FULL_ASSERT
 240  void assert_failed(uint8_t *file, uint32_t line) { while (1); }
 241  #endif
 242  
```
