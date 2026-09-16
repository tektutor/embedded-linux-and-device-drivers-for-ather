/* tasks.c - two FreeRTOS tasks at different priorities sharing a queue.
 * Reference starter code (FreeRTOS on the STM32 Nucleo).
 * Confirm the API and printf availability against your SDK/port.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

static QueueHandle_t q;

static void producer(void *arg) {          /* higher priority */
    int n = 0;
    for (;;) {
        xQueueSend(q, &n, portMAX_DELAY);
        n++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
static void consumer(void *arg) {          /* lower priority */
    int v;
    for (;;)
        if (xQueueReceive(q, &v, portMAX_DELAY) == pdTRUE)
            printf("got %d\n", v);         /* or toggle a pin / scope */
}
int main(void) {
    q = xQueueCreate(8, sizeof(int));
    xTaskCreate(producer, "prod", 256, NULL, 3, NULL);   /* prio 3 */
    xTaskCreate(consumer, "cons", 256, NULL, 1, NULL);   /* prio 1 */
    vTaskStartScheduler();
    for (;;);
}
