#include "keyboard.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "pcf8575/pcf8575.h"

#define ROWS_CNT 8
#define CLMN_CNT 8
#define KEYBOARD_SCAN_RATE 1

typedef kbrd_callback_t kbrd_key_clbk_t[KEY_STATE_COUNT];

static pcf8575_t *gpio_expander;
static const uint8_t address = 0x20;
static const pcf8575_pinmap_t pinmap = {
    .extint = -1,
};

static const i2c_port_t i2c_port = I2C_NUM_0;
static const i2c_config_t i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = 33,
    .scl_io_num = 32,
    .sda_pullup_en = GPIO_PULLUP_DISABLE,
    .scl_pullup_en = GPIO_PULLUP_DISABLE,
    .master.clk_speed = 400000,
};

static const char key_to_char_table[KEY_COUNT][2] = {
    [KEY_1] = {'1', '!'},
    [KEY_2] = {'2', '@'},
    [KEY_3] = {'3', '#'},
    [KEY_4] = {'4', '$'},
    [KEY_5] = {'5', '%'},
    [KEY_6] = {'6', '^'},
    [KEY_7] = {'7', '&'},
    [KEY_8] = {'8', '*'},
    [KEY_9] = {'9', '('},
    [KEY_0] = {'0', ')'},
    [KEY_MINUS] = {'-', '_'},
    [KEY_EQUALS] = {'=', '+'},
    [KEY_BACKSPACE] = {8, 8},
    [KEY_Q] = {'q', 'Q'},
    [KEY_W] = {'w', 'W'},
    [KEY_E] = {'e', 'E'},
    [KEY_R] = {'r', 'R'},
    [KEY_T] = {'t', 'T'},
    [KEY_Y] = {'y', 'Y'},
    [KEY_U] = {'u', 'U'},
    [KEY_I] = {'i', 'I'},
    [KEY_O] = {'o', 'O'},
    [KEY_P] = {'p', 'P'},
    [KEY_BRACKET_LEFT] = {'[', '{'},
    [KEY_BRACKET_RIGHT] = {']', '}'},
    [KEY_BACKSLASH] = {'\\', '|'},
    [KEY_A] = {'a', 'A'},
    [KEY_S] = {'s', 'S'},
    [KEY_D] = {'d', 'D'},
    [KEY_F] = {'f', 'F'},
    [KEY_G] = {'g', 'G'},
    [KEY_H] = {'h', 'H'},
    [KEY_J] = {'j', 'J'},
    [KEY_K] = {'k', 'K'},
    [KEY_L] = {'l', 'L'},
    [KEY_SEMICOLON] = {';', ':'},
    [KEY_QUOTES] = {'\'', '\"'},
    [KEY_ENTER] = {'\n', '\n'},
    [KEY_LSHIFT] = {15, 15},
    [KEY_Z] = {'z', 'Z'},
    [KEY_X] = {'x', 'X'},
    [KEY_C] = {'c', 'C'},
    [KEY_V] = {'v', 'V'},
    [KEY_B] = {'b', 'B'},
    [KEY_N] = {'n', 'N'},
    [KEY_M] = {'m', 'M'},
    [KEY_ARROW_COMMA] = {',', '<'},
    [KEY_ARROW_DOT] = {'.', '>'},
    [KEY_SLASH] = {'/', '?'},
    [KEY_TILDA] = {'`', '~'},
    [KEY_RSHIFT] = {15, 15},
    [KEY_CTRL] = {'\0', '\0'},
    [KEY_ALT] = {'\0', '\0'},
    [KEY_SPACE] = {' ', ' '},
    [KEY_ARROW_LEFT] = {'\0', '\0'},
    [KEY_ARROW_UP] = {'\0', '\0'},
    [KEY_ARROW_DOWN] = {'\0', '\0'},
    [KEY_ARROW_RIGHT] = {'\0', '\0'}
};

static uint8_t keys[KEY_COUNT] = {0};
static kbrd_key_clbk_t callbacks[KEY_COUNT] = {0};

static void scan(void);

inline void keyboard_register_callback(kbrd_key_t key, kbrd_key_state_t state, kbrd_callback_t clbk)
{
    callbacks[key][state] = clbk;
}

inline bool keyboard_is_key_pressed(kbrd_key_t key)
{
    if (key >= sizeof(keys)) return false;
    return keys[key];
}

inline char keyboard_key_to_char(kbrd_key_t key, bool shifted)
{
    return key_to_char_table[key][shifted];
}

void keyboard_task(void *arg)
{
    esp_err_t err;

    err = i2c_param_config(i2c_port, &i2c_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE("kbrd", "I2C configuration error");
        goto error;
    }

    err = i2c_driver_install(i2c_port, i2c_cfg.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("kbrd", "I2C driver install error");
        goto error;
    }

    gpio_expander = pcf8575_init(i2c_port, address, pinmap);
    if (gpio_expander == NULL)
    {
        ESP_LOGE("kbrd", "port expander initialization error");
        goto error;
    }

    while (1)
    {
        scan();
        vTaskDelay(KEYBOARD_SCAN_RATE);
    }

error:
    while (1) vTaskDelay(1000);
}

static void scan(void)
{
    uint16_t clmn_selector = 0;
    uint16_t port_value = 0;

    uint8_t key;
    uint8_t key_val;

    for (int clmn = 0; clmn < CLMN_CNT; ++clmn)
    {
        clmn_selector = 0x100 << clmn;
        pcf8575_write(gpio_expander, ~clmn_selector);

        port_value = pcf8575_read(gpio_expander);
        for (int row = 0; row < ROWS_CNT; ++row)
        {
            key = row * CLMN_CNT + (clmn < 6 ? clmn : 6 + (7 - clmn));
            if (key >= sizeof(keys)) break;

            key_val = !(port_value & 0x01);

            // execute callbacks
            if (key_val != keys[key])
            {
                if (key_val && callbacks[key][KEY_PRESSED] != NULL)
                    callbacks[key][KEY_PRESSED](key, KEY_PRESSED, key_val);
                else if (!key_val && callbacks[key][KEY_RELEASED] != NULL)
                    callbacks[key][KEY_RELEASED](key, KEY_RELEASED, key_val);
                    
                if (callbacks[key][KEY_TOGGLED] != NULL)
                    callbacks[key][KEY_TOGGLED](key, KEY_TOGGLED, key_val);
            }

            keys[key] = key_val;
            port_value >>= 1;
        }

        clmn_selector <<= 1;
    }

    pcf8575_write(gpio_expander, 0xFFFF);
}

