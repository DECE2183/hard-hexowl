#include "ui.h"

#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <keyboard.h>

#include "screens/screen.h"
#include "ssd1322/ssd1322.h"
#include "ssd1322/ssd1322_bitmap.h"
#include "bitmaps/hexowl_logo_full_bmp.h"

static const int res_x = 256, res_y = 64;
static const ssd1322_pinmap_t pinmap = {
    .reset = 22,
    .dc = 21,
    .cs = 5,
};

static spi_host_device_t spi_host = SPI3_HOST;
static const spi_bus_config_t spi_bus_cfg = {
    .miso_io_num = -1,
    .mosi_io_num = 23,
    .sclk_io_num = 18,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 8192,
    .flags = SPICOMMON_BUSFLAG_MASTER,
};

ssd1322_t *ui_display;
SemaphoreHandle_t ui_refresh_sem;

static const ui_screen_t *current_screen;

void ui_task(void *arg)
{
    esp_err_t err;

    err = spi_bus_initialize(spi_host, &spi_bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        ESP_LOGE("disp", "SPI bus initialization error");
        goto error;
    }

    ui_display = ssd1322_init(spi_host, pinmap, res_x, res_y);
    if (ui_display == NULL)
    {
        ESP_LOGE("disp", "display initialization error");
        goto error;
    }

    ui_refresh_sem = xSemaphoreCreateBinary();
    if (ui_refresh_sem == NULL)
    {
        ESP_LOGE("disp", "semaphore creation error");
        goto error;
    }

    // intialize screens
    for (int i = 0; i < ui_screens_count; ++i)
    {
        if (!ui_screens[i]->init())
        {
            ESP_LOGE("disp", "screen %d initialization error", i);
            goto error;
        }
    }

    ssd1322_draw_bitmap(ui_display, 12, 7, hexowl_logo_full);
    ssd1322_send_framebuffer(ui_display);

    // open first or test screen
    vTaskDelay(700);
    if (keyboard_is_key_pressed(KEY_TILDA))
    {
        current_screen = ui_screens[SCREEN_TEST];
    }
    else
    {
        current_screen = ui_screens[0];
    }
    current_screen->open();
    ssd1322_send_framebuffer(ui_display);

    while (1)
    {
        if (xSemaphoreTake(ui_refresh_sem, 250))
        {
            current_screen->draw();
            ssd1322_send_framebuffer(ui_display);
        }
    }

error:
    while (1) vTaskDelay(1000);
}

void ui_change_screen(ui_screen_num_t screen)
{
    current_screen->close();
    current_screen = ui_screens[screen];
    current_screen->open();
}
