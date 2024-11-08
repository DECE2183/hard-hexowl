#include "screen.h"

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_private/esp_clk.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <keyboard.h>
#include <sensors.h>

#include "../ssd1322/ssd1322.h"
#include "../ssd1322/ssd1322_font.h"
#include "../fonts/cascadia_font.h"

extern SemaphoreHandle_t ui_refresh_sem;
extern ssd1322_t *ui_display;

typedef struct {
    int pos_x;
    int pos_y;
    int size_x;
    int size_y;
} key_visual_t;

typedef struct {
    kbrd_key_t key;
    bool pressed;
} key_status_t;

static key_visual_t keyboard_layout[KEY_COUNT];
static QueueHandle_t keyboard_queue;
static TaskHandle_t cpu_freq_task;
static void key_toggle_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void create_keyboard_layout();
static void draw_keyboard_layout();

static char cpu_text_buffer[16];
static char vbat_text_buffer[16];
static char chrg_text_buffer[16];
static void sensor_vbat_callback(sens_t sensor, float value);
static void sensor_chrg_callback(sens_t sensor, float value);
static void sensor_is_chrg_callback(sens_t sensor, float value);
static void cpu_freq_display(void *arg);

static bool init(void);
static void open(void);
static void draw(void);
static void close(void);

const ui_screen_t test_screen = {
    .init = init,
    .open = open,
    .draw = draw,
    .close = close
};

static bool init(void)
{
    keyboard_queue = xQueueCreate(16, sizeof(key_status_t));
    create_keyboard_layout();
    return true;
}

static void open(void)
{
    // clear screen and draw initial layout
    ssd1322_fill(ui_display, 0);
    draw_keyboard_layout();

    esp_app_desc_t running_app_info;
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    if (esp_ota_get_partition_description(running_part, &running_app_info) != ESP_OK)
    {
        strcpy(running_app_info.version, "err");
    }

    ssd1322_draw_string(ui_display, 168, 2, running_app_info.version, cascadia_font);
    ssd1322_draw_string(ui_display, 168, 14, "CPU:", cascadia_font);

    ssd1322_draw_string(ui_display, 168, 26, "VBat:", cascadia_font);
    sensor_vbat_callback(SENS_VBAT, sensors_get_value(SENS_VBAT));
    ssd1322_draw_string(ui_display, 168, 38, "Chrg:", cascadia_font);
    sensor_chrg_callback(SENS_CHRG, sensors_get_value(SENS_CHRG));
    ssd1322_draw_string(ui_display, 168, 50, "IsCh:", cascadia_font);
    sensor_is_chrg_callback(SENS_BAT_CHARGING, sensors_get_value(SENS_BAT_CHARGING));

    // register sensors callbacks
    sensors_register_callback(SENS_VBAT, sensor_vbat_callback);
    sensors_register_callback(SENS_CHRG, sensor_chrg_callback);
    sensors_register_callback(SENS_BAT_CHARGING, sensor_is_chrg_callback);

    // register keyboard callbacks
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        keyboard_register_callback(i, KEY_TOGGLED, key_toggle_callback);
    }

    xTaskCreatePinnedToCore(cpu_freq_display, "cpu-freq", 4096, NULL, 0, &cpu_freq_task, 0);
}

static void draw(void)
{
    static key_status_t k;
    static key_visual_t *v;

    while (xQueueReceive(keyboard_queue, &k, 0))
    {
        v = &keyboard_layout[k.key];

        if (k.pressed)
            ssd1322_draw_rect_filled(ui_display, v->pos_x + 1, v->pos_y + 1, v->size_x - 2, v->size_y - 2, 8);
        else
            ssd1322_draw_rect_filled(ui_display, v->pos_x + 1, v->pos_y + 1, v->size_x - 2, v->size_y - 2, 0);
    }
}

static void close(void)
{
    // unregister sensors callbacks
    sensors_register_callback(SENS_VBAT, NULL);
    sensors_register_callback(SENS_CHRG, NULL);
    sensors_register_callback(SENS_BAT_CHARGING, NULL);

    // unregister keyboard callbacks
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        keyboard_register_callback(i, KEY_TOGGLED, NULL);
    }

    vTaskDelete(cpu_freq_task);
}

static void key_toggle_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    static key_status_t _k;
    
    _k.key = k;
    _k.pressed = pressed;

    xQueueSend(keyboard_queue, &_k, 0);
    xSemaphoreGive(ui_refresh_sem);
}

#define set_layout(x_mul)                           \
    keyboard_layout[key].pos_x = x;                 \
    keyboard_layout[key].pos_y = y;                 \
    keyboard_layout[key].size_x = key_size * x_mul; \
    keyboard_layout[key].size_y = key_size

static void create_keyboard_layout()
{
    static const int key_size = 12;
    static const int x_begin = 4;
    static const int y_begin = 4;

    kbrd_key_t key = 0;
    int x = x_begin;
    int y = y_begin;

    // 1-st row
    for (int i = 0; i < 12; ++i)
    {
        set_layout(1);
        x += key_size;
        ++key;
    }
    set_layout(1.25);
    ++key;

    // 2-nd row
    x = x_begin + key_size * 0.25;
    y += key_size;
    for (int i = 0; i < 13; ++i)
    {
        set_layout(1);
        x += key_size;
        ++key;
    }

    // 3-rd row
    x = x_begin + key_size * 0.5;
    y += key_size;
    for (int i = 0; i < 11; ++i)
    {
        set_layout(1);
        x += key_size;
        ++key;
    }
    set_layout(1.75);
    ++key;

    // 4-th row
    x = x_begin;
    y += key_size;
    for (int i = 0; i < 12; ++i)
    {
        set_layout(1);
        x += key_size;
        ++key;
    }
    set_layout(1.25);
    ++key;

    // 5-th row
    x = x_begin;
    y += key_size;
    for (int i = 0; i < 2; ++i)
    {
        set_layout(1.25);
        x += key_size * 1.25;
        ++key;
    }
    set_layout(6.25);
    ++key;

    x = x_begin + key_size * 9;
    for (int i = 0; i < 4; ++i)
    {
        set_layout(1);
        x += key_size;
        ++key;
    }
}

static void draw_keyboard_layout()
{
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        ssd1322_draw_rect(ui_display, keyboard_layout[i].pos_x, keyboard_layout[i].pos_y, keyboard_layout[i].size_x, keyboard_layout[i].size_y, 15);
    }
}

static void cpu_freq_display(void *arg)
{
    while(1)
    {
        sprintf(cpu_text_buffer, "%uMHz", esp_clk_cpu_freq()/1000000);
        ssd1322_draw_rect_filled(ui_display, 210, 14, 40, 12, 0);
        ssd1322_draw_string(ui_display, 210, 14, cpu_text_buffer, cascadia_font);
        xSemaphoreGive(ui_refresh_sem);
        vTaskDelay(1000);
    }
}

static void sensor_vbat_callback(sens_t sensor, float value)
{
    sprintf(vbat_text_buffer, "%.02f", value);
    ssd1322_draw_rect_filled(ui_display, 210, 26, 40, 12, 0);
    ssd1322_draw_string(ui_display, 210, 26, vbat_text_buffer, cascadia_font);
    xSemaphoreGive(ui_refresh_sem);
}

static void sensor_chrg_callback(sens_t sensor, float value)
{
    sprintf(chrg_text_buffer, "%.02f", value);
    ssd1322_draw_rect_filled(ui_display, 210, 38, 40, 12, 0);
    ssd1322_draw_string(ui_display, 210, 38, chrg_text_buffer, cascadia_font);
    xSemaphoreGive(ui_refresh_sem);
}

static void sensor_is_chrg_callback(sens_t sensor, float value)
{
    ssd1322_draw_rect_filled(ui_display, 210, 50, 40, 12, 0);

    if (value > 0)
        ssd1322_draw_string(ui_display, 210, 50, "YES", cascadia_font);
    else
        ssd1322_draw_string(ui_display, 210, 50, "NO", cascadia_font);

    xSemaphoreGive(ui_refresh_sem);
}
