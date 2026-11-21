# `targets/f55` — Momentum board target for the WB55 custom clone

This directory is a **drop-in board target** for Momentum. It inherits
`targets/f7` and overrides only what this board changes:

| File | Why it is overridden |
| --- | --- |
| `target.json` | enables `"inherit": "7"`, excludes NFC/RFID/iButton, renames the board library to `flipper55` |
| `furi_hal_resources.[ch]` | new pin map: I²C display, nRF24 on the freed SPI2 pins, no NFC/RFID/iButton |
| `furi_hal_spi_config.[ch]` | SPI1 = CC1101, SPI2 = SD + nRF24, no display/NFC handles |
| `furi_hal_i2c_config.c` | I²C1 fixed at 400 kHz for the display (independent of OTP version) |
| `furi_hal_target_hw.h` | only Sub-GHz + IR HAL headers exist (no NFC/RFID/iButton) |
| `furi_hal_version_device.c` | custom model strings, accepts target 0/7/55 |

Everything else (clock tree, BLE, USB, storage, GUI, Sub-GHz, IR, power thread)
comes from `targets/f7` unchanged.

> **Test status: not built yet.** This is a reviewed scaffold, not a validated
> board port. Expect 1–3 build iterations (missing symbols, SDK CSV, module
> exclusions). §6 lists the likely failures and fixes.

---

## 1. Install into a Momentum checkout

```bash
git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware

# copy the board target
cp -r /path/to/momentum-wb55-custom-clone/firmware/target_f55 targets/f55

# allow TARGET_HW=55
#   site_scons/commandline.scons: allowed_values=["7","18"] -> ["7","18","55"]
```

Then apply the two GUI patches from
`../display_driver_patch/README.md` §2.1 and the contrast patch below.

### 1.1 Contrast patch (required)

`applications/services/notification/notification_app.c` calls the stock
driver's contrast helper, which is a no-op once the canvas uses the I²C
display:

```diff
-static void
-notification_apply_lcd_contrast(NotificationApp* app) {
-    Gui* gui = furi_record_open(RECORD_GUI);
-    u8x8_d_st756x_set_contrast(&gui->canvas->fb.u8x8, app->settings.contrast);
-    furi_record_close(RECORD_GUI);
-}
+static void
+notification_apply_lcd_contrast(NotificationApp* app) {
+    Gui* gui = furi_record_open(RECORD_GUI);
+    int value = ST7567_DEFAULT_CONTRAST + app->settings.contrast;
+    if(value < 0) value = 0;
+    if(value > 0x3F) value = 0x3F;
+    st7567_i2c_set_contrast((uint8_t)value);
+    furi_record_close(RECORD_GUI);
+}
```

with `#include <st7567_i2c.h>` added to the file. (If you prefer not to touch
the notification service, leave it: contrast stays at the driver default.)

## 2. Build

```bash
./fbt TARGET_HW=55            # build everything
./fbt TARGET_HW=55 --help     # list aliases (flash_usb, updater, ...)
```

The board library is named `flipper55` (see `target.json`), so the linker
expects `linker_dependencies` to contain `flipper55` — that is already set.
Artifacts appear under `build/f55/` (the build directory is derived from
`TARGET_HW`).

`target.json` reuses `../f7/api_symbols.csv` for the SDK symbol list. It is a
stopgap: API symbols differ slightly on this board (no NFC/RFID/display-SPI).
Regenerate properly once the firmware builds:

```bash
./fbt TARGET_HW=55 sdk   # or: ./fbt TARGET_HW=55 fapsdk
```

## 3. First-boot expectations

| Feature | Expected first-boot state |
| --- | --- |
| Display | works if the I²C driver is installed; contrast default `0x28` |
| Buttons | all six work; OK doubles as BOOT0 |
| microSD | mounts if wired per the pinout |
| Sub-GHz | CC1101 detected; TX/RX works with stock app |
| IR | TX/RX work (stock HAL, stock pins) |
| BLE | works after the radio stack is flashed (qFlipper) |
| NFC / RFID / iButton | apps excluded; any menu entry that reaches them should be absent |
| Battery | **shows 0 %** until the power layer is patched or a BQ27220 is fitted |
| nRF24 | invisible: no core app yet; driver + FAP in `firmware/nrf24/` |
| RGB LED | absent; LP5562 probes NACK harmlessly |
| Power off | does **not** cut power (no BQ25896); use a physical switch |

## 4. Flashing

For a blank STM32WB55:

1. **Radio stack** (Cortex-M0+): `./fbt TARGET_HW=55 flash_usb` normally installs
   the whole bundle via the Flipper bootloader/DFU. For a bare chip, flash the
   bootloader at `0x08000000`, firmware at `0x08008000`, and the stack at its
   load address ≤ `0x080E0000` (`scripts/flash.py` enforces this).
2. **OTP** (optional but recommended): see §5.
3. **Firmware**: qFlipper "Install from file" with the built `.dfu`, or
   `fbt flash_usb`.

## 5. OTP (irreversible — read twice)

Flipper firmware reads a board-identity structure at `0x1FFF7000` (magic
`0xBABE`). The community procedure for DIY boards:

| Field | Value | Why |
| --- | --- | --- |
| Version | 12 | `furi_hal_version_get_hw_version() > 10`; also selects the 400 kHz path in stock firmware |
| Firmware | 7 | stock-compatible target |
| Body | 9 | arbitrary/stock-like |
| Connection | 6 | arbitrary/stock-like |
| Display | MGG | selects the ST7567 contrast profile in stock code |
| Name | up to 8 chars | shown in `device_info` |

Write it with qFlipper's OTP utility or STM32CubeProgrammer at `0x1FFF7000`.
**This cannot be undone.** A blank OTP is accepted by this target (`target == 0`)
so you can defer OTP until the hardware works.

## 6. Likely build failures and fixes

| Symptom | Cause | Fix |
| --- | --- | --- |
| `undefined reference to furi_hal_spi_bus_handle_display` | `lib/u8g2/u8g2_glue.c` still linked (canvas/notification not patched) | apply both patches, or add a dummy handle + `gpio_display_*` pins |
| `undefined reference to gpio_display_rst_n` | same | same |
| `TARGET_HW` invalid | allowed list not edited | add `"55"` in `site_scons/commandline.scons` |
| Missing `flipper55` | `target.json` library rename not reflected in `linker_dependencies` | keep `flipper55` in the list |
| `furi_hal_nfc_*` unresolved | shared code still references NFC | extend `excluded_sources`/`excluded_headers`, or stub the symbols |
| OTP target mismatch warning | `do_i_belong_here` rejects the OTP | target 0/7/55 are accepted; adjust if you write another value |
| Boot loop after enabling BLE | radio stack not flashed | flash the stack, or build without BLE |

## 7. Deliberate gaps (roadmap, not bugs)

1. **nRF24 service/app** — driver in `firmware/nrf24/`; wire it into a FAP.
2. **Battery gauge** — see `NOTES_POWER_AND_LIGHT.md`.
3. **Power off / charger control** — needs a soft-latch or a charger IC.
4. **Contrast/brightness settings UI** — currently a compile-time default plus
   the notification offset.
5. **`api_symbols.csv` regeneration** after the target stabilises.
6. **48-pin variant** (`targets/f55c`) — not provided; see pinout §9.
