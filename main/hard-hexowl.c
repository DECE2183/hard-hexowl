#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_private/esp_clk.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ui.h>
#include <keyboard.h>
#include <sensors.h>
#include <calc.h>

// calc task stuff
extern void calc_task(void *arg);
const uintptr_t calc_task_stack_size = 2 * 1024 * 1024;
StaticTask_t calc_static_task;
StackType_t *calc_stack;
static calc_args_t calc_task_args = {
    .heap_size = 256 * 1024,
};

// ui task stuff
#define UI_STACK_SIZE (2048+256)
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
    esp_ota_img_states_t ota_state;
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_get_state_partition(running, &ota_state);

    // allocate and run calc task
    calc_stack = heap_caps_malloc(calc_task_stack_size, MALLOC_CAP_SPIRAM);
    if (calc_stack == NULL)
    {
        ESP_LOGE("main", "unable to allocate the calc task stack\r\n");
        goto error;
    }

    esp_app_desc_t *running_app_info = malloc(sizeof(esp_app_desc_t));
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    if (esp_ota_get_partition_description(running_part, running_app_info) != ESP_OK)
    {
        strcpy(running_app_info->version, "unknown");
    }

    calc_task_args.firmware_version = malloc((strlen(running_app_info->version)+1)*sizeof(char));
    strcpy(calc_task_args.firmware_version, running_app_info->version);
    free(running_app_info);

    if (!xTaskCreateStaticPinnedToCore(
            calc_task, "calc",
            calc_task_stack_size, (void *)&calc_task_args,
            0, calc_stack, &calc_static_task, 1))
    {
        ESP_LOGE("main", "unable to run the calc task\r\n");
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

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        // TODO: implement secure firmware validation!!!
        vTaskDelay(350);
        esp_ota_mark_app_valid_cancel_rollback();
    }

    // main loop
    while (1)
    {
        vTaskDelay(15);
    }

error:
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        ESP_LOGE("main", "diagnostics failed! start rollback to the previous version...");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    while (1) vTaskDelay(1000);
}
