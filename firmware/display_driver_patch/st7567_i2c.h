/**
 * @file st7567_i2c.h
 * @brief Minimal ST7567S 128x64 COG LCD driver over I2C (GM12864-59N).
 *
 * Target: Estardyn/Tenstar GM12864-59N, 4-pin I2C module, controller ST7567S.
 * Bus:    Furi HAL I2C handle "power" (I2C1, PA9=SCL / PA10=SDA on this board).
 * Slave:  7-bit address 0x3F (write byte 0x7E).
 *
 * The controller has no reset/DC/CS pins on this module: reset is the software
 * command 0xE2, and command/data selection is the I2C control byte
 * (0x00 = command, 0x40 = data).
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ST7567_I2C_ADDRESS 0x3F
#define ST7567_WIDTH       128
#define ST7567_HEIGHT      64
#define ST7567_PAGES       (ST7567_HEIGHT / 8)

/** Default electronic volume (contrast) value, 0x00..0x3F. */
#define ST7567_DEFAULT_CONTRAST 0x28

/** Default regulation ratio, 0x20..0x27. */
#define ST7567_DEFAULT_REG_RATIO 0x25

/**
 * @brief Probe the panel (address-only write). Useful during bring-up.
 * @return true if the controller ACKed.
 */
bool st7567_i2c_probe(void);

/**
 * @brief Full initialisation sequence, including software reset.
 *
 * Safe to call again after a power cycle or a wedged controller. Takes about
 * 10 ms due to the charge-pump settling delays.
 */
void st7567_i2c_init(void);

/** @brief Set electronic volume (contrast), 0x00..0x3F. */
void st7567_i2c_set_contrast(uint8_t value);

/** @brief Set regulation ratio, 0x20..0x27 (panel batch dependent). */
void st7567_i2c_set_regulation_ratio(uint8_t value);

/** @brief Display on/off (0xAF / 0xAE). */
void st7567_i2c_set_display_enabled(bool enabled);

/** @brief Invert all pixels (0xA7 / 0xA6). */
void st7567_i2c_set_inverted(bool inverted);

/** @brief Flip the image 180 degrees using segment/COM remap. */
void st7567_i2c_set_flipped(bool flipped);

/**
 * @brief Write a horizontal run of bytes into display RAM.
 *
 * @param page  Page (0..7), i.e. y / 8.
 * @param column Start column (0..127).
 * @param data  Bytes in ST7567 order: bit0 = top row of the page.
 * @param size  Number of bytes (caller guarantees column + size <= 128).
 */
void st7567_i2c_write_page(uint8_t page, uint8_t column, const uint8_t* data, size_t size);

/**
 * @brief Fill the whole panel with a byte pattern (bring-up test).
 */
void st7567_i2c_fill(uint8_t pattern);

#ifdef __cplusplus
}
#endif
