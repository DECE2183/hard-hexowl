#include "keyboard.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "pcf8575/pcf8575.h"

#define ROWS_CNT 8
#define CLMN_CNT 8

static pcf8575_t *gpio_expander;
static const uint8_t address = 0x20;
static const pcf8575_pinmap_t pinmap = {
    .extint = -1,
};

static const i2c_port_t i2c_port = I2C_NUM_0;
static const i2c_config_t i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = 33,
    .scl_io_num = 32,
    .sda_pullup_en = GPIO_PULLUP_DISABLE,
    .scl_pullup_en = GPIO_PULLUP_DISABLE,
    .master.clk_speed = 400000,
};

static uint8_t keys[58] = {0};

static void scan(void);

bool keyboard_is_key_pressed(kbrd_keys_t key)
{
    if (key >= sizeof(keys)) return false;
    return keys[key];
}

void keyboard_task(void *arg)
{
    esp_err_t err;

    err = i2c_param_config(i2c_port, &i2c_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE("kbrd", "I2C configuration error");
        goto error;
    }

    err = i2c_driver_install(i2c_port, i2c_cfg.mode, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("kbrd", "I2C driver install error");
        goto error;
    }

    gpio_expander = pcf8575_init(i2c_port, address, pinmap);
    if (gpio_expander == NULL)
    {
        ESP_LOGE("kbrd", "port expander initialization error");
        goto error;
    }

    while (1)
    {
        scan();
        vTaskDelay(1);
    }

error:
    while (1) vTaskDelay(1000);
}

static void scan(void)
{
    uint16_t clmn_selector = 0;
    uint16_t port_value = 0;
    uint16_t key;
    
    for (int clmn = 0; clmn < CLMN_CNT; ++clmn)
    {
        clmn_selector = 0x100 << clmn;
        pcf8575_write(gpio_expander, ~clmn_selector);

        port_value = pcf8575_read(gpio_expander);
        for (int row = 0; row < ROWS_CNT; ++row)
        {
            key = row * CLMN_CNT + clmn;
            if (key >= sizeof(keys)) break;

            keys[key] = !(port_value & 0x01);
            if (keys[key])
                ESP_LOGI("kbrd", "key %d pressed", key);

            port_value >>= 1;
        }

        clmn_selector <<= 1;
    }

    pcf8575_write(gpio_expander, 0xFFFF);
}

