#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdio.h>

#include <hexowl.h>

extern void gorun(uintptr_t);

void hexowl_task_handler(void *arg)
{
    uintptr_t go_heap_size = (uintptr_t)arg;
    gorun(go_heap_size);

    HexocalcInit();

    printf("Hexowl task initialized\r\n");
}