#include "pcf8575.h"

static void int_handler(void *arg)
{
    pcf8575_t *device = (pcf8575_t *)arg;
    if (device == NULL || device->extint_handler == NULL)
    {
        return;
    }
    device->extint_handler(device);
}

pcf8575_t *pcf8575_init(i2c_port_t i2c, uint8_t address, pcf8575_pinmap_t pinmap)
{
    pcf8575_t *device = malloc(sizeof(pcf8575_t));
    if (device == NULL)
    {
        return NULL;
    }

    device->i2c = i2c;
    device->address = address;
    device->pinmap = pinmap;
    device->last_read_value = 0;
    device->last_write_value = 0;

    if (device->pinmap.extint > -1)
    {
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        gpio_set_direction(device->pinmap.extint, GPIO_MODE_INPUT);
        gpio_set_pull_mode(device->pinmap.extint, GPIO_PULLUP_ONLY);
        gpio_set_intr_type(device->pinmap.extint, GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(device->pinmap.extint, int_handler, device);
    }

    return device;
}

void pcf8575_deinit(pcf8575_t *device)
{
    if (device->pinmap.extint > -1)
    {
        gpio_set_pull_mode(device->pinmap.extint, GPIO_FLOATING);
    }
    free(device);
}

uint16_t pcf8575_read(pcf8575_t *device)
{
    i2c_master_read_from_device(device->i2c, device->address | 0x80, (uint8_t *)&device->last_read_value, 2, 100);
    return device->last_read_value;
}

uint8_t pcf8575_read_port(pcf8575_t *device, uint8_t port)
{
    uint8_t val = port == 0 ? pcf8575_read(device) & 0xFF : pcf8575_read(device) >> 8;
    return val;
}

uint8_t pcf8575_read_pin(pcf8575_t *device, uint8_t pin)
{
    return (pcf8575_read(device) >> pin) & 0x01;
}

void pcf8575_write(pcf8575_t *device, uint16_t value)
{
    device->last_write_value = value;
    i2c_master_write_to_device(device->i2c, device->address, (uint8_t *)&device->last_write_value, 2, 200);
}

void pcf8575_write_port(pcf8575_t *device, uint8_t port, uint8_t value)
{
    uint16_t val = device->last_write_value & (0xFF << (8 * port));
    val |= ((uint16_t)value) << (8 * port);
    pcf8575_write(device, val);
}

void pcf8575_write_pin(pcf8575_t *device, uint8_t pin, uint8_t value)
{
    uint16_t val = device->last_write_value & (1 << pin);
    val |= (value & 0x01) << pin;
    pcf8575_write(device, val);
}

void pcf8575_enable_extint(pcf8575_t *device)
{
    if (device->pinmap.extint <= -1)
    {
        return;
    }

    gpio_intr_enable(device->pinmap.extint);
}

void pcf8575_disable_extint(pcf8575_t *device)
{
    if (device->pinmap.extint <= -1)
    {
        return;
    }

    gpio_intr_disable(device->pinmap.extint);
}
