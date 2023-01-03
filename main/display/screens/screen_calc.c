#include "screen.h"

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <keyboard.h>
#include <sensors.h>

#include "../ssd1322/ssd1322.h"
#include "../ssd1322/ssd1322_font.h"
#include "../fonts/cascadia_font.h"

#define OUTPUT_BUFFER_LEN (1024 * 16)
#define INPUT_BUFFER_LEN (1024)

extern SemaphoreHandle_t ui_refresh_sem;
extern ssd1322_t *ui_display;

static char output_buffer[OUTPUT_BUFFER_LEN];
static int output_buffer_len = 0;
static char *output_line_begin = &output_buffer[0];
static char input_buffer[INPUT_BUFFER_LEN];
static int input_buffer_len = 0;
static int input_cursor = 0;

static void register_text_key_callbacks(kbrd_key_state_t state, kbrd_callback_t callback);
static void register_navigation_key_callbacks(kbrd_key_state_t state, kbrd_callback_t callback);
static void text_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void navigation_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

static void backspace_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);
static void enter_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

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
    return true;
}

static void open(void)
{
    // clear screen and draw initial layout
    ssd1322_fill(ui_display, 0);
    ssd1322_draw_hline(ui_display, 0, ui_display->res_x, ui_display->res_y - 16, 8);

    // register keyboard callbacks
    register_text_key_callbacks(KEY_PRESSED, text_key_pressed_callback);
    register_navigation_key_callbacks(KEY_PRESSED, navigation_key_pressed_callback);
    keyboard_register_callback(KEY_BACKSPACE, KEY_PRESSED, backspace_key_pressed_callback);
    keyboard_register_callback(KEY_ENTER, KEY_PRESSED, enter_key_pressed_callback);

    xSemaphoreGive(ui_refresh_sem);
}

static void draw(void)
{
    static int cursor_overflow = 0;
    static int out_y = 0;
    static char *out_nl = NULL;

    // draw output
    out_y = 4;
    out_nl = output_line_begin;
    while (out_y < ui_display->res_y - 14)
    {
        ssd1322_draw_string(ui_display, 4, out_y, out_nl, cascadia_font);
        
        out_nl = strchr(out_nl, '\n');
        if (out_nl == NULL)
            break;
        else
            ++out_nl;

        out_y += 12;
    }

    // clear input field
    ssd1322_draw_rect_filled(ui_display, 0, ui_display->res_y - 15, ui_display->res_x, 15, 0);
    ssd1322_draw_hline(ui_display, 0, ui_display->res_x, ui_display->res_y - 16, 8);

    cursor_overflow = input_cursor - (ui_display->res_x - 16) / 8;
    if (cursor_overflow > 0)
    {
        // draw input string
        ssd1322_draw_string(ui_display, 4, ui_display->res_y - 14, &input_buffer[cursor_overflow], cascadia_font);
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
        ssd1322_draw_string(ui_display, 4, ui_display->res_y - 14, input_buffer, cascadia_font);
        // draw cursor underline
        ssd1322_draw_hline(ui_display, 4 + input_cursor * 8, 12 + input_cursor * 8, ui_display->res_y - 1, 3);
    }
}

static void close(void)
{
    // unregister keyboard callbacks
    register_text_key_callbacks(KEY_PRESSED, NULL);
    register_navigation_key_callbacks(KEY_PRESSED, NULL);
    keyboard_register_callback(KEY_BACKSPACE, KEY_PRESSED, NULL);
    keyboard_register_callback(KEY_ENTER, KEY_PRESSED, NULL);
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
}

static void text_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (input_cursor >= INPUT_BUFFER_LEN-1) return;

    if (input_buffer[input_cursor] != '\0')
    {
        memmove(&input_buffer[input_cursor+1], &input_buffer[input_cursor], input_buffer_len - input_cursor + 1);
    }

    input_buffer[input_cursor] = keyboard_key_to_char(k, keyboard_is_key_pressed(KEY_LSHIFT) || keyboard_is_key_pressed(KEY_RSHIFT));
    ++input_buffer_len;
    ++input_cursor;

    xSemaphoreGive(ui_refresh_sem);
}

static void navigation_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (keyboard_is_key_pressed(KEY_LSHIFT) || keyboard_is_key_pressed(KEY_RSHIFT))
    {
        switch (k)
        {
        case KEY_ARROW_LEFT:
            --input_cursor;
            break;
        case KEY_ARROW_RIGHT:
            ++input_cursor;
            break;
        default:
            break;
        }
    }
    else
    {
        switch (k)
        {
        case KEY_ARROW_LEFT:
            --input_cursor;
            break;
        case KEY_ARROW_RIGHT:
            ++input_cursor;
            break;
        default:
            break;
        }
    }

    if (input_cursor < 0)
    {
        input_cursor = 0;
    }
    else if (input_cursor > input_buffer_len)
    {
        input_cursor = input_buffer_len;
    }

    xSemaphoreGive(ui_refresh_sem);
}

static void backspace_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (input_cursor <= 0) return;

    if (input_buffer[input_cursor] != '\0')
    {
        memmove(&input_buffer[input_cursor-1], &input_buffer[input_cursor], input_buffer_len - input_cursor + 1);
    }
    else
    {
        input_buffer[input_cursor-1] = '\0';
    }

    --input_buffer_len;
    --input_cursor;

    xSemaphoreGive(ui_refresh_sem);
}

static void enter_key_pressed_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    memcpy(&output_buffer[output_buffer_len], input_buffer, input_buffer_len);
    output_buffer_len += input_buffer_len+1;
    output_buffer[output_buffer_len-1] = '\n';

    input_cursor = 0;
    input_buffer_len = 0;
    memset(input_buffer, 0, INPUT_BUFFER_LEN);

    xSemaphoreGive(ui_refresh_sem);
}
