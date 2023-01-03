#include "screen.h"

#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <keyboard.h>
#include <sensors.h>

#include "../ssd1322/ssd1322.h"
#include "../ssd1322/ssd1322_font.h"
#include "../bitmaps/hexowl_logo.h"
#include "../fonts/cascadia_font.h"

extern SemaphoreHandle_t ui_refresh_sem;
static ssd1322_t *display;

typedef struct {
    int pos_x;
    int pos_y;
    int size_x;
    int size_y;
} key_visual_t;

static key_visual_t keyboard_layout[KEY_COUNT];
static void key_toggle_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void create_keyboard_layout();
static void draw_keyboard_layout();

static char vbat_text_buffer[16];
static char chrg_text_buffer[16];
static void sensor_vbat_callback(sens_t sensor, float value);
static void sensor_chrg_callback(sens_t sensor, float value);

static bool init(void *arg);
static void open(void);
static void draw(void);
static void close(void);

const ui_screen_t test_screen = {
    .init = init,
    .open = open,
    .draw = draw,
    .close = close
};

static bool init(void *arg)
{
    display = (ssd1322_t *)arg;
    create_keyboard_layout();
    return true;
}

static void open(void)
{
    // clear screen and draw initial info
    ssd1322_fill(display, 0);
    draw_keyboard_layout();

    ssd1322_draw_string(display, 168, 4, "VBat:", &cascadia_font);
    sensor_vbat_callback(SENS_VBAT, sensors_get_value(SENS_VBAT));
    ssd1322_draw_string(display, 168, 20, "Chrg:", &cascadia_font);
    sensor_chrg_callback(SENS_CHRG, sensors_get_value(SENS_CHRG));

    // register sensors callbacks
    sensors_register_callback(SENS_VBAT, sensor_vbat_callback);
    sensors_register_callback(SENS_CHRG, sensor_chrg_callback);

    // register keyboard callbacks
    for (int i = 0; i < KEY_COUNT; ++i)
    {
        keyboard_register_callback(i, KEY_TOGGLED, key_toggle_callback);
    }
}

static void draw(void)
{

}

static void close(void)
{

}

static void key_toggle_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (pressed)
        ssd1322_draw_rect_filled(display, keyboard_layout[k].pos_x + 1, keyboard_layout[k].pos_y + 1, keyboard_layout[k].size_x - 2, keyboard_layout[k].size_y - 2, 8);
    else
        ssd1322_draw_rect_filled(display, keyboard_layout[k].pos_x + 1, keyboard_layout[k].pos_y + 1, keyboard_layout[k].size_x - 2, keyboard_layout[k].size_y - 2, 0);

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
        ssd1322_draw_rect(display, keyboard_layout[i].pos_x, keyboard_layout[i].pos_y, keyboard_layout[i].size_x, keyboard_layout[i].size_y, 15);
    }
}

static void sensor_vbat_callback(sens_t sensor, float value)
{
    sprintf(vbat_text_buffer, "%.02f", value);
    ssd1322_draw_string(display, 216, 4, vbat_text_buffer, &cascadia_font);
    xSemaphoreGive(ui_refresh_sem);
}

static void sensor_chrg_callback(sens_t sensor, float value)
{
    sprintf(chrg_text_buffer, "%.02f", value);
    ssd1322_draw_string(display, 216, 20, chrg_text_buffer, &cascadia_font);
    xSemaphoreGive(ui_refresh_sem);
}
