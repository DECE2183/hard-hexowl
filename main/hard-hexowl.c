#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdio.h>

const uintptr_t hexowl_heap_size = 256 * 1024;
const uintptr_t hexowl_task_stack_size = 2 * 1024 * 1024;
extern void hexowl_task_handler(void *arg);
StaticTask_t task_hexowl;
StackType_t *task_hexowl_stack;

void app_main(void)
{
    esp_err_t err;

    task_hexowl_stack = heap_caps_malloc(hexowl_task_stack_size, MALLOC_CAP_SPIRAM);
    if (task_hexowl_stack == NULL)
    {
        printf("ERROR: unable to allocate the hexowl task stack\r\n");
        goto error;
    }

    if (!xTaskCreateStaticPinnedToCore(
        hexowl_task_handler, "hexowl",
        hexowl_task_stack_size, (void *)hexowl_heap_size,
        0, task_hexowl_stack, &task_hexowl, 1))
    {
        printf("ERROR: unable to create the hexowl task\r\n");
        goto error;
    }

    while (1)
    {
        vTaskDelay(15);
    }

error:
    while (1)
    {
        vTaskDelay(15);
    }
}
