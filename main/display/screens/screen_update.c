#include "screen.h"

#include <stdio.h>
#include <string.h>

#include <esp_pm.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <ui.h>
#include <keyboard.h>
#include <sdcard.h>

#include "../ssd1322/ssd1322.h"
#include "../ssd1322/ssd1322_font.h"
#include "../fonts/cascadia_font.h"

extern SemaphoreHandle_t ui_refresh_sem;
extern ssd1322_t *ui_display;

static bool done = false;
static float progress = 0;
static TaskHandle_t upload_task;
static esp_pm_lock_handle_t pm_lock;

static void upload_task_handler(void *arg);
static void print_error(const char *msg);
static void draw_progress(float progress);

static bool init(void);
static void open(void);
static void draw(void);
static void close(void);

const ui_screen_t update_screen = {
    .init = init,
    .open = open,
    .draw = draw,
    .close = close};

static bool init(void)
{
    return esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "upd", &pm_lock) == ESP_OK;
}

static void open(void)
{
    esp_pm_lock_acquire(pm_lock);

    // clear screen and draw initial layout
    ssd1322_fill(ui_display, 0);
    ssd1322_draw_string(ui_display, 10, ui_display->res_y/2 - 20, "do not turn off the power!!!", cascadia_font);

    done = false;
    progress = 0;
    draw_progress(0);

    if (xTaskCreate(upload_task_handler, "ota-upload", 8*1024, NULL, 0, &upload_task) != pdTRUE)
    {
        ESP_LOGE("upd_scr", "unable to create upload task");
    }
}

static void draw(void)
{
    draw_progress(progress);
}

static void close(void)
{
    esp_pm_lock_release(pm_lock);
}

static void upload_task_handler(void *arg)
{
    const size_t read_buff_size = 2048;
    uint8_t *read_buff = NULL;
    int total_size = 0;
    int read_size = 0;
    int updated_size = 0;

    esp_ota_handle_t update_handle;
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL)
    {
        print_error("no update partiotion");
        goto error;
    }

    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI("upd_scr", "running firmware version: %s", running_app_info.version);
    }

    if (!sdcard_is_inserted())
    {
        ESP_LOGW("upd_scr", "SD card not inserted");
        print_error("SD card not inserted");
        goto error;
    }

    if (sdcard_mount() != SD_OK)
    {
        ESP_LOGW("upd_scr", "SD card mount error");
        print_error("SD card mount error");
        goto error;
    }

    total_size = sdcard_file_size("hard-hexowl.bin");
    if (total_size <= 0)
    {
        sdcard_unmount();
        ESP_LOGW("upd_scr", "there is no bin file");
        print_error("there is no bin file");
        goto error;
    }

    if (sdcard_open("hard-hexowl.bin", "r") != SD_OK)
    {
        sdcard_unmount();
        ESP_LOGW("upd_scr", "unable to open firmware file");
        print_error("unable to open firmware file");
        goto error;
    }

    read_buff = malloc(read_buff_size);
    if (read_buff == NULL)
    {
        sdcard_unmount();
        ESP_LOGW("upd_scr", "failed to allocate memory");
        print_error("failed to allocate memory");
        goto error;
    }

    if ((read_size = sdcard_read(read_buff, read_buff_size)) < read_buff_size)
    {
        sdcard_unmount();
        ESP_LOGW("upd_scr", "firmware read error (read: %d, want: %d)", read_size, read_buff_size);
        print_error("firmware read error");
        goto error;
    }

    esp_app_desc_t *new_app_info = (esp_app_desc_t*)&read_buff[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)];
    ESP_LOGI("upd_scr", "new firmware version: %s", new_app_info->version);

    if (strlen(new_app_info->version) <= strlen(running_app_info.version) && strcmp(new_app_info->version, running_app_info.version) <= 0)
    {
        sdcard_unmount();
        ESP_LOGW("upd_scr", "versions are the same, skip update");
        print_error("already up to date");
        goto error;
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK)
    {
        sdcard_unmount();
        ESP_LOGE("upd_scr", "esp_ota_begin failed (%s)", esp_err_to_name(err));
        print_error("update begin failed");
        goto error;
    }

    while (1)
    {
        err = esp_ota_write(update_handle, read_buff, read_size);
        if (err != ESP_OK)
        {
            sdcard_unmount();
            esp_ota_abort(update_handle);
            ESP_LOGE("upd_scr", "esp_ota_write failed (%s)", esp_err_to_name(err));
            print_error("update begin failed");
            goto error;
        }

        updated_size += read_size;
        progress = (float)(updated_size) / (float)(total_size);
        xSemaphoreGive(ui_refresh_sem);
        vTaskDelay(1);

        read_size = sdcard_read(read_buff, read_buff_size);
        if (read_size <= 0)
        {
            break;
        }
    }

    sdcard_close();
    sdcard_unmount();

    if (read_size < 0)
    {
        esp_ota_abort(update_handle);
        print_error("firmware read failed");
        goto error;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE("upd_scr", "validation failed, image is corrupted");
            print_error("firmware is corrupted");
        }
        else
        {
            ESP_LOGE("upd_scr", "esp_ota_end failed (%s)!", esp_err_to_name(err));
            print_error("firmware flash error");
        }
        goto error;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE("upd_scr", "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        print_error("set boot partition error");
        goto error;
    }

    ESP_LOGI("upd_scr", "update done");

    done = true;
    progress = 1;
    free(read_buff);
    xSemaphoreGive(ui_refresh_sem);

    vTaskDelay(1800);
    ESP_LOGW("upd_scr", "restarting...");
    esp_restart();

    vTaskDelete(NULL);
    return;

error:
    if (read_buff != NULL)
    {
        free(read_buff);
    }

    xSemaphoreGive(ui_refresh_sem);
    vTaskDelay(2500);
    ui_change_screen(SCREEN_CALCULATION);
    vTaskDelete(NULL);
}

static void print_error(const char *msg)
{
    ssd1322_fill(ui_display, 0);
    ssd1322_draw_string(ui_display, 10, ui_display->res_y/2 + 8, msg, cascadia_font);
}

static void draw_progress(float progress)
{
    if (progress < 0)
    {
        progress = 0;
    }
    else if (progress > 1)
    {
        progress = 1;
    }

    int bar_width = (ui_display->res_x - 22);
    int progress_width = bar_width * progress;
    int progress_fade = (float)bar_width * progress - progress_width;

    int half_y = ui_display->res_y / 2;

    // frame
    ssd1322_draw_rect(ui_display, 10, half_y - 4, bar_width+2, 8, 14);
    // progress infill
    ssd1322_draw_rect_filled(ui_display, 11, half_y - 3, progress_width, 6, 10);

    ESP_LOGI("upd_scr", "progress %d (%d/%d)", (int)(progress * 100), progress_width, bar_width);

    if (progress_width < bar_width)
    {
        if (bar_width - progress_width > 1)
        {
            // progress bg
            ssd1322_draw_rect_filled(ui_display, 11, half_y - 3, progress_width, 6, 4);
        }
        if (progress_fade > 0)
        {
            // progress fade
            ssd1322_draw_vline(ui_display, half_y - 3, half_y + 3, progress_width + 1, 4 + (6 * progress_fade));
        }
    }

    if (progress == 1)
    {
        ssd1322_draw_string(ui_display, 10, half_y + 8, "done, restarting...", cascadia_font);
    }
}
