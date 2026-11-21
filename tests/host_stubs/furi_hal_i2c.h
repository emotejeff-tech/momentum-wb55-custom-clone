/* Minimal host-side stub of the Furi HAL I2C API. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct FuriHalI2cBusHandle {
    int unused;
} FuriHalI2cBusHandle;

void furi_hal_i2c_acquire(const FuriHalI2cBusHandle* handle);
void furi_hal_i2c_release(const FuriHalI2cBusHandle* handle);
bool furi_hal_i2c_tx(
    const FuriHalI2cBusHandle* handle,
    uint8_t address,
    const uint8_t* data,
    size_t size,
    uint32_t timeout);

extern const FuriHalI2cBusHandle furi_hal_i2c_handle_power;
