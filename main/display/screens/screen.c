#include "screen.h"

extern const ui_screen_t test_screen;

const ui_screen_t *const ui_screens[] = {
    [SCREEN_TEST] = &test_screen,
};

const int ui_screens_count = sizeof(ui_screens) / sizeof(typeof(ui_screens[0]));
