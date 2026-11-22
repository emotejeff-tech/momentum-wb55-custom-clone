/**
 * @file st7567_i2c.h
 * @brief ST7567S 128x64 COG LCD driver over I2C (GM12864-59N).
 *
 * Controller: Sitronix ST7567S (also sold as ST7567A/G on similar panels).
 * Module:     Estardyn/Tenstar GM12864-59N, 4-pin I2C, 128x64, metal frame.
 *
 * Address:    the ST7567S exposes four I2C slave addresses, 0x3C..0x3F,
 *             selected by the module's SA0/SA1 straps. Most 59N modules are
 *             wired to 0x3F, but batches exist at 0x3E (and occasionally
 *             0x3C/0x3D). This driver auto-detects across 0x3C..0x3F.
 *
 * Protocol:   7-bit address + control byte (0x00 = command, 0x40 = data).
 *             No reset/DC/CS pins: reset is the software command 0xE2.
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

/* ST7567S I2C address range (SA0/SA1 strapping). */
#define ST7567_I2C_ADDR_MIN 0x3C
#define ST7567_I2C_ADDR_MAX 0x3F

/* Factory default on most GM12864-59N modules. */
#define ST7567_I2C_ADDRESS 0x3F

#define ST7567_WIDTH       128
#define ST7567_HEIGHT      64
#define ST7567_PAGES       (ST7567_HEIGHT / 8)

/** Default electronic volume (contrast) value, 0x00..0x3F. */
#define ST7567_DEFAULT_CONTRAST 0x28

/** Default regulation ratio, 0x20..0x27. */
#define ST7567_DEFAULT_REG_RATIO 0x25

/**
 * @brief Panel configuration. Zero-initialise then call
 *        st7567_i2c_default_config() for the 59N defaults.
 */
typedef struct {
    uint8_t address; /**< 0 = auto-detect 0x3C..0x3F, else fixed address */
    uint8_t contrast; /**< electronic volume 0x00..0x3F */
    uint8_t regulation_ratio; /**< 0x20..0x27 (low 3 bits used) */
    bool bias_1_7; /**< false = 1/9 bias (128x64), true = 1/7 */
    bool booster_5x; /**< false = 4x booster (3.3 V), true = 5x */
    bool flipped; /**< 180-degree image using SEG/COM remap */
} St7567Config;

/** @brief Fill a config with GM12864-59N defaults. */
void st7567_i2c_default_config(St7567Config* config);

/** @brief True if the controller ACKs at this 7-bit address. */
bool st7567_i2c_probe_addr(uint8_t address);

/**
 * @brief Probe 0x3C..0x3F and return the first address that ACKs, or 0.
 *        Logs every responding address.
 */
uint8_t st7567_i2c_scan(void);

/** @brief Address currently used by this driver (0 if not yet detected). */
uint8_t st7567_i2c_get_address(void);

/** @brief Force an address, or pass 0 to re-enable auto-detection. */
void st7567_i2c_set_address(uint8_t address);

/**
 * @brief Probe and initialise with 59N defaults.
 * @return true if a controller was found and the init sequence was sent.
 */
bool st7567_i2c_init(void);

/**
 * @brief Probe and initialise with an explicit configuration.
 * @return false if the configured/detected address does not ACK.
 */
bool st7567_i2c_init_ex(const St7567Config* config);

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

/** @brief Fill the whole panel with a byte pattern (bring-up test). */
void st7567_i2c_fill(uint8_t pattern);

#ifdef __cplusplus
}
#endif
