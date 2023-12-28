#pragma once

#include <stdbool.h>

typedef struct {
    bool (*init)(void);
    void (*open)(void);
    void (*draw)(void);
    void (*close)(void);
} ui_screen_t;

typedef enum {
    SCREEN_CALCULATION,
    SCREEN_UPDATE,
    SCREEN_TEST,
} ui_screen_num_t;

extern const ui_screen_t *const ui_screens[];
extern const int ui_screens_count;
