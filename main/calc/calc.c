#include "calc.h"

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <hexowl.h>

#define INPUT_LEN (1024)
#define OUTPUT_LEN (4096)
#define LOCK_TIMEOUT (2500)

extern void gorun(uintptr_t);

SemaphoreHandle_t calc_begin_sem;
SemaphoreHandle_t calc_in_lock_mux;

SemaphoreHandle_t calc_out_sem;
SemaphoreHandle_t calc_out_lock_mux;

SemaphoreHandle_t calc_done_sem;

static char input_str[INPUT_LEN+1] = {0};
static char output_str[OUTPUT_LEN+1] = {0};

static void hx_print_func(GoString str)
{
    ESP_LOGI("calc", "output triggered");

    if (xSemaphoreTake(calc_out_lock_mux, LOCK_TIMEOUT))
    {
        memset(input_str, 0, INPUT_LEN);
        memcpy(input_str, str.p, str.n);
        xSemaphoreGive(calc_out_sem);
    }
    else
    {
        ESP_LOGW("calc", "output resource lock mutex timeout");
    }
}

static char *str_chain_append(char *dest, const char *source, int len)
{
    if (len == 0)
        len = strlen(source);

    memcpy(dest, source, len);
    dest += len;
    *dest = 0;
    return dest;
}

static void calc_begin(void)
{
    hexowl_calculate_return_t vals;
    char *strend = output_str;

    vals = HexowlCalculate(input_str);

    if (vals.success == 0)
    {
        strend = str_chain_append(strend, "<: error occured: ", 0);
        strend = str_chain_append(strend, vals.decVal.p, vals.decVal.n);
    }
    else
    {
        if (vals.decVal.n > 0)
        {
            strend = str_chain_append(strend, "<: ", 0);
            strend = str_chain_append(strend, vals.decVal.p, vals.decVal.n);
        }
        if (vals.hexVal.n > 0)
        {
            strend = str_chain_append(strend, "\n   ", 0);
            strend = str_chain_append(strend, vals.hexVal.p, vals.hexVal.n);
        }
        if (vals.binVal.n > 0)
        {
            strend = str_chain_append(strend, "\n   ", 0);
            strend = str_chain_append(strend, vals.binVal.p, vals.binVal.n);
        }

        // strend = str_chain_append(strend, "\r\n\r\n\tTime:\t", 0);
        // itoa(vals.calcTime, strend, 10);
        // strend += strlen(strend);
        // strend = str_chain_append(strend, " ms\r\n\r\n", 0);
    }
    
    strend = str_chain_append(strend, "\n", 0);
}

void calc_task(void *arg)
{
    calc_begin_sem = xSemaphoreCreateBinary();
    calc_in_lock_mux = xSemaphoreCreateMutex();
    calc_out_sem = xSemaphoreCreateBinary();
    calc_out_lock_mux = xSemaphoreCreateMutex();
    calc_done_sem = xSemaphoreCreateBinary();

    if (calc_begin_sem == NULL || calc_in_lock_mux == NULL || calc_out_sem == NULL || calc_out_lock_mux == NULL || calc_done_sem == NULL)
    {
        ESP_LOGE("calc", "semaphore creation error");
        goto error;
    }

    uintptr_t go_heap_size = (uintptr_t)arg;
    gorun(go_heap_size);

    HexowlInit(hx_print_func, OUTPUT_LEN);
    ESP_LOGI("calc", "hexowl task initialized");

    while (1)
    {
        if (xSemaphoreTake(calc_begin_sem, portMAX_DELAY))
        {
            // calculate expression
            calc_begin();
            // inform about complete
            xSemaphoreGive(calc_done_sem);
        }
    }

error:
    while (1) vTaskDelay(1000);
}

void calc_expression(const char *expr)
{
    if (xSemaphoreTake(calc_in_lock_mux, LOCK_TIMEOUT))
    {
        strcpy(input_str, expr);
        xSemaphoreGive(calc_begin_sem);
    }
    else
    {
        ESP_LOGW("calc", "input resource lock mutex timeout");
    }
}

const char *calc_await_expression(void)
{
    if (xSemaphoreTake(calc_done_sem, portMAX_DELAY))
    {
        xSemaphoreGive(calc_in_lock_mux);
        return output_str;
    }
    else
    {
        ESP_LOGE("calc", "expression calculation timeout");
        return NULL;
    }
}

const char *calc_await_output(int timeout)
{
    if (xSemaphoreTake(calc_out_sem, timeout))
    {
        xSemaphoreGive(calc_out_lock_mux);
        return output_str;
    }
    else
    {
        return NULL;
    }
}
