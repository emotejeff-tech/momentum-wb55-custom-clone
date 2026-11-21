# Build roadmap

Ordered so that each phase produces something testable and does not depend on
the next one. Times assume one person, evenings, parts already in hand.

---

## Phase 0 — Bench power and debug (½ day)

| Step | Detail | Pass |
| --- | --- | --- |
| 0.1 | Power the Nucleo from USB only; measure 3V3/5V rails | no short, < 100 mA idle |
| 0.2 | Connect ST-LINK; read out chip ID and flash size | `STM32CubeProgrammer` connects |
| 0.3 | Verify 32 MHz HSE starts (MCO or clock config) | no `furi_crash("Slow HSE/PLL startup")` |
| 0.4 | Flash a stock Momentum build for `TARGET_HW=7` | device boots to stock UI on **SPI** display (if one is attached) |

*If you have no SPI display, skip 0.4 and go straight to Phase 1 with the I²C
panel.*

---

## Phase 1 — Display (1–2 days)

| Step | Detail | Pass |
| --- | --- | --- |
| 1.1 | Wire GM12864-59N: 3V3/GND/PA9/PA10 + 4.7 kΩ pull-ups | I²C scan finds `0x3F` |
| 1.2 | Compile `st7567_i2c.c` into a minimal test app | `st7567_i2c_probe()` true |
| 1.3 | Run `fill(0xFF)`, `fill(0x00)`, `fill(0xAA)` | uniform white, black, checkerboard |
| 1.4 | Integrate `u8g2_st7567_i2c` into canvas (patch §2.1) | Momentum UI renders |
| 1.5 | Measure full-frame time (scope on SDA or GPIO toggle) | ≤ 30 ms at 400 kHz |
| 1.6 | Tune contrast/regulation for your panel batch | readable, no ghosting |

**Deliverable:** a Momentum build that boots and draws on the I²C panel.

---

## Phase 2 — Stock peripherals on the new map (2–3 days)

Wire one at a time, test with the stock app before touching firmware:

| Step | Peripheral | Test | Pass |
| --- | --- | --- | --- |
| 2.1 | Buttons | `input_test` app | all six keys, correct direction |
| 2.2 | microSD | `storage` / file browser | mount, read, write |
| 2.3 | CC1101 | Sub-GHz "Read" | RSSI changes, known remote decodes |
| 2.4 | IR RX | Infrared "Learn" | TSOP decodes a TV remote |
| 2.5 | IR TX | Infrared "Universal" | TV responds at 1 m |
| 2.6 | Speaker (optional) | notification sound | audible |
| 2.7 | Battery divider | multimeter vs firmware ADC | ±3 % |

Do this with a **patched stock target** (`targets/f7` pin edits) before creating
`targets/f55`; it isolates wiring bugs from target-build bugs.

---

## Phase 3 — Custom target and nRF24 (3–5 days)

| Step | Detail | Pass |
| --- | --- | --- |
| 3.1 | Copy `targets/f55`, add `TARGET_HW=55` | `fbt TARGET_HW=55` configures |
| 3.2 | Fix build errors per `target_f55/README.md` §6 | firmware links |
| 3.3 | Flash and boot | UI on I²C panel, no hard fault |
| 3.4 | Re-run Phase 2 tests on `f55` | all pass |
| 3.5 | Add `nrf24.c` to a test app | `nrf24_init()` true, register round-trip |
| 3.6 | Two-module link test | payload received, `TX_DS` set |
| 3.7 | Optional: nRF24 FAP UI | scan/channel view |

At the end of this phase the device is a working Momentum handheld with a
second radio.

---

## Phase 4 — Companion link (1–2 days)

| Step | Detail | Pass |
| --- | --- | --- |
| 4.1 | UART echo STM32↔Pi at 921600 | 0 errors over 1 MB |
| 4.2 | Run `bridge.py` against a loopback | HELLO/HEARTBEAT/ACK |
| 4.3 | Implement STM32 `bridge` app (or reuse serial CLI) | HELLO exchanged |
| 4.4 | `CMD status` round-trip | JSON result < 50 ms |
| 4.5 | AI_REQ round-trip (Pi + LLM) | text on device display |
| 4.6 | Pwnagotchi plugin | epoch/AP/handshake telemetry visible |
| 4.7 | Yank the UART mid-request | device stays up, link recovers |

---

## Phase 5 — Power and enclosure (1 week)

| Step | Detail | Pass |
| --- | --- | --- |
| 5.1 | Li-ion + charger + boost/buck | 3.3 V and 5 V stable under load |
| 5.2 | Battery sense patch or BQ27220 module | percentage tracks discharge |
| 5.3 | Soft-latch power switch (optional) | power-off works |
| 5.4 | Carrier PCB (KiCad) from `hardware/netlist.csv` | DRC clean, boards ordered |
| 5.5 | Enclosure (3D print) | buttons/display/SD accessible |
| 5.6 | 8-hour soak | no resets, no display lockups |

---

## Parallel tracks

- **Documentation:** keep `hardware/PINOUT_AND_WIRING.md` updated the moment a
  wire moves. Every mismatch costs a debugging session later.
- **Firmware patches:** keep `firmware/display_driver_patch/` and
  `firmware/target_f55/` as the only diffs against Momentum; rebase them on each
  Momentum release rather than forking the tree.
- **Companion:** the ESP32-S3 variant can be developed entirely on the bench
  against a USB-serial loopback before the STM32 app exists.

---

## Risk checklist before each phase

- [ ] No 5 V on any 3.3 V-only pin (CC1101, nRF24, SD, LCD).
- [ ] Common ground between STM32 and companion.
- [ ] nRF24 bulk capacitor fitted (100 µF) before any TX test.
- [ ] IR LED current limited (never drive from the pin directly).
- [ ] OTP not written until the board is known-good.
- [ ] RF transmissions into a dummy load / shielded setup during bring-up.
