#include "FreeRTOS.h"
#include "task.h"

void vApplicationMallocFailedHook(void)   { taskDISABLE_INTERRUPTS(); for(;;); }
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
                                          { (void)xTask; (void)pcTaskName; for(;;); }
