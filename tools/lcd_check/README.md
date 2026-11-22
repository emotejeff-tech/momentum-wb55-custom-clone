# LCD diagnostic (Arduino / ESP32)

Proves the GM12864-59N and its wiring **without** the STM32 or Momentum. Useful
when the panel behaves oddly and you need to know whether the fault is the
module, the wiring, or the firmware.

## Use

1. Wire the 4-pin module to a **3.3 V** Arduino/ESP32:

   | LCD | Board |
   | --- | --- |
   | 3V3 | 3V3 |
   | GND | GND |
   | SCL | SCL (ESP32 default GPIO22) |
   | SDA | SDA (ESP32 default GPIO21) |

   Add 4.7 kΩ pull-ups to 3V3 on SCL/SDA if the module has none.

2. Open `lcd_check.ino`, select your board, upload, open the serial monitor at
   **115200**.

3. Read the output:

   ```
   Scanning 0x3C..0x3F ...
     ACK at 0x3F
   Using 0x3F
   Init sent. Expect: white -> black -> checkerboard -> sweep.
   ```

   - No ACK → wiring, pull-ups, 3.3 V, or the module is SPI/UC1701.
   - ACK but no image → contrast/booster; the sketch sweeps contrast at the end.
   - Mirrored image → SEG/COM direction; the production driver has a `flipped`
     option.

## Why this helps

It uses the **same command sequence** as
`firmware/display_driver_patch/st7567_i2c.c` but a different I²C stack. If this
sketch works, the panel and wiring are good and any remaining issue is in the
STM32 integration. If it fails here, no amount of firmware work will fix it.

## Caveats

- This is a diagnostic, not the production driver: no u8g2, no framebuffer,
  blocking writes only.
- 100 kHz by default. Try 400 kHz only with short wires and pull-ups fitted.
- The backlight is hard-wired on the module; a dark screen with ACK is almost
  always contrast/init, not backlight.
