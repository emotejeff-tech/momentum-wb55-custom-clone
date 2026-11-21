#pragma once

/**
 * @file furi_hal_spi_config.h
 * @brief SPI bus layout for the "f55" target.
 *
 * SPI1: CC1101 + external header.
 * SPI2: microSD + nRF24L01+ (the display moved to I2C, freeing this bus).
 *
 * There is intentionally no NFC and no display SPI handle on this board.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include <furi_hal_spi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Presets (same values as stock) */
extern const LL_SPI_InitTypeDef furi_hal_spi_preset_2edge_low_8m;
extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_8m;
extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_4m;
extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_16m;
extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_2m;

/* Buses */
extern FuriHalSpiBus furi_hal_spi_bus_r; /* SPI1 */
extern FuriHalSpiBus furi_hal_spi_bus_d; /* SPI2 */

/* Handles */
extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_subghz; /* SPI1, CS PD0  */
extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_external; /* SPI1 header */
extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_nrf24; /* SPI2, CSN PB0 */
extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_sd_fast; /* SPI2, CS PC12 */
extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_sd_slow;

void furi_hal_spi_config_init_early(void);
void furi_hal_spi_config_deinit_early(void);
void furi_hal_spi_config_init(void);

#ifdef __cplusplus
}
#endif
