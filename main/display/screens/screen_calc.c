#include "screen.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <keyboard.h>
#include <sensors.h>
#include <calc.h>

#include "../ssd1322/ssd1322.h"
#include "../ssd1322/ssd1322_font.h"
#include "../ssd1322/ssd1322_bitmap.h"
#include "../fonts/cascadia_font.h"
#include "../bitmaps/icons/charge_bmp.h"
#include "../bitmaps/icons/battery_bmp.h"
#include "../bitmaps/icons/enter_pressed_bmp.h"
#include "../bitmaps/icons/enter_released_bmp.h"

#define OUTPUT_BUFFER_LEN (1024 * 16)
#define OUTPUT_BUFFER_SCROLL_STEP (4)
#define INPUT_HISTORY_DEPTH (16)
#define INPUT_BUFFER_LEN (1024)

typedef struct {
    char str[INPUT_BUFFER_LEN+1];
    int len;
} ui_input_str_t;

extern SemaphoreHandle_t ui_refresh_sem;
extern ssd1322_t *ui_display;

static char text_buffer[INPUT_BUFFER_LEN*2];

static char output_buffer[OUTPUT_BUFFER_LEN+1] = {'\0'};
static int output_buffer_len = 0;
static int output_buffer_lines_cnt = 0;
static char *output_line_begin = &output_buffer[0];
static int output_buffer_scroll = 0;

static ui_input_str_t input_buffer[INPUT_HISTORY_DEPTH+1];
static int input_history_len = 1;
static int input_history_pos = 0;
static int input_cursor = 0;

static void register_text_key_callbacks(kbrd_key_state_t state, kbrd_callback_t callback);
static void register_navigation_key_callbacks(kbrd_key_state_t state, kbrd_callback_t callback);
static void text_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void navigation_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

static void backspace_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void enter_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void enter_key_released_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

static float last_bat_level;
static int last_bat_is_charge;
static void battery_change_callback(sens_t sensor, float value);

static void output_string(const char *str);
static void input_take_history(void);
static void input_push_history(void);

static void proccess_input_navigation(kbrd_key_t k, kbrd_key_state_t s);
static void proccess_output_navigation(kbrd_key_t k, kbrd_key_state_t s);

static void draw_battery_level(void);
static void draw_output_scrollbar(void);

static TaskHandle_t bg_task_handle;
static void bg_task(void *arg);

static bool init(void);
static void open(void);
static void draw(void);
static void close(void);

const ui_screen_t calc_screen = {
    .init = init,
    .open = open,
    .draw = draw,
    .close = close
};

static bool init(void)
{
    if (!xTaskCreate(bg_task, "disp-bg", 1024, NULL, 0, &bg_task_handle))
        return false;
    
    vTaskSuspend(bg_task_handle);
    return true;
}

static void open(void)
{
    // clear screen and draw initial layout
    ssd1322_fill(ui_display, 0);
    ssd1322_draw_hline(ui_display, 0, ui_display->res_x, ui_display->res_y - 15, 8);

    // register keyboard callbacks
    register_text_key_callbacks(KEY_PRESSED, text_key_pressed_callback);
    register_text_key_callbacks(KEY_DOWN, text_key_pressed_callback);
    register_navigation_key_callbacks(KEY_PRESSED, navigation_key_pressed_callback);
    register_navigation_key_callbacks(KEY_DOWN, navigation_key_pressed_callback);
    keyboard_register_callback(KEY_BACKSPACE, KEY_PRESSED, backspace_key_pressed_callback);
    keyboard_register_callback(KEY_BACKSPACE, KEY_DOWN, backspace_key_pressed_callback);
    keyboard_register_callback(KEY_ENTER, KEY_PRESSED, enter_key_pressed_callback);
    keyboard_register_callback(KEY_ENTER, KEY_RELEASED, enter_key_released_callback);

    // register sensors callbacks
    sensors_register_callback(SENS_BAT_LEVEL, battery_change_callback);
    sensors_register_callback(SENS_BAT_CHARGING, battery_change_callback);

    // uopdate sensor values
    last_bat_level = sensors_get_value(SENS_BAT_LEVEL);
    last_bat_is_charge = sensors_get_value(SENS_BAT_CHARGING);

    vTaskResume(bg_task_handle);
    xSemaphoreGive(ui_refresh_sem);
}

static void draw(void)
{
    static int cursor_overflow = 0;
    static int out_y = 0;
    static char *out_nl = NULL;

    // clear output field
    ssd1322_draw_rect_filled(ui_display, 0, 0, ui_display->res_x, ui_display->res_y - 15, 0);

    // draw output
    out_y = 4;
    out_nl = output_line_begin;
    while (out_y - output_buffer_scroll < ui_display->res_y - 14)
    {
        ssd1322_draw_string(ui_display, 4, out_y - output_buffer_scroll, out_nl, cascadia_font);

        out_nl = strchr(out_nl, '\n');
        if (out_nl == NULL)
            break;
        else
            ++out_nl;

        out_y += 12;
    }

    // draw ui
    draw_output_scrollbar();
    draw_battery_level();

    // clear input field
    ssd1322_draw_rect_filled(ui_display, 0, ui_display->res_y - 14, ui_display->res_x, 14, 0);
    ssd1322_draw_hline(ui_display, 0, ui_display->res_x, ui_display->res_y - 15, 8);

    cursor_overflow = input_cursor - (ui_display->res_x - 40) / 8;
    if (cursor_overflow > 0)
    {
        // draw input string
        ssd1322_draw_string(ui_display, 4, ui_display->res_y - 14, &input_buffer[input_history_pos].str[cursor_overflow], cascadia_font);
        // draw cursor underline
        ssd1322_draw_hline(ui_display, 4 + (input_cursor - cursor_overflow) * 8, 12 + (input_cursor - cursor_overflow) * 8, ui_display->res_y - 1, 3);
        // draw fade effect
        ssd1322_draw_vline(ui_display, ui_display->res_y - 14, ui_display->res_y - 1, 0, 5);
        ssd1322_draw_vline(ui_display, ui_display->res_y - 14, ui_display->res_y - 1, 1, 4);
        ssd1322_draw_vline(ui_display, ui_display->res_y - 14, ui_display->res_y - 1, 2, 3);
        ssd1322_draw_vline(ui_display, ui_display->res_y - 14, ui_display->res_y - 1, 3, 2);
        ssd1322_draw_vline(ui_display, ui_display->res_y - 14, ui_display->res_y - 1, 4, 1);
    }
    else
    {
        // draw input string
        ssd1322_draw_string(ui_display, 4, ui_display->res_y - 14, input_buffer[input_history_pos].str, cascadia_font);
        // draw cursor underline
        ssd1322_draw_hline(ui_display, 4 + input_cursor * 8, 12 + input_cursor * 8, ui_display->res_y - 1, 3);
    }

    // draw enter icon
    if (keyboard_is_key_pressed(KEY_ENTER))
        ssd1322_draw_bitmap(ui_display, ui_display->res_x - 24, ui_display->res_y - 14, enter_pressed);
    else
        ssd1322_draw_bitmap(ui_display, ui_display->res_x - 24, ui_display->res_y - 14, enter_released);
}

static void close(void)
{
    // unregister keyboard callbacks
    register_text_key_callbacks(KEY_PRESSED, NULL);
    register_text_key_callbacks(KEY_DOWN, NULL);
    register_navigation_key_callbacks(KEY_PRESSED, NULL);
    register_navigation_key_callbacks(KEY_DOWN, NULL);
    keyboard_register_callback(KEY_BACKSPACE, KEY_PRESSED, NULL);
    keyboard_register_callback(KEY_BACKSPACE, KEY_DOWN, NULL);
    keyboard_register_callback(KEY_ENTER, KEY_PRESSED, NULL);
    keyboard_register_callback(KEY_ENTER, KEY_RELEASED, NULL);

    // unregister sensors callbacks
    sensors_register_callback(SENS_BAT_LEVEL, NULL);
    sensors_register_callback(SENS_BAT_CHARGING, NULL);

    vTaskSuspend(bg_task_handle);
}

static void register_text_key_callbacks(kbrd_key_state_t state, kbrd_callback_t callback)
{
    for (kbrd_key_t k = KEY_1; k < KEY_BACKSPACE; ++k)
        keyboard_register_callback(k, state, callback);

    for (kbrd_key_t k = KEY_Q; k < KEY_ENTER; ++k)
        keyboard_register_callback(k, state, callback);

    for (kbrd_key_t k = KEY_Z; k < KEY_RSHIFT; ++k)
        keyboard_register_callback(k, state, callback);

    keyboard_register_callback(KEY_SPACE, state, callback);
}

static void register_navigation_key_callbacks(kbrd_key_state_t state, kbrd_callback_t callback)
{
    keyboard_register_callback(KEY_ARROW_LEFT, state, callback);
    keyboard_register_callback(KEY_ARROW_RIGHT, state, callback);
    keyboard_register_callback(KEY_ARROW_UP, state, callback);
    keyboard_register_callback(KEY_ARROW_DOWN, state, callback);
}

static void text_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (input_cursor >= INPUT_BUFFER_LEN-1) return;

    if (input_history_pos > 0)
        input_take_history();

    if (input_buffer[0].str[input_cursor] != '\0')
    {
        memmove(&input_buffer[0].str[input_cursor + 1], &input_buffer[0].str[input_cursor], input_buffer[0].len - input_cursor + 1);
    }

    input_buffer[0].str[input_cursor] = keyboard_key_to_char(k, keyboard_is_key_pressed(KEY_LSHIFT) || keyboard_is_key_pressed(KEY_RSHIFT));
    ++input_buffer[0].len;
    ++input_cursor;

    xSemaphoreGive(ui_refresh_sem);
}

static void navigation_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (keyboard_is_key_pressed(KEY_ALT))
        proccess_output_navigation(k, s);
    else
        proccess_input_navigation(k, s);

    xSemaphoreGive(ui_refresh_sem);
}

static void backspace_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (input_cursor <= 0) return;

    if (input_history_pos > 0)
        input_take_history();

    if (input_buffer[0].str[input_cursor] != '\0')
    {
        memmove(&input_buffer[0].str[input_cursor - 1], &input_buffer[0].str[input_cursor], input_buffer[0].len - input_cursor + 1);
    }
    else
    {
        input_buffer[0].str[input_cursor - 1] = '\0';
    }

    --input_buffer[0].len;
    --input_cursor;

    xSemaphoreGive(ui_refresh_sem);
}

static void enter_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    xSemaphoreGive(ui_refresh_sem);
}

static void enter_key_released_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (input_history_pos > 0)
        input_take_history();

    sprintf(text_buffer, ">: %s", input_buffer[0].str);
    output_string(text_buffer);
    calc_expression(input_buffer[0].str);
    output_string(calc_await_expression());
    output_buffer_scroll -= 12;

    input_push_history();
    xSemaphoreGive(ui_refresh_sem);
}

static void battery_change_callback(sens_t sensor, float value)
{
    if (sensor == SENS_BAT_LEVEL)
        last_bat_level = value;
    else
        last_bat_is_charge = value;

    xSemaphoreGive(ui_refresh_sem);
}

static void output_string(const char *str)
{
    if (str == NULL) return;

    int lines_cnt = 0;
    const char *s;

    // count new line symbols
    for (s = str; *s != '\0'; ++s)
    {
        if (*s == '\n')
            ++lines_cnt;
    }
    output_buffer_lines_cnt += lines_cnt;

    int str_len = s - str;

    // free space for new string
    if (output_buffer_len + str_len > OUTPUT_BUFFER_LEN)
    {
        int dif_len = (output_buffer_len + str_len + 1) - OUTPUT_BUFFER_LEN;
        memmove(output_buffer, &output_buffer[dif_len], output_buffer_len - dif_len);
        output_buffer_len -= dif_len;
    }

    memcpy(&output_buffer[output_buffer_len], str, str_len);
    output_buffer_len += str_len;

    // insert last new line symbol if missed
    if (str[str_len] != '\n')
    {
        output_buffer[output_buffer_len++] = '\n';
        output_buffer_lines_cnt += 1;
    }

    output_buffer[output_buffer_len] = '\0';

    // autoscroll
    output_buffer_scroll = 12 * output_buffer_lines_cnt - (ui_display->res_y - 20);
    if (output_buffer_scroll < 0)
        output_buffer_scroll = 0;
}

static void input_take_history(void)
{
    memcpy(&input_buffer[0], &input_buffer[input_history_pos], sizeof(ui_input_str_t));
    input_history_pos = 0;
}

static void input_push_history(void)
{
    if (input_buffer[0].len == 0) return;

    ++input_history_len;
    if (input_history_len > INPUT_HISTORY_DEPTH)
        input_history_len = INPUT_HISTORY_DEPTH;

    memmove(&input_buffer[1], &input_buffer[0], sizeof(ui_input_str_t) * input_history_len);

    input_cursor = 0;
    input_buffer[0].len = 0;
    memset(input_buffer[0].str, 0, INPUT_BUFFER_LEN);
}

static void proccess_input_navigation(kbrd_key_t k, kbrd_key_state_t s)
{
    switch (k)
    {
    case KEY_ARROW_LEFT:
        --input_cursor;
        goto cursor_move;
    case KEY_ARROW_RIGHT:
        ++input_cursor;
        goto cursor_move;
    case KEY_ARROW_UP:
        ++input_history_pos;
        goto history_move;
    case KEY_ARROW_DOWN:
        --input_history_pos;
        goto history_move;
    default:
        return;
    }

cursor_move:
    if (input_cursor < 0)
        input_cursor = 0;
    else if (input_cursor > input_buffer[input_history_pos].len)
        input_cursor = input_buffer[input_history_pos].len;
    return;

history_move:
    if (input_history_pos < 0)
        input_history_pos = 0;
    else if (input_history_pos > input_history_len-1)
        input_history_pos = input_history_len-1;
    input_cursor = input_buffer[input_history_pos].len;
    return;
}

static void proccess_output_navigation(kbrd_key_t k, kbrd_key_state_t s)
{
    static float scroll_step = OUTPUT_BUFFER_SCROLL_STEP;

    if (s == KEY_PRESSED)
        scroll_step = OUTPUT_BUFFER_SCROLL_STEP;
    else
        scroll_step += 0.25;

    switch (k)
    {
    case KEY_ARROW_LEFT:
        break;
    case KEY_ARROW_RIGHT:
        break;
    case KEY_ARROW_UP:
        output_buffer_scroll -= scroll_step;
        if (output_buffer_scroll < 0)
            output_buffer_scroll = 0;
        break;
    case KEY_ARROW_DOWN:
        output_buffer_scroll += scroll_step;
        if (output_buffer_scroll > output_buffer_lines_cnt * 12 - 12)
            output_buffer_scroll = output_buffer_lines_cnt * 12 - 12;
        break;
    default:
        break;
    }
}

static void draw_battery_level(void)
{
    static int bat_id;

    if (last_bat_is_charge > 0)
    {
        ssd1322_draw_rect_filled(ui_display, ui_display->res_x - 26, 0, 9, 10, 0);
        ssd1322_draw_bitmap(ui_display, ui_display->res_x - 24, 1, charge_icon);
    }

    bat_id = roundf(6 * (last_bat_level / 100));
    ssd1322_draw_rect_filled(ui_display, ui_display->res_x - 17, 0, 14, 10, 0);
    ssd1322_draw_bitmap(ui_display, ui_display->res_x - 16, 2, battery_icons[bat_id]);
}

static void draw_output_scrollbar(void)
{
    static float thumb_size;
    static float thumb_pos;
    static float thumb_top_blend;
    static float thumb_bottom_blend;

    // calculate offsets
    if (output_buffer_lines_cnt*12 < ui_display->res_y - 16) return;
    thumb_size = (((float)ui_display->res_y - 16.0f) / (output_buffer_lines_cnt * 12.0f)) * ((float)ui_display->res_y - 19.0f);
    thumb_pos = (((float)output_buffer_scroll) / (output_buffer_lines_cnt * 12.0f)) * (((float)ui_display->res_y - 18.0f) - thumb_size);
    thumb_top_blend = thumb_pos;
    thumb_bottom_blend = thumb_size + thumb_pos;

    thumb_size = floorf(thumb_size + thumb_pos);
    thumb_pos = ceilf(thumb_pos);

    thumb_top_blend = thumb_pos - thumb_top_blend;
    thumb_bottom_blend -= thumb_size;

    if (thumb_size > ui_display->res_y - 19)
        thumb_size = ui_display->res_y - 19;

    // draw scrollbar bg
    ssd1322_draw_vline(ui_display, 2, ui_display->res_y - 17, ui_display->res_x - 3, 0);
    ssd1322_draw_vline(ui_display, 2, ui_display->res_y - 17, ui_display->res_x - 2, 2);
    ssd1322_draw_vline(ui_display, 2, ui_display->res_y - 17, ui_display->res_x - 1, 2);

    // draw scrollbar thumb
    ssd1322_draw_vline(ui_display, 2 + thumb_pos, 2 + thumb_size, ui_display->res_x - 2, 8);
    ssd1322_draw_vline(ui_display, 2 + thumb_pos, 2 + thumb_size, ui_display->res_x - 1, 8);

    // draw top blend line
    if (thumb_top_blend > 0 && thumb_pos > 0)
        ssd1322_draw_hline(ui_display, ui_display->res_x - 2, ui_display->res_x, 1 + thumb_pos, 2 + roundf(6 * thumb_top_blend));
    // draw bottom blend line
    if (thumb_bottom_blend > 0 && thumb_size < ui_display->res_y - 19)
        ssd1322_draw_hline(ui_display, ui_display->res_x - 2, ui_display->res_x, 2 + thumb_size, 2 + roundf(6 * thumb_bottom_blend));
}

static void bg_task(void *arg)
{
    while(1)
    {
        output_string(calc_await_output(150));
    }
}
