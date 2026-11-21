/**
 * @file st7567_i2c.c
 * @brief ST7567S 128x64 LCD over I2C for the GM12864-59N module.
 *
 * Protocol (ST7567S datasheet):
 *   START, 0x7E (addr 0x3F << 1), control byte, N data bytes, STOP
 *   control byte 0x00 -> following bytes are commands
 *   control byte 0x40 -> following bytes are display RAM data
 *
 * The module has no reset pin, so 0xE2 (software reset) is mandatory and the
 * charge pump must be started in stages (0x2C, 0x2E, 0x2F) with settling time.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include "st7567_i2c.h"

#include <furi.h>
#include <furi_hal_i2c.h>

#include <string.h>

#define TAG "St7567"

/* ST7567 command set (subset) */
#define CMD_DISPLAY_ON        0xAF
#define CMD_DISPLAY_OFF       0xAE
#define CMD_SET_START_LINE    0x40
#define CMD_SET_PAGE          0xB0
#define CMD_SET_COL_MSB       0x10
#define CMD_SET_COL_LSB       0x00
#define CMD_SEG_NORMAL        0xA0
#define CMD_SEG_REVERSE       0xA1
#define CMD_COM_NORMAL        0xC0
#define CMD_COM_REVERSE       0xC8
#define CMD_INVERT_ON         0xA7
#define CMD_INVERT_OFF        0xA6
#define CMD_ALL_PIXELS_OFF    0xA4
#define CMD_BIAS_1_9          0xA2
#define CMD_BIAS_1_7          0xA3
#define CMD_REGULATION_RATIO  0x20
#define CMD_POWER_CONTROL     0x28
#define CMD_SET_EV            0x81
#define CMD_SOFT_RESET        0xE2
#define CMD_NOP               0xE3
#define CMD_BOOSTER           0xF8

#define I2C_TIMEOUT_MS 100

/* The largest single I2C payload we build on the stack: control byte + 128. */
#define TX_CHUNK 128

static const FuriHalI2cBusHandle* st7567_bus(void) {
    /* On this board the stock "power" I2C1 bus carries the display. */
    return &furi_hal_i2c_handle_power;
}

static bool st7567_tx(const uint8_t* data, size_t size) {
    bool ok = false;
    furi_hal_i2c_acquire(st7567_bus());
    ok = furi_hal_i2c_tx(st7567_bus(), ST7567_I2C_ADDRESS, data, size, I2C_TIMEOUT_MS);
    furi_hal_i2c_release(st7567_bus());
    return ok;
}

static bool st7567_cmd(uint8_t command) {
    const uint8_t frame[2] = {0x00, command};
    return st7567_tx(frame, sizeof(frame));
}

static bool st7567_cmd2(uint8_t command, uint8_t argument) {
    const uint8_t frame[3] = {0x00, command, argument};
    return st7567_tx(frame, sizeof(frame));
}

static bool st7567_data(const uint8_t* data, size_t size) {
    uint8_t frame[1 + TX_CHUNK];
    bool ok = true;

    frame[0] = 0x40; /* data control byte */
    while(size > 0 && ok) {
        size_t chunk = size > TX_CHUNK ? TX_CHUNK : size;
        memcpy(&frame[1], data, chunk);
        ok = st7567_tx(frame, chunk + 1);
        data += chunk;
        size -= chunk;
    }
    return ok;
}

bool st7567_i2c_probe(void) {
    /* Address + NOP command: the smallest transaction that proves both the
     * address and the control-byte path without changing display state. */
    return st7567_cmd(CMD_NOP);
}

void st7567_i2c_init(void) {
    /* Software reset, then let the internal state machine settle. */
    st7567_cmd(CMD_SOFT_RESET);
    furi_delay_ms(5);

    st7567_cmd(CMD_BIAS_1_9); /* 128x64 panels use 1/9 bias */
    st7567_cmd(CMD_SEG_NORMAL);
    st7567_cmd(CMD_COM_REVERSE);
    st7567_cmd(CMD_SET_START_LINE | 0x00);
    st7567_cmd(CMD_REGULATION_RATIO | (ST7567_DEFAULT_REG_RATIO & 0x07));

    st7567_cmd2(CMD_SET_EV, ST7567_DEFAULT_CONTRAST & 0x3F);

    /* Charge pump: booster -> regulator -> follower, with settling time. */
    st7567_cmd(CMD_POWER_CONTROL | 0x04); /* booster on */
    furi_delay_ms(2);
    st7567_cmd(CMD_POWER_CONTROL | 0x06); /* + voltage regulator */
    furi_delay_ms(2);
    st7567_cmd(CMD_POWER_CONTROL | 0x07); /* + voltage follower */
    furi_delay_ms(2);

    st7567_cmd(CMD_ALL_PIXELS_OFF);
    st7567_cmd(CMD_INVERT_OFF);
    st7567_cmd(CMD_DISPLAY_ON);

    FURI_LOG_I(TAG, "init done (addr 0x%02X)", ST7567_I2C_ADDRESS);
}

void st7567_i2c_set_contrast(uint8_t value) {
    st7567_cmd2(CMD_SET_EV, value & 0x3F);
}

void st7567_i2c_set_regulation_ratio(uint8_t value) {
    st7567_cmd(CMD_REGULATION_RATIO | (value & 0x07));
}

void st7567_i2c_set_display_enabled(bool enabled) {
    st7567_cmd(enabled ? CMD_DISPLAY_ON : CMD_DISPLAY_OFF);
}

void st7567_i2c_set_inverted(bool inverted) {
    st7567_cmd(inverted ? CMD_INVERT_ON : CMD_INVERT_OFF);
}

void st7567_i2c_set_flipped(bool flipped) {
    st7567_cmd(flipped ? CMD_SEG_REVERSE : CMD_SEG_NORMAL);
    st7567_cmd(flipped ? CMD_COM_NORMAL : CMD_COM_REVERSE);
}

void st7567_i2c_write_page(uint8_t page, uint8_t column, const uint8_t* data, size_t size) {
    if(page >= ST7567_PAGES || column >= ST7567_WIDTH || size == 0) return;
    if(column + size > ST7567_WIDTH) size = ST7567_WIDTH - column;

    st7567_cmd(CMD_SET_PAGE | (page & 0x0F));
    st7567_cmd(CMD_SET_COL_MSB | ((column >> 4) & 0x0F));
    st7567_cmd(CMD_SET_COL_LSB | (column & 0x0F));
    st7567_data(data, size);
}

void st7567_i2c_fill(uint8_t pattern) {
    uint8_t line[ST7567_WIDTH];
    memset(line, pattern, sizeof(line));
    for(uint8_t page = 0; page < ST7567_PAGES; page++) {
        st7567_i2c_write_page(page, 0, line, sizeof(line));
    }
}
