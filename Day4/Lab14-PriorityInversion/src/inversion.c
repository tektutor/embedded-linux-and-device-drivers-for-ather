/* inversion.c - reproduce priority inversion, then fix it with priority inheritance.
 * Reference starter code (FreeRTOS). busy_work_ms() is a placeholder busy loop.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static SemaphoreHandle_t m;   /* see the FIX note below */

static void busy_work_ms(int ms) { volatile int i; while (ms--) for (i=0;i<100000;i++); }

static void low(void *a) {
    xSemaphoreTake(m, portMAX_DELAY);
    busy_work_ms(100);                 /* holds the lock */
    xSemaphoreGive(m);
    vTaskDelete(NULL);
}
static void medium(void *a) {          /* hogs CPU, needs no lock */
    for (;;) busy_work_ms(50);
}
static void high(void *a) {
    xSemaphoreTake(m, portMAX_DELAY);  /* blocks until low releases */
    xSemaphoreGive(m);
    vTaskDelete(NULL);
}
int main(void) {
    /* Without inheritance, high waits for medium to finish -> inversion.
     * FIX: xSemaphoreCreateMutex() (a MUTEX, not a binary semaphore) boosts
     *      low to high's priority while it holds the lock, so medium cannot
     *      delay high. Switch the line below to see the difference.
     */
    m = xSemaphoreCreateMutex();                 /* fixed: priority inheritance */
    /* m = xSemaphoreCreateBinary(); xSemaphoreGive(m);  // to reproduce inversion */
    xTaskCreate(low,    "low",  256, NULL, 1, NULL);
    xTaskCreate(medium, "med",  256, NULL, 2, NULL);
    xTaskCreate(high,   "high", 256, NULL, 3, NULL);
    vTaskStartScheduler();
    for (;;);
}
