# Lab 10: Event Groups

## Board
STM32 Nucleo-F446RE

## Concept

An event group is a set of flag bits. Tasks set bits to signal events.
Other tasks wait for specific combinations using AND (all bits) or OR (any bit).

```
    Sensor task ──(2 sec)──► set bit 0 ──┐
                                         │
    Network task ─(4 sec)──► set bit 1 ──┼──► Coordinator
                                         │    waits for ALL
    Storage task ─(3 sec)──► set bit 2 ──┘    bits 0,1,2

    Bit pattern: 0b000 → 0b001 → 0b101 → 0b111 (ALL READY)
                          ▲ sensor   ▲ storage   ▲ network
                          at 2s      at 3s       at 4s
```

### API
```c
EventGroupHandle_t eg = xEventGroupCreate();
xEventGroupSetBits(eg, (1 << 0));           // set bit 0
xEventGroupWaitBits(eg, ALL_BITS,
    pdTRUE,    // clear on exit
    pdTRUE,    // wait for ALL (AND)
    portMAX_DELAY);
```

## How to Build and Run

```bash
cd Lab10-Event-Groups
make clean
make
make flash
```

## Expected Output
```
[COORD  ] === Cycle 1: Waiting for all subsystems... ===
[SENSOR ] Initializing sensors...
[NETWORK] Connecting to network...
[STORAGE] Mounting filesystem...
[SENSOR ] Sensors ready at tick=2000. Setting bit 0.
[STORAGE] Storage mounted at tick=3000. Setting bit 2.
[NETWORK] Network connected at tick=4000. Setting bit 1.
[COORD  ] ALL READY! bits=0x07, waited 4000 ms
[COORD  ] System fully operational. Running main loop.
[COORD  ] Main loop iteration 1/5
```

The coordinator wakes at tick=4000 (when the slowest subsystem finishes).
bits=0x07 is binary 0b111, confirming all three bits were set.

## Complete Source Code

```c
   1  /**
   2    * Lab 10: Event Groups
   3    * ---------------------
   4    * Three worker tasks each complete a stage of initialization.
   5    * Each sets a different bit in an event group when done.
   6    * A coordinator task waits for ALL three bits before proceeding.
   7    *
   8    * Demonstrates:
   9    *   - xEventGroupSetBits   (worker signals completion)
  10    *   - xEventGroupWaitBits  (coordinator waits for all)
  11    *   - Synchronization barrier pattern
  12    *
  13    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  14    */
  15  
  16  #include "main.h"
  17  #include "FreeRTOS.h"
  18  #include "task.h"
  19  #include "event_groups.h"
  20  #include <stdio.h>
  21  #include <string.h>
  22  
  23  #define LED_PIN      GPIO_PIN_5
  24  #define LED_PORT     GPIOA
  25  
  26  UART_HandleTypeDef huart2;
  27  
  28  /* Each worker sets one bit in the event group */
  29  #define SENSOR_READY   (1 << 0)   /* bit 0 */
  30  #define NETWORK_READY  (1 << 1)   /* bit 1 */
  31  #define STORAGE_READY  (1 << 2)   /* bit 2 */
  32  #define ALL_READY      (SENSOR_READY | NETWORK_READY | STORAGE_READY)
  33  
  34  EventGroupHandle_t startupEvents;
  35  
  36  static void uart_print(const char *msg)
  37  {
  38      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  39  }
  40  
  41  /**
  42    * Simulates sensor subsystem initialization.
  43    * Takes 2 seconds, then sets SENSOR_READY bit.
  44    */
  45  static void Sensor_Init_Task(void *arg)
  46  {
  47      char buf[100];
  48      (void)arg;
  49  
  50      for (;;)
  51      {
  52          uart_print("[SENSOR ] Initializing sensors...\r\n");
  53          vTaskDelay(pdMS_TO_TICKS(2000));  /* simulate 2 sec init */
  54  
  55          snprintf(buf, sizeof(buf),
  56                   "[SENSOR ] Sensors ready at tick=%lu. Setting bit 0.\r\n",
  57                   (unsigned long)HAL_GetTick());
  58          uart_print(buf);
  59  
  60          xEventGroupSetBits(startupEvents, SENSOR_READY);
  61  
  62          /* Wait before next cycle */
  63          vTaskDelay(pdMS_TO_TICKS(15000));
  64      }
  65  }
  66  
  67  /**
  68    * Simulates network subsystem initialization.
  69    * Takes 4 seconds (slowest), then sets NETWORK_READY bit.
  70    */
  71  static void Network_Init_Task(void *arg)
  72  {
  73      char buf[100];
  74      (void)arg;
  75  
  76      for (;;)
  77      {
  78          uart_print("[NETWORK] Connecting to network...\r\n");
  79          vTaskDelay(pdMS_TO_TICKS(4000));  /* simulate 4 sec init */
  80  
  81          snprintf(buf, sizeof(buf),
  82                   "[NETWORK] Network connected at tick=%lu. Setting bit 1.\r\n",
  83                   (unsigned long)HAL_GetTick());
  84          uart_print(buf);
  85  
  86          xEventGroupSetBits(startupEvents, NETWORK_READY);
  87  
  88          vTaskDelay(pdMS_TO_TICKS(15000));
  89      }
  90  }
  91  
  92  /**
  93    * Simulates storage subsystem initialization.
  94    * Takes 3 seconds, then sets STORAGE_READY bit.
  95    */
  96  static void Storage_Init_Task(void *arg)
  97  {
  98      char buf[100];
  99      (void)arg;
 100  
 101      for (;;)
 102      {
 103          uart_print("[STORAGE] Mounting filesystem...\r\n");
 104          vTaskDelay(pdMS_TO_TICKS(3000));  /* simulate 3 sec init */
 105  
 106          snprintf(buf, sizeof(buf),
 107                   "[STORAGE] Storage mounted at tick=%lu. Setting bit 2.\r\n",
 108                   (unsigned long)HAL_GetTick());
 109          uart_print(buf);
 110  
 111          xEventGroupSetBits(startupEvents, STORAGE_READY);
 112  
 113          vTaskDelay(pdMS_TO_TICKS(15000));
 114      }
 115  }
 116  
 117  /**
 118    * Coordinator: waits for ALL three subsystems before starting.
 119    */
 120  static void Coordinator_Task(void *arg)
 121  {
 122      char buf[120];
 123      EventBits_t bits;
 124      uint32_t cycle = 0;
 125      (void)arg;
 126  
 127      for (;;)
 128      {
 129          cycle++;
 130          snprintf(buf, sizeof(buf),
 131                   "\r\n[COORD  ] === Cycle %lu: Waiting for all subsystems... ===\r\n",
 132                   (unsigned long)cycle);
 133          uart_print(buf);
 134  
 135          uint32_t t0 = HAL_GetTick();
 136  
 137          /*
 138           * Wait for ALL three bits to be set.
 139           *   pdTRUE  = clear bits after returning (reset for next cycle)
 140           *   pdTRUE  = wait for ALL bits (AND), not ANY (OR)
 141           */
 142          bits = xEventGroupWaitBits(
 143              startupEvents,
 144              ALL_READY,      /* bits to wait for */
 145              pdTRUE,         /* clear on exit    */
 146              pdTRUE,         /* wait for ALL     */
 147              portMAX_DELAY   /* wait forever     */
 148          );
 149  
 150          uint32_t t1 = HAL_GetTick();
 151  
 152          snprintf(buf, sizeof(buf),
 153                   "[COORD  ] ALL READY! bits=0x%02lX, waited %lu ms\r\n",
 154                   (unsigned long)bits, (unsigned long)(t1 - t0));
 155          uart_print(buf);
 156  
 157          uart_print("[COORD  ] System fully operational. Running main loop.\r\n");
 158  
 159          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 160  
 161          /* Simulate main application work */
 162          for (int i = 0; i < 5; i++)
 163          {
 164              vTaskDelay(pdMS_TO_TICKS(1000));
 165              snprintf(buf, sizeof(buf),
 166                       "[COORD  ] Main loop iteration %d/5\r\n", i + 1);
 167              uart_print(buf);
 168          }
 169  
 170          uart_print("[COORD  ] Cycle complete. Restarting init sequence...\r\n\r\n");
 171      }
 172  }
 173  
 174  static void GPIO_Init(void);
 175  static void UART2_Init(void);
 176  static void SystemClock_Config(void);
 177  
 178  int main(void)
 179  {
 180      HAL_Init();
 181      SystemClock_Config();
 182      GPIO_Init();
 183      UART2_Init();
 184  
 185      uart_print("\r\n=============================================\r\n");
 186      uart_print("  Lab 10: Event Groups\r\n");
 187      uart_print("=============================================\r\n");
 188      uart_print("  3 subsystems init at different speeds\r\n");
 189      uart_print("  Coordinator waits for ALL to be ready\r\n");
 190      uart_print("  Bit 0=Sensor  Bit 1=Network  Bit 2=Storage\r\n");
 191      uart_print("=============================================\r\n\r\n");
 192  
 193      startupEvents = xEventGroupCreate();
 194  
 195      xTaskCreate(Sensor_Init_Task,  "Sensor",  256, NULL, 1, NULL);
 196      xTaskCreate(Network_Init_Task, "Network", 256, NULL, 1, NULL);
 197      xTaskCreate(Storage_Init_Task, "Storage", 256, NULL, 1, NULL);
 198      xTaskCreate(Coordinator_Task,  "Coord",   512, NULL, 2, NULL);
 199  
 200      vTaskStartScheduler();
 201      for (;;);
 202  }
 203  
 204  static void GPIO_Init(void)
 205  {
 206      GPIO_InitTypeDef gpio = {0};
 207      __HAL_RCC_GPIOA_CLK_ENABLE();
 208      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 209      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 210      HAL_GPIO_Init(LED_PORT, &gpio);
 211  }
 212  
 213  static void UART2_Init(void)
 214  {
 215      GPIO_InitTypeDef gpio = {0};
 216      __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
 217      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 218      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 219      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 220      HAL_GPIO_Init(GPIOA, &gpio);
 221      huart2.Instance = USART2;
 222      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 223      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 224      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 225      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 226      HAL_UART_Init(&huart2);
 227  }
 228  
 229  static void SystemClock_Config(void)
 230  {
 231      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 232      __HAL_RCC_PWR_CLK_ENABLE();
 233      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 234      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE; osc.HSEState = RCC_HSE_BYPASS;
 235      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 236      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 237      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 238      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 239      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 240      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 241      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 242      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 243      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 244  }
 245  
 246  #ifdef USE_FULL_ASSERT
 247  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 248  #endif
 249  
```
