#include "ui.h"

#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <keyboard.h>
#include <sensors.h>

#include "ssd1322/ssd1322.h"
#include "ssd1322/ssd1322_font.h"
#include "bitmaps/hexowl_logo.h"
#include "fonts/cascadia_font.h"

typedef struct {
    int pos_x;
    int pos_y;
    int size_x;
    int size_y;
} key_visual_t;

static ssd1322_t *display;
static const int res_x = 256, res_y = 64;
static const ssd1322_pinmap_t pinmap = {
    .reset = 22,
    .dc = 21,
    .cs = 5,
};

static spi_host_device_t spi_host = SPI3_HOST;
static const spi_bus_config_t spi_bus_cfg = {
    .miso_io_num = -1,
    .mosi_io_num = 23,
    .sclk_io_num = 18,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 8192,
    .flags = SPICOMMON_BUSFLAG_MASTER,
};

static key_visual_t keyboard_layout[KEY_COUNT];
static SemaphoreHandle_t refresh_sem;
static char text_buffer[4096];

static void key_toggle_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void create_keyboard_layout();
static void draw_keyboard_layout();

static char vbat_text_buffer[16];
static char chrg_text_buffer[16];
static void sensor_vbat_callback(sens_t sensor, float value);
static void sensor_chrg_callback(sens_t sensor, float value);

void ui_task(void *arg)
{
    esp_err_t err;

    err = spi_bus_initialize(spi_host, &spi_bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        ESP_LOGE("disp", "SPI bus initialization error");
        goto error;
    }

    display = ssd1322_init(spi_host, pinmap, res_x, res_y);
    if (display == NULL)
    {
        ESP_LOGE("disp", "display initialization error");
        goto error;
    }

    refresh_sem = xSemaphoreCreateBinary();
    if (refresh_sem == NULL)
    {
        ESP_LOGE("disp", "semaphore creation error");
        goto error;
    }

    // register sensors callbacks
    sensors_register_callback(SENS_VBAT, sensor_vbat_callback);
    sensors_register_callback(SENS_CHRG, sensor_chrg_callback);

    // register keyboard callbacks
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        keyboard_register_callback(i, KEY_TOGGLED, key_toggle_callback);
    }

    ssd1322_draw_bitmap_4bit(display, 0, 0, hexowl_logo_map, hexowl_logo_size_x, hexowl_logo_size_y);
    ssd1322_send_framebuffer(display);

    vTaskDelay(100);
    ssd1322_fill(display, 0);

    create_keyboard_layout();
    draw_keyboard_layout();

    ssd1322_draw_string(display, 168, 4,  "VBat:", &cascadia_font);
    sensor_vbat_callback(SENS_VBAT, sensors_get_value(SENS_VBAT));
    ssd1322_draw_string(display, 168, 20, "Chrg:", &cascadia_font);
    sensor_chrg_callback(SENS_CHRG, sensors_get_value(SENS_CHRG));
    ssd1322_send_framebuffer(display);

    while (1)
    {
        if (xSemaphoreTake(refresh_sem, 250))
        {
            ssd1322_send_framebuffer(display);
        }
    }

error:
    while (1) vTaskDelay(1000);
}

static void key_toggle_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (pressed)
        ssd1322_draw_rect_filled(display, keyboard_layout[k].pos_x+1, keyboard_layout[k].pos_y+1, keyboard_layout[k].size_x-2, keyboard_layout[k].size_y-2, 8);
    else
        ssd1322_draw_rect_filled(display, keyboard_layout[k].pos_x+1, keyboard_layout[k].pos_y+1, keyboard_layout[k].size_x-2, keyboard_layout[k].size_y-2, 0);

    xSemaphoreGive(refresh_sem);
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
        ssd1322_draw_rect(display, keyboard_layout[i].pos_x, keyboard_layout[i].pos_y, keyboard_layout[i].size_x, keyboard_layout[i].size_y, 15);
    }
}

static void sensor_vbat_callback(sens_t sensor, float value)
{
    sprintf(vbat_text_buffer, "%.02f", value);
    ssd1322_draw_string(display, 216, 4, vbat_text_buffer, &cascadia_font);
    xSemaphoreGive(refresh_sem);
}

static void sensor_chrg_callback(sens_t sensor, float value)
{
    sprintf(chrg_text_buffer, "%.02f", value);
    ssd1322_draw_string(display, 216, 20, chrg_text_buffer, &cascadia_font);
    xSemaphoreGive(refresh_sem);
}
