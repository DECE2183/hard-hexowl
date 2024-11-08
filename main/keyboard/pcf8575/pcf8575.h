#pragma once

#include <stdint.h>

#include <driver/i2c.h>
#include <driver/gpio.h>

typedef struct
{
    gpio_num_t extint;
} pcf8575_pinmap_t;

typedef struct pcf8575
{
    i2c_port_t i2c;
    uint8_t address;

    pcf8575_pinmap_t pinmap;
    void (*extint_handler)(struct pcf8575 *device);

    uint16_t last_read_value;
    uint16_t last_write_value;
} pcf8575_t;

pcf8575_t *pcf8575_init(i2c_port_t i2c, uint8_t address, pcf8575_pinmap_t pinmap);
void pcf8575_deinit(pcf8575_t *device);

uint16_t pcf8575_read(pcf8575_t *device);
uint8_t pcf8575_read_port(pcf8575_t *device, uint8_t port);
uint8_t pcf8575_read_pin(pcf8575_t *device, uint8_t pin);

void pcf8575_write(pcf8575_t *device, uint16_t value);
void pcf8575_write_port(pcf8575_t *device, uint8_t port, uint8_t value);
void pcf8575_write_pin(pcf8575_t *device, uint8_t pin, uint8_t value);

void pcf8575_enable_extint(pcf8575_t *device);
void pcf8575_disable_extint(pcf8575_t *device);
