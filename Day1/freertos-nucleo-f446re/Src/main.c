/**
  * FreeRTOS_ThreadCreation for NUCLEO-F446RE
  * Adapted from STM32446E_EVAL example.
  *
  * Two threads toggle the onboard LED (LD2, PA5) at different rates,
  * suspending and resuming each other in a 15-second cycle.
  */
#include "main.h"
#include "cmsis_os.h"

#define LED_PIN        GPIO_PIN_5
#define LED_PORT       GPIOA
#define LED_CLK_EN()   __HAL_RCC_GPIOA_CLK_ENABLE()

osThreadId LEDThread1Handle, LEDThread2Handle;

static void LED_Thread1(void const *argument);
static void LED_Thread2(void const *argument);
static void SystemClock_Config(void);
static void LED_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    osThreadDef(T1, LED_Thread1, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
    osThreadDef(T2, LED_Thread2, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);

    LEDThread1Handle = osThreadCreate(osThread(T1), NULL);
    LEDThread2Handle = osThreadCreate(osThread(T2), NULL);

    osKernelStart();
    for (;;);
}

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    LED_CLK_EN();
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static void LED_Thread1(void const *argument)
{
    uint32_t count = 0;
    (void)argument;

    for (;;)
    {
        count = osKernelSysTick() + 5000;
        while (count >= osKernelSysTick())
        {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            osDelay(200);
        }
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        osThreadSuspend(NULL);

        count = osKernelSysTick() + 5000;
        while (count >= osKernelSysTick())
        {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            osDelay(400);
        }
        osThreadResume(LEDThread2Handle);
    }
}

static void LED_Thread2(void const *argument)
{
    uint32_t count;
    (void)argument;

    for (;;)
    {
        count = osKernelSysTick() + 10000;
        while (count >= osKernelSysTick())
        {
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
            osDelay(500);
        }
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        osThreadResume(LEDThread1Handle);
        osThreadSuspend(NULL);
    }
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clk = {0};
    RCC_OscInitTypeDef osc = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_BYPASS;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 360;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    osc.PLL.PLLR       = 2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) for(;;);
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) for(;;);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                          RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) for(;;);
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while (1); }
#endif
