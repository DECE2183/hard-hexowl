#include "screen.h"

#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <keyboard.h>
#include <sdcard.h>

#include "../ssd1322/ssd1322.h"
#include "../ssd1322/ssd1322_font.h"
#include "../fonts/cascadia_font.h"

extern SemaphoreHandle_t ui_refresh_sem;
extern ssd1322_t *ui_display;

static bool done = false;
static float progress = 0;
static void key_press_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

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
    return true;
}

static void open(void)
{
    // clear screen and draw initial layout
    ssd1322_fill(ui_display, 0);

    done = false;
    progress = 0;

    keyboard_register_callback(KEY_ENTER, KEY_PRESSED, key_press_callback);
}

static void draw(void)
{

}

static void close(void)
{
    // unregister keyboard callback
    keyboard_register_callback(KEY_ENTER, KEY_PRESSED, NULL);
}

static void key_press_callback(kbrd_key_t k, kbrd_key_state_t s, bool pressed)
{
    if (!done)
    {
        return;
    }
    
    xSemaphoreGive(ui_refresh_sem);
}
