#pragma once

typedef enum {
    SENS_VBAT,
    SENS_CHRG,
    SENS_COUNT
} sens_t;

typedef void (*sens_callback_t)(sens_t sensor, float value);

void sensors_task(void *arg);

void sensors_register_callback(sens_t sensor, sens_callback_t clbk);
float sensors_get_value(sens_t sensor);
