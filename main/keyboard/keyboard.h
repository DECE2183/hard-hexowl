#pragma once

#include <stdbool.h>

typedef enum {
    KEY_LSHIFT = 39,
} kbrd_keys_t;

void keyboard_task(void *arg);

bool keyboard_is_key_pressed(kbrd_keys_t key);
