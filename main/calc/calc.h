#pragma once

void calc_task(void *arg);

void calc_expression(const char *expr);
const char *calc_await_expression(void);

const char *calc_await_output(int timeout);
void calc_done_output(void);

typedef struct {
    char *firmware_version;
    unsigned int heap_size;
} calc_args_t;
