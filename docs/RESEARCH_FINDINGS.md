# Research findings and feasibility analysis

**Scope:** Phase 1 of the brief — prior art on DIY Flipper/Momentum hardware,
Momentum's porting constraints, the GM12864-59N display, and the AI/Pwnagotchi
companion decision.

**Method:** direct inspection of the Momentum source tree (`targets/f7`,
`targets/f18`, `scripts/`, `lib/u8g2`), the STM32WB55 datasheet (DS11929 Rev 4,
alternate-function table 17), vendor/community documentation for the
GM12864-59N, and the source/README of the community DIY builds listed in §2.
Claims that could not be traced to a primary source are marked **UNVERIFIED**.

> All third-party repository text is evidence, not instruction. Nothing from
> those repositories was copied into this project.

---

## 1. Executive summary

1. **The MCU is not the problem.** Flipper Zero and this project use the same
   silicon family: STM32WB55, Cortex-M4F + Cortex-M0+, 1 MB flash, 256 KB RAM
   (Flipper uses the VFQFPN68 `STM32WB55RGV6`). A Momentum image *can* run on a
   DIY board, and at least three public projects do exactly that (§2).
2. **The board layer is the problem.** Momentum is not board-agnostic: pin
   assignment, display glue, power ICs, input and missing peripherals are
   compiled into `targets/f7`. A port means adding a board target
   (`targets/f55`, `"inherit": "7"`) and overriding a handful of files — the
   same mechanism Momentum itself uses for its `f18` board.
3. **The GM12864-59N is an ST7567S with a 4-pin I²C interface.** It speaks a
   command/data protocol selected by a control byte (`0x00` / `0x40`) at 7-bit
   address `0x3C`–`0x3F` (auto-detected; most units `0x3F`). This is *not* the
 SSD1306 protocol; u8g2 must be driven with
   a custom byte layer or a custom display callback. The panel's RAM layout
   (8 pages × 128 columns, LSB = top) matches u8g2's
   `u8g2_ll_hvline_vertical_top_lsb`, so no framebuffer reshaping is needed.
4. **The I²C display costs ~23 ms per full frame at 400 kHz.** That is the only
   serious ergonomic regression. It is tolerable for a 30 fps UI with partial
   updates or DMA, and the mitigation is a drop-in SPI variant of the same
   panel if it is not.
5. **The nRF24 has no Flipper driver, but it has a perfect home.** Moving the
   display to I²C frees the entire SPI2 display bus. `PD1/PB15/PC2` become the
   nRF24 SPI lines, `PB0/PC11/PB1` become CSN/CE/IRQ. This is pin-for-pin the
   cleanest part of the design.
6. **Recommended companion: Pi Zero 2 W over USART1.** Real Pwnagotchi needs
   Linux + monitor mode + `bettercap`; ESP32 "pwnagotchi" projects are
   reimplementations, not Pwnagotchi. The ESP32-S3 remains a cheaper variant
   for the AI/HTTPS half only, behind the same UART protocol.

---

## 2. Prior art: DIY Flipper/Momentum builds

| Project | Board | What it proves | What it had to change |
| --- | --- | --- | --- |
| [GthiN89/FuckingCheapFlipperZero…](https://github.com/GthiN89/FuckingCheapFlipperZero-DIY-Flipper-zero-The-real-on) ("Nucleus Dark MK1", ~400★) | STM32WB55**CG**U6 (WeAct, 48-pin) | A hand-soldered, module-based Momentum device is real and released; OTP write + qFlipper flashing works on DIY hardware | Rewrote input (PISO shift register), stubbed speaker/vibro/125 kHz RFID, remapped header, **nRF24 not supported**, display is SPI ST756x |
| [lamtranBKHN/diy_flipper_zero](https://github.com/lamtranBKHN/diy_flipper_zero) (~37★) | STM32WB55CGU6 | I²C **OLED** (SSD1306/SH1106) + MCP23017 button expander + INA219 + CC1101 + SD + IR all coexist under modified Momentum | NFC dropped; buttons moved to MCP23017; SD/CC1101 share SPI1; custom I²C/OLED glue |
| [AJ60/Flipper_OLED_PCF8574_PN532](https://github.com/AJ60/Flipper_OLED_PCF8574_PN532) | STM32WB55 custom | I²C1 (PA9/PB9) hosts OLED + PCF8574 + INA219; I²C3 (PA7/PB4) hosts PN532; SPI1 hosts SD + CC1101 | Display and input drivers replaced; several HAL pin macros redefined |
| [cbwhickstein/Flipper_Zero_DIY](https://github.com/cbwhickstein/Flipper_Zero_DIY) (~100★) | custom, from open schematic | The published schematic + firmware are sufficient to build a clone | Unfinished; no firmware port documented |
| [enexis1337/DIY-Flipper-PCB](https://github.com/enexis1337/DIY-Flipper-PCB) (~32★) | 1:1 PCB | Mechanical/electrical form factor is reproducible | PCB only, no firmware target |
| [Magnowz/Flipper-Diy](https://github.com/Magnowz/Flipper-Diy) (~107★) | modules | Community knowledge base referenced by Nucleus Dark | — |

**Takeaways**

- Every successful port had to (a) write a board-specific
  `furi_hal_resources.*`, (b) replace at least the display glue, and (c) decide
  what to do about NFC/RFID/iButton.
- Display replacement is *solved* on I²C in the community (SSD1306/SH1106), but
  **no public project documents an ST7567S I²C port** — that is the novel piece
  here, and §5 addresses it in detail.
- The 48-pin WB55CG forces an input expander (MCP23017/PCF8574) or shift
  register. A 68-pin WB55RG has enough pins for six discrete buttons and is the
  better base for this design.
- Community README pin tables are not always consistent with each other or with
  DS11929 (e.g., clicking through the Nucleus Dark table shows `PA3`/`PA15`
  assignments that differ from stock). **All pins in this project were checked
  against DS11929 Table 17, not against community tables.**

---

## 3. How a board target works in Momentum (the porting mechanism)

Momentum's build selects a hardware target with `TARGET_HW` and loads
`targets/f<TARGET_HW>` (`scripts/fbt_tools/fbt_hwtarget.py`):

- `target.json` can declare `"inherit": "<id>"`, which layers the parent
  target's sources, include paths, linker scripts and SDK symbols.
- `excluded_sources`, `excluded_headers` and `excluded_modules` remove
  board-incompatible code.
- Source files with the same name in the child target **override** the parent
  (the loader de-duplicates by filename).

This is exactly how Momentum ships `targets/f18` ("Not Flipper Zero"), which
inherits `f7` and excludes NFC, 125 kHz RFID, Sub-GHz and IR. **`f18` is the
template for this project's `f55` target** (`firmware/target_f55/`).

Required build-system edit: `site_scons/commandline.scons` restricts
`TARGET_HW` to `["7", "18"]`; a new value `"55"` must be added.

---

## 4. Momentum's mandatory/peripheral assumptions

### 4.1 Flash and RAM map (from the f7 linker script and `scripts/flash.py`)

| Region | Address | Size | Notes |
| --- | --- | --- | --- |
| Bootloader | `0x08000000` | 32 KB | flashed separately, then firmware at `0x08008000` |
| Firmware (app) | `0x08008000` | up to `0x080E0000` (**864 KB**) | includes assets, GUI, apps, u8g2 |
| Wireless stack (CPU2) | ≤ `0x080E0000` | top 128 KB | `flash.py` refuses a radio stack above `0x080E0000` |
| SRAM1 (app) | `0x20000008` | 0x2FFF8 (~192 KB) | FreeRTOS heap + stacks |
| SRAM2A | `0x20030000` | 10 KB | IPCC / BLE shared memory |
| SRAM2B | `0x20038000` | 10 KB | reserved for the wireless stack |

Consequences: there is **no room for a second firmware slot**; an SD card is
the only practical app/data store; and adding large drivers (Wi-Fi stacks,
TLS, LVGL) to the M4 is not an option. This is the quantitative argument for
putting AI/Pwnagotchi on a companion.

### 4.2 Peripherals the firmware expects

| Subsystem | Expectation in stock `targets/f7` | Status here |
| --- | --- | --- |
| Display | ST7565/ST7567 over **SPI2**, `u8x8_cad_001`, 1024-byte buffer, 4 MHz, CS `PC11`, DC `PB1`, RST `PB0`, SCK `PD1`, MOSI `PB15`, MISO `PC2` | **Replaced** by I²C ST7567S; SPI2 pins reused for nRF24 |
| microSD | SPI2 (shared with display), CS `PC12`, card-detect `PC10`, power switch `PA3` | Kept (SPI2 now shared with nRF24) |
| Sub-GHz | CC1101 on SPI1, CS `PD0`, GDO0 `PA1`, RF switch `PC4` | Kept |
| NFC | ST25R3916 on SPI1, CS `PE4`, IRQ `PA2` | **Removed** |
| 125 kHz RFID | analog front-end on `PB13/PB14/PC5/PA15/PC4` + comparator | **Removed** |
| iButton | 1-Wire on `PB14` | **Removed** |
| IR | RX `PA0` (TIM2_CH1 capture), TX `PB9` (TIM1_CH3N + DMA2) | Kept |
| Speaker | `PB8` (TIM16_CH1) | Optional |
| Vibro | `PA8` (GPIO) | Optional |
| Buttons | `PB10` up, `PC6` down, `PB11` left, `PB12` right, `PH3` OK, `PC13` back | Kept |
| Power ICs | BQ25896 charger `0x6B`, BQ27220 gauge `0x55`, LP5562 LED driver `0x30` — all on **I²C1** (`PA9`/`PA10`) | **Absent**; bus repurposed for the display |
| USART | USART1 `PB6`/`PB7` (external header, used by CLI/apps) | Kept; used as companion link |
| USB | `PA11`/`PA12`, full-speed | Kept |

### 4.3 Display porting surface

Momentum's display path is small and well isolated:

- `applications/services/gui/canvas.c` calls
  `u8g2_Setup_st756x_flipper(&fb, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32)`.
- `lib/u8g2/u8g2_glue.c` implements the SPI byte callback (Furi HAL SPI bus
  `display`), the GPIO/delay callback (reset pin + delays) and the ST756x
  display callback (init sequence, contrast, flip, draw-tile).
- The buffer is `u8g2_m_16_8_f` = 16×8 tiles = **1024 bytes**, vertical-top-LSB.

Therefore only **one call site** (`canvas.c`) and **one new driver** are needed;
no u8g2 core changes, no filesystem/GUI changes. This is why the I²C display is
a contained change rather than a fork.

### 4.4 Input

Stock input is six GPIOs with edge interrupts; `input_pins[]` in
`furi_hal_resources.c` defines polarity. Note `OK` is on `PH3`, which is
**BOOT0**: it is active-high with a pull-down, and pressing it while resetting
enters the bootloader. This is intentional on Flipper and is preserved here —
it doubles as the DFU button.

A 48-pin WB55CG cannot afford six button GPIOs once radios/SD/IR are placed;
that is why the community builds use MCP23017/PCF8574/PISO. Our recommended
68-pin board avoids the expander entirely.

### 4.5 Board identity (OTP) and BLE stack

`furi_hal_version.c` reads a board-identity structure from OTP at
`0x1FFF7000` (magic `0xBABE`, board version/target/body/connect, name, region).
Without it the firmware falls back to defaults; the DIY community reports that
qFlipper/DFU flows behave better with a valid OTP blob. Writing OTP is
**irreversible** — see the target README §5 for the exact procedure.

The Cortex-M0+ wireless stack is a separate binary flashed at ≤ `0x080E0000`.
Without it, BLE is dead but the M4 firmware still runs.

### 4.6 Graceful degradation of the stock power layer

`furi_hal_power_init()` probes BQ25896 and BQ27220 and stores `charger_ok` /
`gauge_ok`; failures are tolerated. However
`furi_hal_power_get_battery_voltage()` and `..._get_pct()` return the IC values
unconditionally, so a board without those ICs will show **0 % / 0 V** in the
status bar. This is cosmetic at boot and must be patched (ADC divider or a
BQ27220 module) before the device is usable on battery. See
`firmware/target_f55/NOTES_POWER_AND_LIGHT.md`.

The LP5562 RGB driver also lives on I²C1. On this board those writes simply
NACK (address `0x30` ≠ display `0x3F`) and are harmless.

---

## 5. GM12864-59N display: identification and integration

### 5.1 Controller identity

Multiple independent sources identify the module as an **ST7567S** COG panel
with a 4-pin I²C carrier:

- Vendor listings describe it as "12864-59N IIC 4P … ST7567S COG"
  (Tenstar/TZT/AliExpress).
- David Albert's teardown confirms the ST7567S, the 4-pin
  `3V3/GND/SCL/SDA` header, and that the LED backlight is **hard-wired to
  power** through a 100 Ω resistor (`R5`), with no enable pin.
- The ST7567S command set (pages, columns, `0xA0/0xA1`, `0xC0/0xC8`, `0x2F`
  power, `0x81` electronic volume, `0x25` regulation ratio) matches the panel.

**Not** an SSD1306: the SSD1306 uses a completely different init sequence and
column/page addressing, and its I²C control byte semantics are similar but the
display RAM organization differs (SSD1306 is 8 pages × 128 columns too, but the
segment mapping and charge pump commands differ). Do not reuse an SSD1306
driver.

### 5.2 Electrical/protocol facts

| Item | Value | Source |
| --- | --- | --- |
| Interface | I²C, 2 wires, no RST/DC/CS | module pinout |
| 7-bit address | `0x3C`–`0x3F` (SA0/SA1); 59N units usually `0x3F`, some `0x3E` | ST7567S datasheet; community drivers |
| Control byte | `0x00` = command, `0x40` = data (Co = 0) | ST7567S datasheet; community driver |
| Max I²C clock | 400 kHz class (u8g2 advertises `i2c_bus_clock_100kHz = 4` for this controller) | Flipper `u8x2_glue.c` display info |
| Supply | 3.3 V | module |
| Backlight | always on, ~20–30 mA | teardown; R5 = 100 Ω |
| Framebuffer | 8 pages × 128 columns × 8 bits, LSB = top row | ST7567 RAM map |

Typical init sequence used by working ST7567S-I²C code:

```
0xE2            software reset
0xA2            1/9 bias (128x64)
0xA0            SEG normal direction
0xC8            COM reverse direction
0x25            regulation ratio (Rb/Ra)
0x81 0x20       electronic volume (contrast)
0x2C 0x2E 0x2F  power control: booster on, regulator on, follower on
0xAF            display on
0x40            start line 0
```

Flipper's own driver sends a shorter variant (`0xE2, 0xA2, 0xA0, 0xC8, 0x25,
0x81/contrast, 0x2F`) and works on ST7565/ST7567 panels; the staged
`0x2C → 0x2E → 0x2F` sequence is safer for a COG module with an internal charge
pump and is what `firmware/display_driver_patch/` implements (with 1–5 ms
delays between stages).

### 5.3 Why this is a good fit for u8g2

- The panel wants exactly the layout u8g2 calls `vertical_top_lsb`, and Flipper
  already uses that mode.
- u8g2's `u8x8_cad_001` produces command/data runs with a DC line; on I²C the
  "DC line" is the control byte. A ~120-line byte callback is enough to reuse
  90 % of u8g2's ST756x code.
- **However**, Flipper's `u8x8_d_st756x_flipper` also toggles the hardware reset
  pin (`PB0`), which this board needs for nRF24 CSN. The clean solution is a
  self-contained display callback that never touches `PB0` (implemented in
  `firmware/display_driver_patch/`). It keeps the same command sequences and
  contrast semantics but drives them through the I²C transport.

### 5.4 Bandwidth math

- One full frame = 1024 bytes.
- I²C at 400 kHz: 9 bits/byte (8 + ACK) → 9216 bits → **23.0 ms** plus
  START/STOP/control-byte overhead (~2–4 %).
- Flipper's GUI targets ~30 fps (33 ms budget). A blocking full-frame flush
  therefore consumes ~70 % of the GUI thread's budget.
- Mitigations, in order of effort:
  1. u8g2 already redraws only dirty tiles; the byte callback batches per
     `DRAW_TILE` call, so a menu transition rarely writes all 1024 bytes.
  2. Move the flush to I²C TX DMA (Furi HAL I²C does not expose DMA; LL-level
     driver needed).
  3. Use the **SPI variant of the same 59N panel** (same glass, different
     carrier) — the PCB has a footprint for either.
  4. Accept ~20–25 fps for full-screen animations.

### 5.5 Backlight and power

The backlight is not switchable on the stock module. For battery use:

- **Mod:** remove `R5` (100 Ω) and drive the backlight through a small
  N-MOSFET (e.g., 2N7002) from a spare PWM pin, with a 10–33 Ω series resistor.
  Contrast without backlight is unusable (the panel is reflective-negative),
  so this is only for dimming, not for off.
- **Alternative:** load-switch the module's 3.3 V. The controller loses state;
  re-init after power-up. Simpler, but cannot dim.

---

## 6. nRF24L01+ integration

- No Flipper/Momentum driver exists; the Nucleus Dark project explicitly lists
  nRF24 as unsupported. A driver must be written.
- The nRF24 is a 3.3 V, SPI mode-0 device, up to 10 MHz, with `CSN`, `CE` and
  an active-low `IRQ`. It needs 10–100 µF bulk capacitance and short ground
  returns; the PA/LNA variants draw ~115 mA peaks.
- Freed SPI2 pins map perfectly: `PD1` SCK, `PB15` MOSI, `PC2` MISO.
  `PB0` (ex-display reset, has a stock pull-up → CSN idles high) is CSN,
  `PC11` (ex-display CS) is CE, `PB1` (ex-display DC) is IRQ on **EXTI1**
  (EXTI0 is already IR `PA0`; EXTI1 is free).
- Firmware integration options:
  1. **FAP library** (`firmware/nrf24/` + an app): no core changes, user
     installs the app from SD. Recommended first step.
  2. A `furi_hal_nrf24` service plus an app in `applications/` — more work,
     better UX.
- Shares SPI2 with the SD card, so all access must go through Furi HAL bus
  handles (mutex-protected). nRF24 at ≤ 8 MHz avoids disturbing SD timing.

---

## 7. AI agent + Pwnagotchi: architecture analysis

### 7.1 Constraints that drive the decision

| Constraint | Number | Why it matters |
| --- | --- | --- |
| Free app flash | ≤ 864 KB total, shared with GUI/apps/assets | No TLS stack + LLM client + Wi-Fi driver on the M4 |
| Free RAM | ~192 KB, shared with FreeRTOS, GUI, BLE | No JSON/TLS buffers of any size |
| BLE stack | occupies CPU2 + top 128 KB flash + SRAM2 | M4 cannot also run Wi-Fi |
| Pwnagotchi | Linux + monitor mode + `bettercap` + Python | Not runnable on an MCU |
| AI API | HTTPS + JSON, 1–10 s latency | Trivial on Linux; possible on ESP32-S3 with mbedTLS |
| Power | 1S Li-ion ~2000 mAh | Pi Zero 2 W at 150–400 mA dominates runtime |

### 7.2 Options

**Option A1 — Pi Zero 2 W companion (UART).**
Runs Pwnagotchi (jayofelony fork, Python 3, Pi Zero 2 W images), plus a small
Python agent that calls an LLM API and exposes a line protocol over UART.
Pros: real Pwnagotchi; full Linux tooling; easy AI; SSH/web UI; can also host a
captive portal. Cons: +150–400 mA, +~10 s boot, +€20, 5 V rail, larger enclosure.
Best for: "I want the AI + Pwnagotchi features to actually work."

**Option A2 — ESP32-S3 companion (UART).**
Wi-Fi scanning/deauth (Marauder-class), HTTPS AI calls, BLE HID. Pros: ~€8,
~80–120 mA, instant boot, 3.3 V. Cons: **not Pwnagotchi** (no monitor mode /
bettercap; community ESP32 ports are reimplementations), less RAM for JSON/TLS,
firmware complexity moves to the ESP32. Best for: AI chat + Wi-Fi recon without
a Linux computer.

**Option B — host-tethered (USB CDC / BLE).**
No companion; a PC/phone runs the agent. Pros: zero extra hardware, best AI
quality, no power cost. Cons: not standalone; BLE serial throughput and latency
are poor for streaming; phone tethering is awkward. Best for: bench use and
development.

### 7.3 Recommendation

**Primary: A1 (Pi Zero 2 W) with a UART protocol that A2 can also speak.**
The protocol is the stable interface; swapping the companion later must not
require STM32 firmware changes. Option B is supported for free because the same
protocol runs over USB CDC (`/dev/ttyACM0` on the host).

Rationale: Pwnagotchi is a Linux application, not a protocol; only a Linux SBC
runs it. AI is an HTTPS client, which the Pi does better than any MCU. The
STM32 keeps doing what it is good at (real-time radios, IR, Sub-GHz, UI) and
the companion owns Wi-Fi/AI.

### 7.4 What the companion must not do

- It must not become the display or input path (latency, boot dependency).
- It must not be required for Sub-GHz/IR/NFC-class features; the device must
  boot and operate with the companion absent.
- It must not receive raw radio samples over UART; only decisions/telemetry.

---

## 8. Risks and open questions

| # | Risk | Severity | Status / mitigation |
| --- | --- | --- | --- |
| R1 | I²C display bandwidth makes the UI sluggish | Medium | Measure; partial updates + DMA; SPI panel fallback |
| R2 | ST7567S contrast/charge-pump tuning differs per batch | Low | Expose contrast in settings; test 0x20–0x3F |
| R3 | Momentum target scaffold does not build first try (SDK symbols, module exclusions) | High | Iterate `fbt TARGET_HW=55`; start from `targets/f18`, keep exclusions minimal |
| R4 | nRF24 driver absent → feature gap | Medium | Provide minimal driver + FAP; scope to scanning/PTX first |
| R5 | No battery gauge/charger → 0 % UI | Medium | ADC divider patch or BQ27220 module |
| R6 | OTP write bricks board identity | High | Document as irreversible; optional for bench |
| R7 | Pi Zero 2 W power/thermal in a small enclosure | Medium | 5 V 2 A boost, vents, measure |
| R8 | 48-pin WeAct board cannot host this pin map | High for that board | Use 68-pin Nucleo/WB55RG, or the reduced variant in PINOUT §9 |
| R9 | Legal/ethical use of Sub-GHz/nRF24/Pwnagotchi features | — | Receive-only defaults; comply with local RF law |

Open questions to resolve on hardware:

1. Actual full-frame flush time at 400 kHz with u8g2 dirty-tile behavior.
2. Whether the GM12864-59N batch ships with I²C pull-ups (measure; add 4.7 kΩ
   if absent).
3. Whether Momentum's first boot on a blank OTP is stable enough to skip OTP
   for bench bring-up (community reports vary).
4. Whether `f18`'s module exclusions are sufficient or whether NFC/RFID symbols
   leak into shared code and need stubs.

---

## 9. References

**Firmware / build system**

- Momentum firmware: <https://github.com/Next-Flip/Momentum-Firmware>
  - `targets/f7/target.json`, `targets/f7/furi_hal/furi_hal_resources.[ch]`,
    `furi_hal_spi_config.c`, `furi_hal_i2c_config.c`, `furi_hal_power.c`,
    `furi_hal_infrared.c`, `furi_hal_speaker.c`
  - `targets/f18/target.json` (inheritance example)
  - `lib/u8g2/u8g2_glue.c`, `applications/services/gui/canvas.c`
  - `scripts/fbt_tools/fbt_hwtarget.py`, `scripts/flash.py`,
    `site_scons/commandline.scons`
- u8g2: <https://github.com/olikraus/u8g2>

**Silicon**

- ST **DS11929 Rev 4**, *STM32WB55xx/STM32WB35xx datasheet*, Table 17
  "Alternate functions". Local extract used for every AF number in
  `hardware/PINOUT_AND_WIRING.md`.
- ST **RM0434**, *STM32WB55xx reference manual* (I²C, SPI, TIM, EXTI).

**Display**

- Sitronix **ST7567S** datasheet (command set, I²C control byte).
- David Albert, "GM12864-59N I2C LCD Display":
  <https://www.dalbert.net/gm12864-59n-i2c-lcd-display/>
- `luetee/ST7567S_128X64_I2C` (init sequence, address `0x3F`, control bytes):
  <https://github.com/luetee/ST7567S_128X64_I2C>
- `sskeledzija/lcd-st7567s-128x64-i2c` (same module on ESP32).

**Prior art**

- <https://github.com/GthiN89/FuckingCheapFlipperZero-DIY-Flipper-zero-The-real-on>
- <https://github.com/lamtranBKHN/diy_flipper_zero>
- <https://github.com/AJ60/Flipper_OLED_PCF8574_PN532>
- <https://github.com/cbwhickstein/Flipper_Zero_DIY>
- <https://github.com/enexis1337/DIY-Flipper-PCB>

**Companion**

- Pwnagotchi (maintained fork): <https://github.com/jayofelony/pwnagotchi>
- Pwnagotchi project: <https://pwnagotchi.org/>
- bettercap: <https://www.bettercap.org/>
- FlipperHTTP (Flipper ↔ ESP32 HTTP bridge precedent):
  <https://github.com/jblanked/FlipperHTTP>
- `pmaliagil1/flipper-ai` (AI chat over the WiFi dev board + FlipperHTTP).
