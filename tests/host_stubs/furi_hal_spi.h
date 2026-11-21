/* Minimal host-side stub of the Furi HAL SPI API. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct FuriHalSpiBusHandle FuriHalSpiBusHandle;

void furi_hal_spi_acquire(const FuriHalSpiBusHandle* handle);
void furi_hal_spi_release(const FuriHalSpiBusHandle* handle);
bool furi_hal_spi_bus_trx(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_buffer,
    uint8_t* rx_buffer,
    size_t size,
    uint32_t timeout);

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_nrf24;
