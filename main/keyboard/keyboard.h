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
    KEY_LSHIFT = 39,
    KEY_COUNT = 58
} kbrd_key_t;

typedef enum {
    KEY_RELEASED,
    KEY_PRESSED,
    KEY_TOGGLED,
    KEY_STATE_COUNT
} kbrd_key_state_t;

typedef void (*kbrd_callback_t)(kbrd_key_t k, kbrd_key_state_t s, bool pressed);

void keyboard_task(void *arg);

void keyboard_register_callback(kbrd_key_t key, kbrd_key_state_t state, kbrd_callback_t clbk);
bool keyboard_is_key_pressed(kbrd_key_t key);
