#pragma once

void calc_task(void *arg);

void calc_expression(const char *expr);
const char *calc_await_expression(void);
const char *calc_await_output(int timeout);
