# Will my GM12864-59N work with this driver?

Short answer: **yes if yours is the 4-pin I²C 128×64 module sold as
"GM12864-59N" / "12864-59N IIC" with an ST7567S controller** — which is what the
vendor listings and teardowns show. The driver now auto-detects all four
possible ST7567S addresses (`0x3C`–`0x3F`), so the common `0x3F` and the less
common `0x3E` batches both work without a rebuild.

If your module has 8+ pins, `CS`/`A0`/`RST` labels, or a UC1701 controller, it
is **not** compatible with this driver — see §1.

This page is the acceptance test: do §4 before soldering anything permanent.

---

## 0. Confirmed unit (the one this repo was checked against)

The AliExpress listing
[`3256811882689703`](https://www.aliexpress.us/item/3256811882689703.html)
sells the **Estardyn 12864-59N**: 128×64 COG, metal frame, **4-pin I²C**
("4P IIC Interface"), 3.3 V. Estardyn's own 59N I²C product data identifies the
controller as **ST7567S** (model `12864-59N`, LCD type COG, resolution 128×64).

That is exactly the panel this driver targets:

| Property | This unit | Driver expectation |
| --- | --- | --- |
| Resolution | 128×64 | `ST7567_WIDTH`/`HEIGHT` = 128/64 |
| Interface | 4-pin I²C | `st7567_i2c.c` |
| Controller | ST7567S | ST7567 command set |
| Address | `0x3C`–`0x3F` (strapped) | auto-detected; usually `0x3F` |
| Supply | **3.3 V** | do not apply 5 V |
| Backlight | hard-wired on | no brightness pin; expect ~20–30 mA |

No code change is needed for this unit: the defaults (contrast `0x28`,
regulation `0x25`, 1/9 bias, 4× booster, no flip) are the 59N defaults. The
4-pin header order is silkscreened; on 59N modules it is usually
`GND / VCC / SCL / SDA` (some batches label it `3V3 / GND / SCL / SDA`). With
the power off, check the silkscreen before connecting; if the scan finds nothing,
swapping SCL/SDA is the first thing to try.

> I could not read the controller marking through the metal frame — no one can
> without destroying the module. The identification above comes from the
> listing/model data plus the Estardyn-branded product image and the
> Estardyn 59N ST7567S product metadata, not from a die shot. §4 is the
> definitive test for your specific unit.

## 1. Identify the module you actually have

| What you see | Controller / interface | Works with this repo? |
| --- | --- | --- |
| 4 pins: `3V3`, `GND`, `SCL`, `SDA`; 128×64; metal frame; "59N" | **ST7567S**, I²C, addr `0x3C`–`0x3F` | **Yes** — `st7567_i2c.c` |
| Same glass but 8 pins / `CS`, `A0`, `RST`, `SCK`, `SDA` | ST7565/ST7567, **SPI** | Not this driver; use Momentum's stock ST756x SPI driver and keep the display on SPI2 |
| 4-pin I²C but listing says **UC1701** | UC1701, I²C | **No** — different command set; needs a separate driver |
| 4-pin I²C, 128×64, controller **ST7565R** | ST7565R | Usually works (same core commands); if contrast/power misbehaves, try `bias_1_7`/`booster_5x` and the tuning in §6 |
| 4-pin I²C, 132×64 or 128×32 | ST7567 variant with different RAM window | Partially: needs `ST7567_WIDTH`/offset changes; not supported out of the box |

The 59N I²C module is consistently reported as ST7567S:
vendor listings ("12864-59N IIC 4P ST7567S COG"), David Albert's teardown, and
the community drivers all agree. The controller sits on the flex/PCB under the
metal frame; you normally cannot read its marking without destroying the module,
so trust the listing and the address scan.

## 2. Electrical requirements

| Item | Requirement | Consequence if wrong |
| --- | --- | --- |
| Supply | **3.3 V only** | 5 V can destroy the controller/backlight |
| Current | ~1 mA logic + ~20–30 mA backlight (always on) | battery drain; see dimming mod in the pinout |
| Pull-ups | 4.7 kΩ from SCL and SDA to 3.3 V **if the module has none** | no ACK / random failures |
| Bus speed | 100 kHz safe, 400 kHz supported | long wires at 400 kHz → corrupted pixels |
| Reset | none on the module; software `0xE2` | a wedged controller needs a power cycle |

Measure the module unplugged: if SCL–3V3 and SDA–3V3 read ~4.7 kΩ, pull-ups are
already fitted; if open, add them.

## 3. Address detection

The ST7567S has four slave addresses selected by `SA0`/`SA1`:

| SA1 | SA0 | 7-bit address |
| --- | --- | --- |
| 0 | 0 | `0x3C` |
| 0 | 1 | `0x3D` |
| 1 | 0 | `0x3E` |
| 1 | 1 | `0x3F` |

Most 59N modules strap `0x3F`; batches at `0x3E` exist. `st7567_i2c_init()`
probes `0x3C..0x3F`, logs every responder, and uses the first one. You can also
call `st7567_i2c_scan()` yourself and print the result.

> A `0x3C`/`0x3D` responder could also be an SSD1306 OLED on the same bus, but
> on a dedicated display bus that is unlikely. If you see two responders, find
> the conflict before trusting the display.

## 4. Ten-minute acceptance test (do this first)

1. **Power:** 3.3 V + GND only; measure 3.3 V at the module.
2. **Scan:** call `st7567_i2c_scan()` (or run an I²C scanner) and confirm
   exactly one address in `0x3C..0x3F`.
3. **Init:** call `st7567_i2c_init()`; it returns `false` if nothing ACKs.
4. **Patterns:**
   ```c
   st7567_i2c_fill(0xFF);   /* all pixels on  */
   furi_delay_ms(500);
   st7567_i2c_fill(0x00);   /* all pixels off */
   furi_delay_ms(500);
   st7567_i2c_fill(0xAA);   /* 1-px checkerboard */
   ```
   Expected: uniform white, uniform black, fine checkerboard. Any deviation is
   in the table below.
5. **Text:** with the u8g2 glue installed, the Momentum UI should render after
   the `canvas.c` patch.

If you do not have the STM32 side running yet, flash
[`tools/lcd_check/lcd_check.ino`](../tools/lcd_check/lcd_check.ino) to any
3.3 V Arduino/ESP32. It runs the same command sequence over a different I²C
stack, which separates "bad panel/wiring" from "bad firmware" in one minute.

## 5. Failure matrix

| Symptom | Most likely cause | Fix |
| --- | --- | --- |
| `st7567_i2c_scan()` returns 0 | SDA/SCL swapped, no pull-ups, module is SPI, no 3.3 V, or a dead module | continuity check; add 2×4.7 kΩ; confirm 4-pin I²C; try 100 kHz |
| ACK but completely blank | init not called, contrast at 0, or booster/power stage failed | call `init()`, then `set_contrast(0x28)`; try `booster_5x=true` |
| All pixels black/white, no pattern | contrast saturated or panel batch needs a different regulation ratio | sweep contrast `0x18..0x38`, ratio `0x24..0x27` |
| Image mirrored / upside down | segment/COM direction differs by batch | `st7567_i2c_set_flipped(true)` or `config.flipped = true` |
| Image shifted 4 px, right edge wrapped | 132-column controller variant (not the 59N) | not supported; needs a column offset |
| Garbled pixels, works then corrupts | bus too fast / long wires / missing pull-ups | 100 kHz, shorter wires, 4.7 kΩ |
| Works, then freezes after ESD/brown-out | no hardware reset line | power-cycle; add a 3.3 V load switch if it matters |
| Faint/unreadable without backlight | panel is reflective-negative; backlight is essential | backlight is hard-wired on; do not cut R5 without a dimming mod |

## 6. Tuning for your batch

```c
St7567Config cfg;
st7567_i2c_default_config(&cfg);   /* 0x3F auto, contrast 0x28, ratio 0x25,
                                      1/9 bias, 4x booster, not flipped */
cfg.contrast = 0x30;               /* 0x00..0x3F */
cfg.regulation_ratio = 0x26;       /* 0x20..0x27 */
cfg.booster_5x = true;             /* try if the image is too faint at 3.3 V */
cfg.flipped = true;                /* if the image is upside down */
if(!st7567_i2c_init_ex(&cfg)) { /* report wiring error */ }
```

Start with the defaults. Change **one** parameter at a time and re-check the
checkerboard; contrast and regulation interact.

## 7. What has actually been verified here

Byte-level host tests (see `tests/display_driver_test.c`) prove:

- the exact I²C stream: address selection, `0x00`/`0x40` control bytes, the
  14-transaction init sequence, page/column addressing, clipping, contrast,
  flip and a full 8-page fill;
- auto-detection fallback: `0x3F` absent, `0x3E` present → init succeeds on
  `0x3E`; no device → `init()` returns false;
- config variants: 1/7 bias, 5x booster, flipped, custom contrast/ratio.

They **cannot** prove that your specific unit's flex, backlight or controller
batch behaves identically. §4 is the real acceptance test; if it fails, §5 maps
the symptom to a cause.

## 8. If your module turns out to be SPI

Do not force it onto I²C. Keep the stock display bus and driver:

- wire `CS/DC/RST/SCK/MOSI` to `PC11/PB1/PB0/PD1/PB15` (stock `targets/f7` map);
- drop the nRF24 or move it to SPI1;
- do **not** apply the I²C canvas patch.

The rest of the board (buttons, SD, CC1101, IR) is unchanged.
