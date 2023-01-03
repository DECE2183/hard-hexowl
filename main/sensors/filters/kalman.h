#pragma once

typedef struct {
    float measure_error;
    float measure_speed;
    float estimate_error;
    float last_estimate;
} kalman_t;

kalman_t *kalman_init(float error, float speed);
void kalman_deinit(kalman_t *f);

float kalman_filter(kalman_t *f, float value);