# Display driver patch — GM12864-59N (ST7567S, I²C) for Momentum

This directory contains a self-contained u8g2 driver for the **GM12864-59N**
128×64 COG LCD (ST7567S controller, 4-pin I²C module) and the two-line change
needed in Momentum's GUI layer.

| File | Purpose |
| --- | --- |
| `st7567_i2c.[ch]` | Controller transport: reset/init, contrast, flip, page writes, fill. Uses Furi HAL I²C (I2C1 handle). |
| `u8g2_st7567_i2c.[ch]` | u8g2 display callback + `u8g2_Setup_st7567_i2c_flipper()` (signature-compatible with the stock setup call). |
| `README.md` | This file. |

Nothing here depends on SPI, on `gpio_display_cs/di/rst_n`, or on Momentum's
`lib/u8g2/u8g2_glue.c`. The stock ST756x driver is left untouched for other
boards.

---

## 1. Why a new driver instead of reusing `u8x8_d_st756x_flipper`

The stock driver is good and its command sequences are correct, but:

1. It drives an **SPI byte layer** (`u8x8_hw_spi_stm32`), which this board does
   not have for the display.
2. Its init path toggles **`gpio_display_rst_n` = PB0**, which this board uses
   as **nRF24 CSN**. Pulsing it would corrupt nRF24 state.
3. Its contrast selection keys off `furi_hal_version_get_hw_display()`, which
   is meaningless on a custom board.

The replacement keeps the same ST7567 command set (reset, bias 1/9, segment
normal, COM reverse, regulation ratio, electronic volume, power control,
display on) and the same u8g2 buffer layout (`vertical_top_lsb`, 16×8 tiles,
1024 bytes), so the GUI cannot tell the difference.

## 2. Install into a Momentum tree

```bash
# from the Momentum root
cp firmware/display_driver_patch/st7567_i2c.*      lib/u8g2/
cp firmware/display_driver_patch/u8g2_st7567_i2c.* lib/u8g2/
```

`lib/u8g2/SConscript` uses `GlobRecursive("*.c")`, so no build-file edit is
needed for the sources. The headers are on the same include path
(`#/lib/u8g2`).

### 2.1 Patch `applications/services/gui/canvas.c`

```diff
 #include <stdint.h>
-#include <u8g2_glue.h>
+#include <u8g2_st7567_i2c.h>
 ...
-    u8g2_Setup_st756x_flipper(&canvas->fb, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
+    u8g2_Setup_st7567_i2c_flipper(&canvas->fb, U8G2_R0);
```

That is the only GUI change. `u8g2_InitDisplay()` and `u8g2_SetPowerSave()`
below it keep working and now reach the I²C controller.

### 2.2 Target-side changes (see `firmware/target_f55/`)

- Do **not** initialise the display SPI handle in
  `furi_hal_spi_config_init_early()`; the pins become nRF24.
- Remove the display CS/DC/RST GPIO configuration from
  `furi_hal_resources_init_early()`; PB0/PC11/PB1 become nRF24 CSN/CE/IRQ.
- Make `furi_hal_version_get_hw_version()` return **≥ 11** (e.g. 12). Stock
  `furi_hal_i2c_config.c` selects 100 kHz for version ≤ 10 and 400 kHz above;
  the display needs 400 kHz.
- Keep I²C1 on `PA9`/`PA10` (the stock "power" handle). The BQ25896/BQ27220/
  LP5562 devices are absent and their failed probes are non-fatal.

## 3. Initialisation sequence (what the driver sends)

```
0xE2            software reset            + 5 ms
0xA2            1/9 bias
0xA0            SEG normal
0xC8            COM reverse
0x40            start line 0
0x25            regulation ratio (Rb/Ra)
0x81 0x28       electronic volume = 0x28 + 2 ms
0x2C            booster on                + 2 ms
0x2E            + voltage regulator       + 2 ms
0x2F            + voltage follower        + 2 ms
0xA4            all pixels off
0xA6            normal (not inverted)
0xAF            display on
```

Derived from the ST7567S command set and the working community sequence for
this exact module (see `docs/RESEARCH_FINDINGS.md` §5.2). Flipper's stock
driver uses a shorter `0x2F`-only power-up; the staged version is safer for a
COG panel with an internal charge pump.

**Panel-batch tuning:** if the image is too dark/washed out, sweep
`st7567_i2c_set_contrast()` 0x18–0x38 and the regulation ratio 0x24–0x27.
Expose both in a settings menu rather than hard-coding them per build.

## 4. Timing and performance

| Operation | Bytes on bus | Time at 400 kHz | Notes |
| --- | --- | --- | --- |
| Full frame | 1024 + overhead | **≈ 23–25 ms** | u8g2 only calls DRAW_TILE for dirty tiles |
| Tile row (16 tiles) | 128 + 2 control | ≈ 3 ms | one `st7567_i2c_write_page()` |
| Command | 2 | ≈ 50 µs | many small transactions during init |

The GUI thread is blocked for the duration of each transfer because
`furi_hal_i2c_tx()` is synchronous. At 30 fps that is up to ~70 % of the frame
budget for full-screen animations; menus and text screens are much cheaper
because u8g2 redraws dirty tiles only.

If you need more headroom, in order of effort:

1. **Batch tiles.** u8g2 may call DRAW_TILE once per tile row; coalescing
   consecutive rows into one 1 KB transfer saves START/STOP overhead but does
   not change the 9-bit-per-byte cost.
2. **I²C TX DMA.** Add an LL-level transfer (I2C1 TX DMA via DMAMUX) and return
   from DRAW_TILE immediately; needs a completion callback before the next
   frame. ~23 ms of bus time but ~0 % CPU.
3. **SPI panel.** The same 59N glass is sold with an SPI carrier; the PCB in
   this project keeps a footprint for both. That is the only way to get
   sub-5 ms frames.

## 5. Bring-up test

```c
/* temporary, e.g. in an app's on_start */
st7567_i2c_probe();
st7567_i2c_init();
st7567_i2c_fill(0xFF);          /* all pixels on  */
furi_delay_ms(500);
st7567_i2c_fill(0x00);          /* all pixels off */
st7567_i2c_fill(0xAA);          /* checkerboard   */
```

Expected: uniform white, uniform black, then a 1-px checkerboard. If the
checkerboard is shifted or wrapped, the column/page addressing is off by one —
check `ST7567_WIDTH` and the panel's `default_x_offset` (0 for this module).

## 6. Known caveats

- **No hardware reset.** If the controller locks up (ESD, brown-out), only a
  power cycle recovers it. Add a load switch or an RC on 3V3 if this matters.
- **Bus sharing.** The driver uses the stock `furi_hal_i2c_handle_power`
  (I²C1). On this board no other device answers, but the power/light drivers
  still probe it at boot. Their NACKs are harmless; if you patch them out
  (recommended), the bus becomes display-only.
- **Backlight is always on.** See `hardware/PINOUT_AND_WIRING.md` §7.1 for the
  dimming mod.
- **Static framebuffer.** `u8g2_m_16_8_f()` returns a single static 1 KB buffer,
  exactly like the stock driver. If you ever instantiate two u8g2 contexts for
  this display, they will share it.
