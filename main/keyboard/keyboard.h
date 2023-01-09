#pragma once

#include <stdbool.h>

typedef enum {
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_MINUS,
    KEY_EQUALS,
    KEY_BACKSPACE,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_T,
    KEY_Y,
    KEY_U,
    KEY_I,
    KEY_O,
    KEY_P,
    KEY_BRACKET_LEFT,
    KEY_BRACKET_RIGHT,
    KEY_BACKSLASH,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_SEMICOLON,
    KEY_QUOTES,
    KEY_ENTER,
    KEY_LSHIFT,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_N,
    KEY_M,
    KEY_ARROW_COMMA,
    KEY_ARROW_DOT,
    KEY_SLASH,
    KEY_TILDA,
    KEY_RSHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_SPACE,
    KEY_ARROW_LEFT,
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_ARROW_RIGHT,
    KEY_COUNT
} kbrd_key_t;

typedef enum {
    KEY_RELEASED,
    KEY_PRESSED,
    KEY_TOGGLED,
    KEY_DOWN,
    KEY_UP,
    KEY_STATE_COUNT
} kbrd_key_state_t;

typedef void (*kbrd_callback_t)(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

void keyboard_task(void *arg);

void keyboard_register_callback(kbrd_key_t key, kbrd_key_state_t state, kbrd_callback_t clbk);
bool keyboard_is_key_pressed(kbrd_key_t key);
char keyboard_key_to_char(kbrd_key_t key, bool shifted);
