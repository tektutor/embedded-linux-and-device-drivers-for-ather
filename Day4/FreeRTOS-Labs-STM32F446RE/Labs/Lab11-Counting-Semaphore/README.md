# Lab 11: Counting Semaphore (Resource Pool)

## Board
STM32 Nucleo-F446RE

## Concept

A counting semaphore manages a pool of N identical resources. Each take decrements
the count. Each give increments it. When count reaches 0, the next taker blocks.

```
    5 clients, 3 slots:

    Slot 1: ████ Client 1 ████████████
    Slot 2: ████ Client 2 ████████████████████
    Slot 3: ████ Client 3 ████████████████████████████
    Waiting:     Client 4 ─────wait─────┤████ Client 4 ████
    Waiting:     Client 5 ─────────────wait──────────┤████ Client 5
                 ▲                      ▲            ▲
                 all slots taken        C1 finishes   C2 finishes
                 C4 and C5 block        C4 gets slot  C5 gets slot
```

### API
```c
// Create: max=3, initial=3 (all available)
SemaphoreHandle_t pool = xSemaphoreCreateCounting(3, 3);
xSemaphoreTake(pool, portMAX_DELAY);   // acquire (count--)
xSemaphoreGive(pool);                  // release (count++)
UBaseType_t n = uxSemaphoreGetCount(pool);  // check availability
```

## How to Build and Run

```bash
cd Lab11-Counting-Semaphore
make clean
make
make flash
```

## Expected Output
```
[Client1] Job #1: requesting slot... (3/3 available)
[Client1] GOT slot after 0 ms wait. (2/3 remaining)
[Client2] Job #1: requesting slot... (2/3 available)
[Client2] GOT slot after 0 ms wait. (1/3 remaining)
[Client3] Job #1: requesting slot... (1/3 available)
[Client3] GOT slot after 0 ms wait. (0/3 remaining)
[Client4] Job #1: requesting slot... (0/3 available)
[Client5] Job #1: requesting slot... (0/3 available)
[Client1] Released slot after 2500 ms work.
[Client4] GOT slot after 2100 ms wait. (0/3 remaining)
```

Clients 1-3 get slots immediately (0 ms wait). Clients 4-5 block until a slot frees up.
Their wait times match the work duration of the clients ahead of them.

## Complete Source Code

```c
   1  /**
   2    * Lab 11: Counting Semaphore
   3    * ---------------------------
   4    * A counting semaphore manages a pool of 3 "connection slots".
   5    * Five client tasks each try to acquire a slot, hold it for
   6    * a few seconds (simulating work), then release it.
   7    *
   8    * With only 3 slots and 5 clients, you see clients waiting
   9    * in line for a slot to free up.
  10    *
  11    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  12    */
  13  
  14  #include "main.h"
  15  #include "FreeRTOS.h"
  16  #include "task.h"
  17  #include "semphr.h"
  18  #include <stdio.h>
  19  #include <string.h>
  20  
  21  #define LED_PIN      GPIO_PIN_5
  22  #define LED_PORT     GPIOA
  23  #define MAX_SLOTS    3
  24  #define NUM_CLIENTS  5
  25  
  26  UART_HandleTypeDef huart2;
  27  SemaphoreHandle_t connectionPool;
  28  
  29  static void uart_print(const char *msg)
  30  {
  31      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  32  }
  33  
  34  /**
  35    * Client task: acquires a connection slot, does work, releases it.
  36    */
  37  static void Client_Task(void *arg)
  38  {
  39      uint32_t id = (uint32_t)arg;
  40      uint32_t jobs = 0;
  41      char buf[120];
  42  
  43      /* Stagger start times so clients don't all rush at once */
  44      vTaskDelay(pdMS_TO_TICKS(id * 200));
  45  
  46      for (;;)
  47      {
  48          jobs++;
  49          uint32_t avail = uxSemaphoreGetCount(connectionPool);
  50          snprintf(buf, sizeof(buf),
  51                   "[Client%lu] Job #%lu: requesting slot... (%lu/%d available)\r\n",
  52                   (unsigned long)id, (unsigned long)jobs,
  53                   (unsigned long)avail, MAX_SLOTS);
  54          uart_print(buf);
  55  
  56          uint32_t t0 = HAL_GetTick();
  57          xSemaphoreTake(connectionPool, portMAX_DELAY);
  58          uint32_t t1 = HAL_GetTick();
  59  
  60          avail = uxSemaphoreGetCount(connectionPool);
  61          snprintf(buf, sizeof(buf),
  62                   "[Client%lu] GOT slot after %lu ms wait. (%lu/%d remaining)\r\n",
  63                   (unsigned long)id, (unsigned long)(t1 - t0),
  64                   (unsigned long)avail, MAX_SLOTS);
  65          uart_print(buf);
  66  
  67          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
  68  
  69          /* Simulate connection work: 2-4 seconds depending on client */
  70          uint32_t workTime = 2000 + (id * 500);
  71          vTaskDelay(pdMS_TO_TICKS(workTime));
  72  
  73          xSemaphoreGive(connectionPool);
  74  
  75          snprintf(buf, sizeof(buf),
  76                   "[Client%lu] Released slot after %lu ms work.\r\n\r\n",
  77                   (unsigned long)id, (unsigned long)workTime);
  78          uart_print(buf);
  79  
  80          /* Brief pause before next job */
  81          vTaskDelay(pdMS_TO_TICKS(500));
  82      }
  83  }
  84  
  85  static void GPIO_Init(void);
  86  static void UART2_Init(void);
  87  static void SystemClock_Config(void);
  88  
  89  int main(void)
  90  {
  91      HAL_Init();
  92      SystemClock_Config();
  93      GPIO_Init();
  94      UART2_Init();
  95  
  96      uart_print("\r\n=============================================\r\n");
  97      uart_print("  Lab 11: Counting Semaphore\r\n");
  98      uart_print("=============================================\r\n");
  99  
 100      char buf[80];
 101      snprintf(buf, sizeof(buf),
 102               "  %d connection slots, %d clients\r\n", MAX_SLOTS, NUM_CLIENTS);
 103      uart_print(buf);
 104      uart_print("  Clients wait when all slots are taken\r\n");
 105      uart_print("=============================================\r\n\r\n");
 106  
 107      /*
 108       * Create a counting semaphore.
 109       *   Arg 1: maximum count (3 slots)
 110       *   Arg 2: initial count (all 3 available)
 111       */
 112      connectionPool = xSemaphoreCreateCounting(MAX_SLOTS, MAX_SLOTS);
 113  
 114      for (uint32_t i = 1; i <= NUM_CLIENTS; i++)
 115      {
 116          char name[12];
 117          snprintf(name, sizeof(name), "Client%lu", (unsigned long)i);
 118          xTaskCreate(Client_Task, name, 512, (void *)i, 1, NULL);
 119      }
 120  
 121      vTaskStartScheduler();
 122      for (;;);
 123  }
 124  
 125  static void GPIO_Init(void)
 126  {
 127      GPIO_InitTypeDef gpio = {0};
 128      __HAL_RCC_GPIOA_CLK_ENABLE();
 129      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 130      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 131      HAL_GPIO_Init(LED_PORT, &gpio);
 132  }
 133  
 134  static void UART2_Init(void)
 135  {
 136      GPIO_InitTypeDef gpio = {0};
 137      __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
 138      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 139      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 140      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 141      HAL_GPIO_Init(GPIOA, &gpio);
 142      huart2.Instance = USART2;
 143      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 144      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 145      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 146      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 147      HAL_UART_Init(&huart2);
 148  }
 149  
 150  static void SystemClock_Config(void)
 151  {
 152      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 153      __HAL_RCC_PWR_CLK_ENABLE();
 154      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 155      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE; osc.HSEState = RCC_HSE_BYPASS;
 156      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 157      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 158      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 159      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 160      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 161      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 162      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 163      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 164      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 165  }
 166  
 167  #ifdef USE_FULL_ASSERT
 168  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 169  #endif
 170  
```
