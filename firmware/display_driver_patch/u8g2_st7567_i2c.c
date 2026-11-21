/**
 * @file u8g2_st7567_i2c.c
 * @brief u8g2 display callback for the GM12864-59N (ST7567S, I2C).
 *
 * Why not reuse `u8x8_d_st756x_flipper` from Momentum?
 *   - it toggles `gpio_display_rst_n` (PB0), which this board uses as nRF24 CSN;
 *   - it is bound to the SPI byte layer at compile time in practice.
 * This callback keeps the same command semantics but talks only to
 * st7567_i2c.c, so no SPI bus and no reset pin are touched.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */

#include "u8g2_st7567_i2c.h"
#include "st7567_i2c.h"

#include <string.h>

static const u8x8_display_info_t u8x8_st7567_i2c_info = {
    .chip_enable_level = 0,
    .chip_disable_level = 1,
    .post_chip_enable_wait_ns = 0,
    .pre_chip_disable_wait_ns = 0,
    .reset_pulse_width_ms = 0, /* no reset pin: software reset in INIT */
    .post_reset_wait_ms = 5,
    .sda_setup_time_ns = 0,
    .sck_pulse_width_ns = 0,
    .sck_clock_hz = 0, /* I2C, not SPI */
    .spi_mode = 0,
    .i2c_bus_clock_100kHz = 4, /* 400 kHz class */
    .data_setup_time_ns = 0,
    .write_pulse_width_ns = 0,
    .tile_width = 16, /* 16 tiles * 8 px = 128 */
    .tile_height = 8, /* 8 pages * 8 px = 64 */
    .default_x_offset = 0,
    .flipmode_x_offset = 0,
    .pixel_width = ST7567_WIDTH,
    .pixel_height = ST7567_HEIGHT,
};

uint8_t u8x8_d_st7567_i2c(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    switch(msg) {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
        u8x8_d_helper_display_setup_memory(u8x8, &u8x8_st7567_i2c_info);
        break;

    case U8X8_MSG_DISPLAY_INIT:
        /* No hardware reset line: initialise the controller in software. */
        st7567_i2c_init();
        break;

    case U8X8_MSG_DISPLAY_DRAW_TILE: {
        const u8x8_tile_t* tile = (const u8x8_tile_t*)arg_ptr;
        if(tile == NULL || tile->tile_ptr == NULL || tile->cnt == 0) break;

        st7567_i2c_write_page(
            tile->y_pos,
            (uint8_t)(tile->x_pos * 8 + u8x8->x_offset),
            tile->tile_ptr,
            (size_t)tile->cnt * 8);
        break;
    }

    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
        st7567_i2c_set_display_enabled(arg_int == 0);
        break;

    case U8X8_MSG_DISPLAY_SET_CONTRAST:
        /* u8g2 passes 0..255; ST7567 electronic volume is 0..63. */
        st7567_i2c_set_contrast((uint8_t)(arg_int >> 2));
        break;

    case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
        st7567_i2c_set_flipped(arg_int != 0);
        break;

    default:
        return 0;
    }

    return 1;
}

void u8g2_Setup_st7567_i2c_flipper(u8g2_t* u8g2, const u8g2_cb_t* rotation) {
    uint8_t tile_buf_height;
    uint8_t* buffer;

    /* cad/byte callbacks are unused: the display callback drives the bus
     * directly. u8x8_dummy_cb satisfies u8g2's GPIO/delay slot. */
    u8g2_SetupDisplay(
        u8g2, u8x8_d_st7567_i2c, u8x8_cad_empty, u8x8_byte_empty, u8x8_dummy_cb);

    /* 16x8 tiles, vertical-top-LSB: identical layout to the stock panel.
     * 1024-byte static framebuffer, same as the stock driver. */
    buffer = u8g2_m_16_8_f(&tile_buf_height);
    u8g2_SetupBuffer(u8g2, buffer, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}
