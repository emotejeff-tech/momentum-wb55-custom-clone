/**
 * @file st7567_i2c.c
 * @brief ST7567S 128x64 LCD over I2C for the GM12864-59N module.
 *
 * Protocol (ST7567S datasheet):
 *   START, addr (0x3C..0x3F << 1), control byte, N bytes, STOP
 *   control byte 0x00 -> commands, 0x40 -> display RAM data
 *
 * The module has no reset pin, so 0xE2 (software reset) is mandatory and the
 * charge pump is started in stages (0x2C, 0x2E, 0x2F) with settling time.
 *
 * Address handling: SA0/SA1 on the ST7567S select 0x3C..0x3F. init() probes
 * the range so the same firmware works with 0x3F and 0x3E modules.
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

/* Largest single I2C payload built on the stack: control byte + 128. */
#define TX_CHUNK 128

static uint8_t s_address = 0; /* 0 = not detected yet */
static bool s_address_locked = false;

static const FuriHalI2cBusHandle* st7567_bus(void) {
    /* On this board the stock "power" I2C1 bus carries the display. */
    return &furi_hal_i2c_handle_power;
}

static bool st7567_tx_to(uint8_t address, const uint8_t* data, size_t size) {
    bool ok;
    furi_hal_i2c_acquire(st7567_bus());
    ok = furi_hal_i2c_tx(st7567_bus(), address, data, size, I2C_TIMEOUT_MS);
    furi_hal_i2c_release(st7567_bus());
    return ok;
}

static bool st7567_tx(const uint8_t* data, size_t size) {
    if(s_address == 0) return false;
    return st7567_tx_to(s_address, data, size);
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

void st7567_i2c_default_config(St7567Config* config) {
    if(config == NULL) return;
    config->address = 0; /* auto-detect */
    config->contrast = ST7567_DEFAULT_CONTRAST;
    config->regulation_ratio = ST7567_DEFAULT_REG_RATIO;
    config->bias_1_7 = false; /* 1/9 for 128x64 */
    config->booster_5x = false; /* 4x is enough at 3.3 V */
    config->flipped = false;
}

bool st7567_i2c_probe_addr(uint8_t address) {
    const uint8_t frame[2] = {0x00, CMD_NOP};
    return st7567_tx_to(address, frame, sizeof(frame));
}

uint8_t st7567_i2c_scan(void) {
    uint8_t found = 0;
    uint8_t found_count = 0;

    for(uint8_t address = ST7567_I2C_ADDR_MIN; address <= ST7567_I2C_ADDR_MAX; address++) {
        if(st7567_i2c_probe_addr(address)) {
            FURI_LOG_I(TAG, "I2C device ACK at 0x%02X", address);
            found_count++;
            if(found == 0) found = address;
        }
    }

    if(found_count > 1) {
        FURI_LOG_W(TAG, "%u devices in 0x3C..0x3F; using 0x%02X", found_count, found);
    } else if(found_count == 0) {
        FURI_LOG_E(TAG, "no ST7567S at 0x3C..0x3F (wiring/pull-ups/address?)");
    }
    return found;
}

uint8_t st7567_i2c_get_address(void) {
    return s_address;
}

void st7567_i2c_set_address(uint8_t address) {
    s_address = address;
    s_address_locked = (address != 0);
}

static bool st7567_resolve_address(uint8_t requested) {
    if(requested != 0) {
        if(!st7567_i2c_probe_addr(requested)) {
            FURI_LOG_E(TAG, "no ACK at configured 0x%02X", requested);
            return false;
        }
        s_address = requested;
        s_address_locked = true;
        return true;
    }

    if(s_address_locked && s_address != 0) {
        return st7567_i2c_probe_addr(s_address);
    }

    uint8_t found = st7567_i2c_scan();
    if(found == 0) return false;
    s_address = found;
    return true;
}

bool st7567_i2c_init_ex(const St7567Config* config) {
    St7567Config defaults;
    if(config == NULL) {
        st7567_i2c_default_config(&defaults);
        config = &defaults;
    }

    if(!st7567_resolve_address(config->address)) return false;

    /* Software reset, then let the internal state machine settle. */
    st7567_cmd(CMD_SOFT_RESET);
    furi_delay_ms(5);

    st7567_cmd(config->bias_1_7 ? CMD_BIAS_1_7 : CMD_BIAS_1_9);
    st7567_cmd(config->flipped ? CMD_SEG_REVERSE : CMD_SEG_NORMAL);
    st7567_cmd(config->flipped ? CMD_COM_NORMAL : CMD_COM_REVERSE);
    st7567_cmd(CMD_SET_START_LINE | 0x00);
    st7567_cmd(CMD_REGULATION_RATIO | (config->regulation_ratio & 0x07));
    st7567_cmd2(CMD_SET_EV, config->contrast & 0x3F);

    /* Booster level must be set before the power-control stages. */
    st7567_cmd2(CMD_BOOSTER, config->booster_5x ? 0x01 : 0x00);

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

    FURI_LOG_I(
        TAG,
        "init OK: addr=0x%02X contrast=0x%02X ratio=0x%02X%s%s",
        s_address,
        config->contrast & 0x3F,
        config->regulation_ratio & 0x07,
        config->flipped ? " flipped" : "",
        config->booster_5x ? " booster5x" : "");

    return true;
}

bool st7567_i2c_init(void) {
    St7567Config config;
    st7567_i2c_default_config(&config);
    return st7567_i2c_init_ex(&config);
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
