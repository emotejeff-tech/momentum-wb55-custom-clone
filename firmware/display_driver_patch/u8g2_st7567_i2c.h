/**
 * @file u8g2_st7567_i2c.h
 * @brief u8g2 binding for the GM12864-59N (ST7567S over I2C).
 *
 * Drop-in replacement for Momentum's `u8g2_Setup_st756x_flipper()` call in
 * applications/services/gui/canvas.c, but without any SPI or reset-pin
 * dependency.
 *
 * Part of momentum-wb55-custom-clone. GPL-3.0-or-later.
 */
#pragma once

#include <u8g2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise u8g2 for a 128x64 ST7567S on I2C.
 *
 * Signature-compatible with Momentum's `u8g2_Setup_st756x_flipper()`: the
 * 1024-byte full framebuffer comes from u8g2's static allocator
 * (`u8g2_m_16_8_f`), so canvas.c only needs the setup call swapped.
 *
 * @param u8g2     u8g2 context (owned by the caller, typically Canvas).
 * @param rotation u8g2 rotation, usually U8G2_R0.
 */
void u8g2_Setup_st7567_i2c_flipper(u8g2_t* u8g2, const u8g2_cb_t* rotation);

/** @brief u8x8 display callback, exposed for tests. */
uint8_t u8x8_d_st7567_i2c(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

#ifdef __cplusplus
}
#endif
