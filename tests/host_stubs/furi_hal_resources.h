/* Minimal host-side stub of Furi HAL resources. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void* port;
    uint32_t pin;
} GpioPin;

void furi_hal_gpio_write(const GpioPin* gpio, bool state);

extern const GpioPin gpio_nrf24_ce;
