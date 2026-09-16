# Lab 12: Heap and Stack Monitoring

## Board
STM32 Nucleo-F446RE

## Concept

FreeRTOS provides runtime functions to check memory usage:

```
    ┌──────────────────── FreeRTOS Heap (15 KB) ────────────────────┐
    │                                                                │
    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌────────┐ │
    │  │ Light   │ │ Medium  │ │ Heavy   │ │ Monitor │ │ Free   │ │
    │  │ stack   │ │ stack   │ │ stack   │ │ stack   │ │ heap   │ │
    │  │ 1024B   │ │ 1024B   │ │ 2048B   │ │ 2048B   │ │ space  │ │
    │  │         │ │         │ │         │ │         │ │        │ │
    │  │ used:▓▓ │ │ used:▓▓▓│ │ used:▓▓▓│ │ used:▓▓▓│ │        │ │
    │  │ free:░░░│ │ free:░░ │ │ ▓▓▓▓▓▓▓ │ │ ▓▓▓     │ │        │ │
    │  │         │ │         │ │ free:░░ │ │ free:░░ │ │        │ │
    │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └────────┘ │
    └──────────────────────────────────────────────────────────────┘

    xPortGetFreeHeapSize()              → current free bytes
    xPortGetMinimumEverFreeHeapSize()   → lowest free ever (high water mark)
    uxTaskGetStackHighWaterMark(handle) → unused stack words per task
```

### Why This Matters
On a microcontroller with 128 KB RAM, stack overflow corrupts memory silently.
There is no OS to catch it. Monitor water marks during development. If a water
mark is below 20 words, increase that task's stack size.

## How to Build and Run

```bash
cd Lab12-Heap-Monitor
make clean
make
make flash
```

## Expected Output
```
[MONITOR] Initial free heap: 5824 bytes
[MONITOR] Total heap (configTOTAL_HEAP_SIZE): 15360 bytes

============================================
  MEMORY REPORT
============================================
  Heap total:     15360 bytes
  Heap used:       9536 bytes (62%)
  Heap free:       5824 bytes
  Heap min ever:   5824 bytes (closest to full)
--------------------------------------------
  Task Stack High Water Marks (words free):
    Light  (stack=256):  220 words free  (OK)
    Medium (stack=256):  175 words free  (OK)
    Heavy  (stack=512):  168 words free  (OK)
    Monitor(stack=512):  142 words free  (OK)
============================================
```

Light task uses the least stack (highest water mark). Heavy task uses the most
(lowest water mark). All are well above the 20-word danger zone.

## Complete Source Code

```c
   1  /**
   2    * Lab 12: Heap and Stack Monitoring
   3    * -----------------------------------
   4    * Shows how to monitor FreeRTOS memory usage at runtime:
   5    *   - Total free heap (xPortGetFreeHeapSize)
   6    *   - Minimum ever free heap (xPortGetMinimumEverFreeHeapSize)
   7    *   - Per-task stack high water mark (uxTaskGetStackHighWaterMark)
   8    *
   9    * Three worker tasks with different stack usage patterns.
  10    * A monitor task prints a memory report every 3 seconds.
  11    *
  12    * UART2 at 115200 baud: minicom -D /dev/ttyACM0 -b 115200
  13    */
  14  
  15  #include "main.h"
  16  #include "FreeRTOS.h"
  17  #include "task.h"
  18  #include <stdio.h>
  19  #include <string.h>
  20  
  21  #define LED_PIN      GPIO_PIN_5
  22  #define LED_PORT     GPIOA
  23  
  24  UART_HandleTypeDef huart2;
  25  
  26  TaskHandle_t hLightTask, hMediumTask, hHeavyTask, hMonitorTask;
  27  
  28  static void uart_print(const char *msg)
  29  {
  30      HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  31  }
  32  
  33  /**
  34    * Light stack usage: small local variables.
  35    */
  36  static void Light_Task(void *arg)
  37  {
  38      uint32_t counter = 0;
  39      (void)arg;
  40  
  41      for (;;)
  42      {
  43          counter++;
  44          /* Just a small variable on stack */
  45          uint8_t flag = (counter % 2 == 0) ? 1 : 0;
  46          if (flag) HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
  47          vTaskDelay(pdMS_TO_TICKS(1000));
  48      }
  49  }
  50  
  51  /**
  52    * Medium stack usage: moderate local buffer.
  53    */
  54  static void Medium_Task(void *arg)
  55  {
  56      (void)arg;
  57  
  58      for (;;)
  59      {
  60          /* 64-byte buffer on stack */
  61          char buf[64];
  62          snprintf(buf, sizeof(buf), "Medium task running at %lu",
  63                   (unsigned long)HAL_GetTick());
  64          /* buf is used (printed), so the compiler does not optimize it away */
  65          uart_print("[MED   ] ");
  66          uart_print(buf);
  67          uart_print("\r\n");
  68          vTaskDelay(pdMS_TO_TICKS(2000));
  69      }
  70  }
  71  
  72  /**
  73    * Heavy stack usage: large local arrays.
  74    * This task uses significant stack space on purpose.
  75    */
  76  static void Heavy_Task(void *arg)
  77  {
  78      (void)arg;
  79  
  80      for (;;)
  81      {
  82          /* 256-byte buffer on stack (uses significant stack) */
  83          char data[256];
  84          memset(data, 0, sizeof(data));
  85          snprintf(data, sizeof(data),
  86                   "Heavy task: tick=%lu, processing %d bytes of data",
  87                   (unsigned long)HAL_GetTick(), (int)sizeof(data));
  88          uart_print("[HEAVY ] ");
  89          uart_print(data);
  90          uart_print("\r\n");
  91          vTaskDelay(pdMS_TO_TICKS(2500));
  92      }
  93  }
  94  
  95  /**
  96    * Monitor task: prints heap and stack usage report.
  97    */
  98  static void Monitor_Task(void *arg)
  99  {
 100      char buf[120];
 101      (void)arg;
 102  
 103      uart_print("[MONITOR] Memory monitor started.\r\n\r\n");
 104  
 105      /* Print initial heap state */
 106      snprintf(buf, sizeof(buf),
 107               "[MONITOR] Initial free heap: %u bytes\r\n",
 108               (unsigned)xPortGetFreeHeapSize());
 109      uart_print(buf);
 110      snprintf(buf, sizeof(buf),
 111               "[MONITOR] Total heap (configTOTAL_HEAP_SIZE): %u bytes\r\n\r\n",
 112               (unsigned)configTOTAL_HEAP_SIZE);
 113      uart_print(buf);
 114  
 115      vTaskDelay(pdMS_TO_TICKS(2000));
 116  
 117      for (;;)
 118      {
 119          uint32_t freeHeap    = xPortGetFreeHeapSize();
 120          uint32_t minFreeHeap = xPortGetMinimumEverFreeHeapSize();
 121          uint32_t usedHeap    = configTOTAL_HEAP_SIZE - freeHeap;
 122  
 123          uart_print("============================================\r\n");
 124          uart_print("  MEMORY REPORT\r\n");
 125          uart_print("============================================\r\n");
 126  
 127          snprintf(buf, sizeof(buf),
 128                   "  Heap total:     %5u bytes\r\n",
 129                   (unsigned)configTOTAL_HEAP_SIZE);
 130          uart_print(buf);
 131  
 132          snprintf(buf, sizeof(buf),
 133                   "  Heap used:      %5lu bytes (%lu%%)\r\n",
 134                   (unsigned long)usedHeap,
 135                   (unsigned long)(usedHeap * 100 / configTOTAL_HEAP_SIZE));
 136          uart_print(buf);
 137  
 138          snprintf(buf, sizeof(buf),
 139                   "  Heap free:      %5lu bytes\r\n",
 140                   (unsigned long)freeHeap);
 141          uart_print(buf);
 142  
 143          snprintf(buf, sizeof(buf),
 144                   "  Heap min ever:  %5lu bytes (closest to full)\r\n",
 145                   (unsigned long)minFreeHeap);
 146          uart_print(buf);
 147  
 148          uart_print("--------------------------------------------\r\n");
 149          uart_print("  Task Stack High Water Marks (words free):\r\n");
 150  
 151          /*
 152           * uxTaskGetStackHighWaterMark returns the MINIMUM number
 153           * of free stack words since the task started.
 154           * Lower = closer to overflow. Zero = crashed or will crash.
 155           */
 156          UBaseType_t hwm;
 157  
 158          hwm = uxTaskGetStackHighWaterMark(hLightTask);
 159          snprintf(buf, sizeof(buf),
 160                   "    Light  (stack=256):  %3lu words free  %s\r\n",
 161                   (unsigned long)hwm,
 162                   hwm < 20 ? "*** WARNING ***" : "(OK)");
 163          uart_print(buf);
 164  
 165          hwm = uxTaskGetStackHighWaterMark(hMediumTask);
 166          snprintf(buf, sizeof(buf),
 167                   "    Medium (stack=256):  %3lu words free  %s\r\n",
 168                   (unsigned long)hwm,
 169                   hwm < 20 ? "*** WARNING ***" : "(OK)");
 170          uart_print(buf);
 171  
 172          hwm = uxTaskGetStackHighWaterMark(hHeavyTask);
 173          snprintf(buf, sizeof(buf),
 174                   "    Heavy  (stack=512):  %3lu words free  %s\r\n",
 175                   (unsigned long)hwm,
 176                   hwm < 20 ? "*** WARNING ***" : "(OK)");
 177          uart_print(buf);
 178  
 179          hwm = uxTaskGetStackHighWaterMark(hMonitorTask);
 180          snprintf(buf, sizeof(buf),
 181                   "    Monitor(stack=512):  %3lu words free  %s\r\n",
 182                   (unsigned long)hwm,
 183                   hwm < 20 ? "*** WARNING ***" : "(OK)");
 184          uart_print(buf);
 185  
 186          uart_print("============================================\r\n\r\n");
 187  
 188          HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
 189          vTaskDelay(pdMS_TO_TICKS(3000));
 190      }
 191  }
 192  
 193  static void GPIO_Init(void);
 194  static void UART2_Init(void);
 195  static void SystemClock_Config(void);
 196  
 197  int main(void)
 198  {
 199      HAL_Init();
 200      SystemClock_Config();
 201      GPIO_Init();
 202      UART2_Init();
 203  
 204      uart_print("\r\n=============================================\r\n");
 205      uart_print("  Lab 12: Heap and Stack Monitoring\r\n");
 206      uart_print("=============================================\r\n");
 207      uart_print("  3 workers with different stack usage\r\n");
 208      uart_print("  Monitor prints memory report every 3 sec\r\n");
 209      uart_print("=============================================\r\n\r\n");
 210  
 211      xTaskCreate(Light_Task,   "Light",   256, NULL, 1, &hLightTask);
 212      xTaskCreate(Medium_Task,  "Medium",  256, NULL, 1, &hMediumTask);
 213      xTaskCreate(Heavy_Task,   "Heavy",   512, NULL, 1, &hHeavyTask);
 214      xTaskCreate(Monitor_Task, "Monitor", 512, NULL, 2, &hMonitorTask);
 215  
 216      vTaskStartScheduler();
 217      for (;;);
 218  }
 219  
 220  static void GPIO_Init(void)
 221  {
 222      GPIO_InitTypeDef gpio = {0};
 223      __HAL_RCC_GPIOA_CLK_ENABLE();
 224      gpio.Pin = LED_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
 225      gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
 226      HAL_GPIO_Init(LED_PORT, &gpio);
 227  }
 228  
 229  static void UART2_Init(void)
 230  {
 231      GPIO_InitTypeDef gpio = {0};
 232      __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
 233      gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
 234      gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_PULLUP;
 235      gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate = GPIO_AF7_USART2;
 236      HAL_GPIO_Init(GPIOA, &gpio);
 237      huart2.Instance = USART2;
 238      huart2.Init.BaudRate = 115200; huart2.Init.WordLength = UART_WORDLENGTH_8B;
 239      huart2.Init.StopBits = UART_STOPBITS_1; huart2.Init.Parity = UART_PARITY_NONE;
 240      huart2.Init.Mode = UART_MODE_TX_RX; huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
 241      huart2.Init.OverSampling = UART_OVERSAMPLING_16;
 242      HAL_UART_Init(&huart2);
 243  }
 244  
 245  static void SystemClock_Config(void)
 246  {
 247      RCC_ClkInitTypeDef clk = {0}; RCC_OscInitTypeDef osc = {0};
 248      __HAL_RCC_PWR_CLK_ENABLE();
 249      __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
 250      osc.OscillatorType = RCC_OSCILLATORTYPE_HSE; osc.HSEState = RCC_HSE_BYPASS;
 251      osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 252      osc.PLL.PLLM = 8; osc.PLL.PLLN = 360; osc.PLL.PLLP = RCC_PLLP_DIV2;
 253      osc.PLL.PLLQ = 7; osc.PLL.PLLR = 2;
 254      if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
 255      if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);
 256      clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
 257      clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
 258      clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
 259      if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
 260  }
 261  
 262  #ifdef USE_FULL_ASSERT
 263  void assert_failed(uint8_t *file, uint32_t line) { while(1); }
 264  #endif
 265  
```
