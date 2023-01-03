#include <stdio.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ui.h>
#include <keyboard.h>
#include <sensors.h>

// hexowl task stuff
extern void hexowl_task(void *arg);
const uintptr_t hexowl_heap_size = 256 * 1024;
const uintptr_t hexowl_task_stack_size = 2 * 1024 * 1024;
StaticTask_t hexowl_static_task;
StackType_t *hexowl_static_stack;

// ui task stuff
#define UI_STACK_SIZE 2048
const uintptr_t ui_stack_size = UI_STACK_SIZE;
StackType_t ui_static_stack[UI_STACK_SIZE];
StaticTask_t ui_static_task;

// keyboard task stuff
#define KBRD_STACK_SIZE 2048
const uintptr_t keyboard_stack_size = KBRD_STACK_SIZE;
StackType_t keyboard_static_stack[KBRD_STACK_SIZE];
StaticTask_t keyboard_static_task;

// sensors task stuff
#define SENS_STACK_SIZE 2048
const uintptr_t sensors_stack_size = SENS_STACK_SIZE;
StackType_t sensors_static_stack[SENS_STACK_SIZE];
StaticTask_t sensors_static_task;

void app_main(void)
{
    // allocate and run hexowl task
    hexowl_static_stack = heap_caps_malloc(hexowl_task_stack_size, MALLOC_CAP_SPIRAM);
    if (hexowl_static_stack == NULL)
    {
        ESP_LOGE("main", "unable to allocate the hexowl task stack\r\n");
        goto error;
    }

    if (!xTaskCreateStaticPinnedToCore(
            hexowl_task, "hexowl",
            hexowl_task_stack_size, (void *)hexowl_heap_size,
            0, hexowl_static_stack, &hexowl_static_task, 1))
    {
        ESP_LOGE("main", "unable to run the hexowl task\r\n");
        goto error;
    }

    // run keyboard task
    if (!xTaskCreateStatic(
            keyboard_task, "keyboard",
            keyboard_stack_size, NULL,
            0, keyboard_static_stack, &keyboard_static_task))
    {
        ESP_LOGE("main", "unable to run the keyboard task\r\n");
        goto error;
    }

    // run sensors task
    if (!xTaskCreateStatic(
            sensors_task, "sensors",
            sensors_stack_size, NULL,
            0, sensors_static_stack, &sensors_static_task))
    {
        ESP_LOGE("main", "unable to run the sensors task\r\n");
        goto error;
    }

    // run ui task
    if (!xTaskCreateStatic(
            ui_task, "ui",
            ui_stack_size, NULL,
            0, ui_static_stack, &ui_static_task))
    {
        ESP_LOGE("main", "unable to run the ui task\r\n");
        goto error;
    }

    // main loop
    while (1)
    {
        vTaskDelay(15);
    }

error:
    while (1) vTaskDelay(1000);
}
