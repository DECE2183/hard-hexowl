#include "kalman.h"

#include <stdlib.h>
#include <math.h>

kalman_t *kalman_init(float error, float speed)
{
    kalman_t *f = malloc(sizeof(kalman_t));
    if (f == NULL) return NULL;

    f->last_estimate = 0.0f;
    f->estimate_error = error;
    f->measure_error = error;
    f->measure_speed = speed;

    return f;
}

void kalman_deinit(kalman_t *f)
{
    free(f);
}

float kalman_filter(kalman_t *f, float value)
{
    float gain, current_estimate;

    gain = f->estimate_error / (f->estimate_error + f->measure_error);
    current_estimate = f->last_estimate + gain * (value - f->last_estimate);
    f->estimate_error = (1.0 - gain) * f->estimate_error + fabsf(f->last_estimate - current_estimate) * f->measure_speed;
    f->last_estimate = current_estimate;

    return current_estimate;
}
