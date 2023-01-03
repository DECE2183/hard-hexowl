#pragma once

typedef struct {
    float vals[3];
} median_t;

median_t *median_init();
void median_deinit(median_t *f);

float median_filter(median_t *f, float value);
