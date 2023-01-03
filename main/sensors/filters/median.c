#include "median.h"

#include <stdlib.h>
#include <string.h>

median_t *median_init()
{
    median_t *f = malloc(sizeof(median_t));
    if (f == NULL) return NULL;

    memset(f, 0, sizeof(median_t));
    return f;
}

void median_deinit(median_t *f)
{
    free(f);
}

float median_filter(median_t *f, float value)
{
    f->vals[2] = f->vals[1];
    f->vals[1] = f->vals[0];
    f->vals[0] = value;

    float middle;
    float a = f->vals[0];
    float b = f->vals[1];
    float c = f->vals[2];

    if ((a <= b) && (a <= c))
    {
        middle = (b <= c) ? b : c;
    }
    else
    {
        if ((b <= a) && (b <= c))
        {
            middle = (a <= c) ? a : c;
        }
        else
        {
            middle = (a <= b) ? a : b;
        }
    }
    return middle;
}