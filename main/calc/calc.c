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
SemaphoreHandle_t calc_done_sem;
SemaphoreHandle_t calc_lock_mux;

static char input_str[INPUT_LEN];
static char output_str[OUTPUT_LEN];

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
    struct CalculatePrompt_return vals;
    char *strend = output_str;

    vals = CalculatePrompt(input_str);

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
    calc_done_sem = xSemaphoreCreateBinary();
    calc_lock_mux = xSemaphoreCreateMutex();

    if (calc_begin_sem == NULL || calc_done_sem == NULL || calc_lock_mux == NULL)
    {
        ESP_LOGE("calc", "semaphore creation error");
        goto error;
    }

    uintptr_t go_heap_size = (uintptr_t)arg;
    gorun(go_heap_size);

    HexocalcInit();
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
    if (xSemaphoreTake(calc_lock_mux, LOCK_TIMEOUT))
    {
        strcpy(input_str, expr);
        xSemaphoreGive(calc_begin_sem);
    }
    else
    {
        ESP_LOGW("calc", "resource lock mutex timeout");
    }
}

const char *calc_await_expression(void)
{
    if (xSemaphoreTake(calc_done_sem, portMAX_DELAY))
    {
        xSemaphoreGive(calc_lock_mux);
        return output_str;
    }
    else
    {
        ESP_LOGE("calc", "expression calculation timeout");
        return NULL;
    }
}
