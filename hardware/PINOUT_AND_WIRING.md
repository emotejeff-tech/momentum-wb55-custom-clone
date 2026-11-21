# Pin assignment and wiring

**This file is the authority for wiring.** Every alternate-function (AF) number
below was checked against **ST DS11929 Rev 4, Table 17 "Alternate functions"**
for the STM32WB55xx. Where a pin is used as plain GPIO, the AF column says
`GPIO`. Where a peripheral requires an AF, the number is given.

The design targets the **VFQFPN68** package (`STM32WB55RGT6`, the same package
as Flipper Zero) on a **NUCLEO-WB55RG** for bring-up. A 48-pin `WB55CG`
(WeAct) variant is documented in §9 — it cannot host this map.

> Terminology: "stock" = the pin used by Flipper Zero's `targets/f7` board
> layer. "Kept" means we deliberately reuse the stock pin so Momentum's HAL
> needs no change for that peripheral.

---

## 1. Base board and pin budget

| Board | Package | GPIOs available | Verdict |
| --- | --- | --- | --- |
| NUCLEO-WB55RG | VFQFPN68 | all PA/PB/PC0–PC6/PC10–PC15/PD0–PD1/PH0–PH3 | **Recommended.** Arduino + Morpho headers, ST-LINK, 32 MHz crystal, reset button. |
| WeAct STM32WB55CG "Black Pill" | UFQFPN48 | ~28 | Workable only with the reduced map (§9) + I²C button expander. |

The 68-pin package is required for: six discrete buttons, a dedicated SPI2 for
nRF24, SD card detect, and a battery-sense ADC pin.

---

## 2. Power and ground

| Net | Source | Consumers | Notes |
| --- | --- | --- | --- |
| `VBAT` | 1S Li-ion (3.0–4.2 V) | 3.3 V buck-boost, 5 V boost | Fuse + reverse-polarity protection recommended |
| `+3V3` | buck-boost ≥ 1 A | MCU, radios, LCD, SD | 100 nF per IC + 10 µF bulk per rail |
| `+5V` | boost ≥ 2 A | Pi Zero 2 W only | Do not back-feed USB |
| `VBUS` | USB-C 5 V | charger, optional bench power | 5.1 kΩ CC1/CC2 to GND for a UFP sink |
| `GND` | common | all | Single-point star at the battery negative |

MCU supply pins (VFQFPN68): `VDD` ×N, `VDDA`, `VREF+`, `VDDRF`, `VDDSMPS`,
`VLXSMPS`, `VFBSMPS`, `VBAT`, `VDDUSB` — follow the datasheet's SMPS/LDO
reference design. Decouple each `VDD` with 100 nF as close as possible; use
4.7 µF on `VDDA`/`VREF+`.

| Item | Connection | Why |
| --- | --- | --- |
| `NRST` | 10 kΩ to 3V3, 100 nF to GND, pushbutton to GND | standard reset |
| `BOOT0` = `PH3` | 10 kΩ to GND; button to 3V3 (this is the OK key) | BOOT0 sampled at reset; pressing OK while resetting enters the ROM bootloader (intended DFU shortcut) |
| `OSC_IN`/`OSC_OUT` | 32 MHz crystal + load caps per crystal CL, or TCXO into OSC_IN (HSE bypass) | radio will not start without it |
| `PC14`/`PC15` | optional 32.768 kHz crystal (LSE) | accurate RTC; otherwise LSI |
| `VBAT` | optional coin cell | RTC/backup only |
| `PA13`/`PA14` | SWDIO/SWCLK + GND + 3V3 on a 4-pin header | debug/flash |

**Battery sense (needed for a usable battery UI):**

```
VBAT ── 100 kΩ ──┬── 100 kΩ ── GND
                 │
                 ├── 100 nF ── GND
                 └── PC5  (ADC1_IN14)
```

Divider ratio 2:1. `PC5` is VFQFPN68 pin 26 and is **ADC1_IN14** (DS11929
§5, pin table). The stock firmware has no ADC battery path — this requires the
power-layer patch described in `firmware/target_f55/NOTES_POWER_AND_LIGHT.md`.

---

## 3. Master pin table

Legend: **Dir** = direction from the MCU. **Stock** = Flipper `targets/f7`
equivalent. All pins are 3.3 V logic; `FT` pins are 5 V tolerant but nothing
here needs 5 V.

### 3.1 Display — GM12864-59N (ST7567S, I²C)

| Signal | MCU pin | AF | Dir | Peripheral | Stock | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `LCD_SCL` | **PA9** | AF4 `I2C1_SCL` | out | I2C1 | `PA9` = power-I²C SCL (BQ25896/BQ27220/LP5562) | 4.7 kΩ pull-up to 3V3; 400 kHz |
| `LCD_SDA` | **PA10** | AF4 `I2C1_SDA` | I/O | I2C1 | `PA10` = power-I²C SDA | 4.7 kΩ pull-up; address `0x3F` |
| `LCD_3V3` | +3V3 | — | pwr | — | — | 100 nF + 4.7 µF at module |
| `LCD_GND` | GND | — | pwr | — | — | — |

The module has **no reset, no DC, no CS**. Reset is the software command
`0xE2`; the controller must be re-initialised after a power cycle. The stock
display pins (`PC11`, `PB1`, `PB0`, `PD1`, `PB15`, `PC2`) are freed and
reassigned to the nRF24 (§3.3).

> Alternative bus: the display can instead go on **I2C3** (`PC0` SCL /
> `PC1` SDA, AF4), which stock uses for the external I²C header. I²C1 is chosen
> because the power ICs it normally serves are absent, so no header
> functionality is lost.

### 3.2 Sub-GHz — CC1101 (SPI1)

| Signal | MCU pin | AF | Dir | Stock | Notes |
| --- | --- | --- | --- | --- | --- |
| `CC_SCK` | **PA5** | AF5 `SPI1_SCK` | out | `PA5` | shared SPI1 |
| `CC_MISO` | **PB4** | AF5 `SPI1_MISO` | in | `PB4` | |
| `CC_MOSI` | **PB5** | AF5 `SPI1_MOSI` | out | `PB5` | |
| `CC_CSN` | **PD0** | GPIO out | out | `PD0` | 10 kΩ pull-up; active low |
| `CC_GDO0` | **PA1** | GPIO in (also AF1 `TIM2_CH2`) | in | `PA1` | RX data / IRQ; optional timer capture |
| `CC_GDO2` | PB2 *(optional)* | GPIO in (AF1 `LPTIM1_OUT` available) | in | — | only if your module brings it out; EXTI2 free |
| `CC_RF_SW` | **PC4** *(optional)* | GPIO out | out | `PC4` | antenna switch control for modules that need it |

CC1101 is **3.3 V only**. 100 nF + 10 µF at the module; keep the SPI stub
< 5 cm. Most breakouts carry their own 26 MHz crystal and matching network.

### 3.3 2.4 GHz — nRF24L01+ (SPI2, on the freed display pins)

| Signal | MCU pin | AF | Dir | Stock | Notes |
| --- | --- | --- | --- | --- | --- |
| `NRF_SCK` | **PD1** | AF5 `SPI2_SCK` | out | display SCK | ≤ 8 MHz recommended (SD shares the bus) |
| `NRF_MOSI` | **PB15** | AF5 `SPI2_MOSI` | out | display MOSI | |
| `NRF_MISO` | **PC2** | AF5 `SPI2_MISO` | in | display MISO | |
| `NRF_CSN` | **PB0** | GPIO out | out | display RST | stock pull-up keeps CSN high (deselected) at reset |
| `NRF_CE` | **PC11** | GPIO out | out | display CS | 10 kΩ pull-down; must be driven low early |
| `NRF_IRQ` | **PB1** | GPIO in / **EXTI1** | in | display DC | active low; EXTI0 is taken by IR RX |

`PB0`/`PB1` share EXTI lines 0/1 with `PA0` (IR RX) and nothing else. `PB1` is
used as the interrupt; `PB0` is an output so its EXTI mapping is irrelevant.
`PC10` (SD card detect) is also on EXTI10 together with `PB10` (Up button), but
the SD driver polls it as a plain input — **do not** enable EXTI on `PC10`.

### 3.4 Storage — microSD (SPI2, shared with nRF24)

| Signal | MCU pin | AF | Dir | Stock | Notes |
| --- | --- | --- | --- | --- | --- |
| `SD_SCK` | **PD1** | AF5 `SPI2_SCK` | out | `PD1` | 16 MHz max in stock presets |
| `SD_MOSI` | **PB15** | AF5 `SPI2_MOSI` | out | `PB15` | 22–33 Ω series optional |
| `SD_MISO` | **PC2** | AF5 `SPI2_MISO` | in | `PC2` | 10 kΩ pull-up |
| `SD_CS` | **PC12** | GPIO out | out | `PC12` | 10 kΩ pull-up |
| `SD_CD` | **PC10** | GPIO in | in | `PC10` | card-detect switch to GND; 10 kΩ pull-up; polled |
| `SD_PWR_EN` | **PA3** *(optional)* | GPIO out (open-drain) | out | `PA3` | stock drives a load switch; tie SD power permanently on and leave PA3 high-Z if you omit it |

Use a **3.3 V microSD socket** (not a 5 V level-shifted Arduino module) to
avoid extra latency on the shared bus. Decouple with 10 µF + 100 nF.

### 3.5 Infrared

| Signal | MCU pin | AF | Dir | Stock | Notes |
| --- | --- | --- | --- | --- | --- |
| `IR_RX` | **PA0** | AF1 `TIM2_CH1` (input capture) | in | `PA0` | TSOP38238 output; 10 kΩ pull-up; active low |
| `IR_TX` | **PB9** | AF1 `TIM1_CH3N` | out | `PB9` | complementary output; drive through a transistor |

`PB9` is `TIM1_CH3N`, not `TIM1_CH3`. Flipper's IR HAL already configures
channel 3 with `LL_TIM_CHANNEL_CH3N` and `LL_TIM_EnableAllOutputs(TIM1)`, so
keeping the stock pin means zero IR driver changes.

### 3.6 User interface

| Signal | MCU pin | AF | Dir | Stock | Polarity / notes |
| --- | --- | --- | --- | --- | --- |
| `BTN_UP` | **PB10** | GPIO in | in | `PB10` | to GND, internal pull-up |
| `BTN_DOWN` | **PC6** | GPIO in | in | `PC6` | to GND, internal pull-up |
| `BTN_LEFT` | **PB11** | GPIO in | in | `PB11` | to GND, internal pull-up |
| `BTN_RIGHT` | **PB12** | GPIO in | in | `PB12` | to GND, internal pull-up |
| `BTN_OK` | **PH3** | GPIO in | in | `PH3` | **to 3V3**, external 10 kΩ pull-down (BOOT0) |
| `BTN_BACK` | **PC13** | GPIO in | in | `PC13` | to GND, internal pull-up; backup-domain pin, input only |
| `SPEAKER` | **PB8** *(optional)* | AF14 `TIM16_CH1` | out | `PB8` | piezo or amplifier; series 100 Ω |
| `VIBRO` | **PA8** *(optional)* | GPIO out | out | `PA8` | MOSFET + flyback diode |
| `BAT_SENSE` | **PC5** | analog `ADC1_IN14` | in | — | divider §2. No spare pin is allocated to a status LED; use the LCD or a Pi-side LED. |

`PH3` doubles as BOOT0: pressing OK during reset enters the ROM bootloader.
This is how you recover a bricked board without opening the case.

### 3.7 Companion link and host interfaces

| Signal | MCU pin | AF | Dir | Stock | Notes |
| --- | --- | --- | --- | --- | --- |
| `UART_TX` | **PB6** | AF7 `USART1_TX` | out | `PB6` | 921600 8N1, 3.3 V |
| `UART_RX` | **PB7** | AF7 `USART1_RX` | in | `PB7` | 3.3 V |
| `USB_DM` | **PA11** | AF10 `USB_DM` | I/O | `PA11` | 22 Ω series optional; ESD array recommended |
| `USB_DP` | **PA12** | AF10 `USB_DP` | I/O | `PA12` | |
| `SWDIO` | **PA13** | AF0 | I/O | `PA13` | |
| `SWCLK` | **PA14** | AF0 | in | `PA14` | |

USART1 is the stock external-header UART (pins 13/14) and is also used by
Flipper's serial apps/CLI. The bridge app acquires it at runtime; USB CDC
(PA11/PA12) remains the debug console. If you need the CLI on hardware UART,
run the bridge over USB CDC instead — the protocol is transport-agnostic.

---

## 4. Bus and resource allocation summary

| Bus | Pins | Devices | Speed | Notes |
| --- | --- | --- | --- | --- |
| SPI1 (AF5) | PA5/PB4/PB5 | CC1101 (CS PD0) | 8 MHz | stock bus; external-header SPI also lives here |
| SPI2 (AF5) | PD1/PB15/PC2 | nRF24 (CSN PB0), microSD (CS PC12) | 4–16 MHz | **was the display bus**; free now |
| I2C1 (AF4) | PA9/PA10 | GM12864-59N (0x3F) | 400 kHz | **was the power-IC bus**; those ICs are absent |
| I2C3 (AF4) | PC0/PC1 | external I²C header | 100 kHz | untouched; available for sensors |
| USART1 (AF7) | PB6/PB7 | Pi Zero 2 W / ESP32-S3 | 921600 | COBS + CRC framing |
| TIM1 | PB9 (CH3N) | IR TX | 38 kHz carrier | stock IR HAL |
| TIM2 | PA0 (CH1) | IR RX capture | — | stock IR HAL |
| TIM16 | PB8 (CH1) | speaker | — | optional |
| LPTIM2 | PA4 (OUT) | stock PWM API | — | external header |
| EXTI0 | PA0 | IR RX | — | |
| EXTI1 | PB1 | nRF24 IRQ | — | |
| EXTI3 | PH3 | OK button | — | |
| EXTI6 | PC6 | Down | — | |
| EXTI10/11/12 | PB10/PB11/PB12 | Up/Left/Right | — | |
| EXTI13 | PC13 | Back | — | |

### Conflict checks performed

- **SPI2 CS lines**: `PB0` (nRF24) and `PC12` (SD) are both plain GPIO,
  software NSS — no hardware NSS conflict.
- **I²C1 address space**: display `0x3F`; absent BQ25896 `0x6B`, BQ27220
  `0x55`, LP5562 `0x30`. No collision, and the absent devices NACK harmlessly.
- **USART1 vs I²C1**: `PA9/PA10` can be either I²C1 or USART1 (AF7). We use
  them as I²C1 and keep USART1 on `PB6/PB7`, so both peripherals coexist.
- **TIM2**: IR RX uses capture channels on `PA0`; CC1101 `GDO0` on `PA1` is
  `TIM2_CH2`-capable but is used as GPIO/EXTI. If you later want hardware
  capture for Sub-GHz, do not enable it while IR RX is active.
- **EXTI10**: `PC10` (SD CD) and `PB10` (Up) share line 10. SD CD is polled,
  never configured as EXTI. Do not change that.
- **BOOT0**: `PH3` must be low at reset; the OK button pulls it high only while
  pressed. A stuck OK button at power-on enters DFU — documented, not a bug.

---

## 5. Passives and protection

| Ref | Value | Placement | Purpose |
| --- | --- | --- | --- |
| R1, R2 | 4.7 kΩ | SCL/SDA → 3V3 | I²C pull-ups (add only if the LCD module lacks them) |
| R3 | 10 kΩ | `PB0` → 3V3 | nRF24 CSN idle high |
| R4 | 10 kΩ | `PC11` → GND | nRF24 CE idle low |
| R5 | 10 kΩ | `PB1` → 3V3 | nRF24 IRQ pull-up |
| R6 | 10 kΩ | `PC12` → 3V3 | SD CS idle high |
| R7 | 10 kΩ | `PC10` → 3V3 | SD card-detect pull-up |
| R8 | 10 kΩ | `PA0` → 3V3 | TSOP38238 output pull-up |
| R9 | 100 Ω + 4.7 µF | TSOP VCC | supply filter (datasheet recommendation) |
| R10 | 1 kΩ | `PB9` → NPN base | IR LED driver |
| R11 | 15–22 Ω | IR LED anode → 3V3 | LED current limit (≈ 80–130 mA) |
| R12, R13 | 100 kΩ | battery divider | 2:1 sense |
| C1–C8 | 100 nF | every IC VCC | decoupling |
| C9 | 10 µF | CC1101, nRF24, SD, LCD rails | bulk |
| C10 | 100 µF | nRF24 VCC | PA/LNA current spikes |
| D1 | Schottky | vibro motor | flyback |
| U1 | USBLC6-2SC6 | USB D+/D− | ESD protection |

---

## 6. Level shifting

**None is required** for the mandatory BOM: STM32WB55, GM12864-59N, CC1101,
nRF24L01+, TSOP38238, microSD and the Pi/ESP32 UART are all 3.3 V.

Cases that *do* need shifting/adaptation:

- 5 V Arduino microSD modules: they carry their own shifter, but add latency to
  a bus shared with the nRF24. Prefer a 3.3 V socket.
- 5 V IR receivers or 5 V sensors on the external header: use a level shifter
  or verify the pin is `FT` (5 V tolerant) **and** the peripheral is
  open-drain. Do not rely on `FT` for driven 5 V signals into the MCU.
- Powering the Pi Zero 2 W: 5 V only, from the boost converter — never from
  the 3.3 V rail.

---

## 7. Wiring details

### 7.1 Display

```
GM12864-59N (4-pin)                STM32WB55
  3V3  ──────────────── +3V3 (100 nF + 4.7 µF to GND)
  GND  ──────────────── GND
  SCL  ──────────────── PA9   (AF4 I2C1_SCL, 4.7 kΩ to 3V3)
  SDA  ──────────────── PA10  (AF4 I2C1_SDA, 4.7 kΩ to 3V3)
```

Contrast is set by the electronic-volume command (`0x81`, value 0x00–0x3F) and
the regulation ratio (`0x20`–`0x27`). Defaults in the driver: ratio `0x25`,
contrast `0x28`; expose both in settings for panel-batch variance.

**Backlight mod (optional):** the module's LED is hard-wired through R5
(100 Ω). Remove R5 and drive the LED string with a low-side N-MOSFET from a
spare PWM pin (e.g. `PA7` `TIM17_CH1` AF14, or `PA4` LPTIM2). Keep a 15–33 Ω
series resistor. Without the backlight the panel is unreadable — this is for
dimming and power saving, not for turning the display "off".

### 7.2 IR transmitter

```
PB9 (TIM1_CH3N) ── 1 kΩ ──┐
                          │  BC337 / 2N2222 (or 2N7002 gate)
                          │
+3V3 ── 15–22 Ω ── IR LED ── collector
                              emitter ── GND
```

Pulse current: `(3.3 − 1.35 − 0.2) / 18 ≈ 97 mA`. For longer range use a
logic-level MOSFET and 10 Ω, and keep duty cycle low (38 kHz, ~30 % carrier).
The stock IR HAL drives `PB9` as `TIM1_CH3N`; no code change is needed.

### 7.3 IR receiver

```
+3V3 ── 100 Ω ──┬── TSOP38238 VCC
                └── 4.7 µF ── GND
TSOP OUT ── PA0 (TIM2_CH1, 10 kΩ pull-up)
TSOP GND ── GND
```

### 7.4 microSD

```
SD socket (3.3 V)          STM32WB55
  VDD ── +3V3 (10 µF + 100 nF)
  GND ── GND
  SCK ── PD1   (AF5 SPI2_SCK)
  MOSI ─ PB15  (AF5 SPI2_MOSI)
  MISO ─ PC2   (AF5 SPI2_MISO)
  CS   ─ PC12  (GPIO, 10 kΩ pull-up)
  CD   ─ PC10  (GPIO, switch to GND, 10 kΩ pull-up)
```

### 7.5 Buttons

```
Up/Down/Left/Right/Back:  pin ── switch ── GND
OK:                       PH3 ── switch ── +3V3   (10 kΩ pull-down to GND)
```

Add 100 nF across each switch only if you see bounce in `input_test`; the
firmware already debounces (`INPUT_DEBOUNCE_TICKS`).

### 7.6 Companion UART

```
STM32 PB6 (TX) ──→ Pi GPIO15 / RXD   (ESP32: RX)
STM32 PB7 (RX) ←── Pi GPIO14 / TXD   (ESP32: TX)
GND ───────────── GND                (always common)
```

Do **not** connect the Pi's 5 V to the STM32 board's 5 V rail unless you have
designed the power path; power the Pi from its own boost rail.

On Raspberry Pi OS:

```ini
# /boot/firmware/config.txt
enable_uart=1
dtoverlay=disable-bt
# /boot/firmware/cmdline.txt: remove "console=serial0,115200"
```

Then use `/dev/ttyAMA0` at 921600. On the ESP32-S3 use any UART (e.g. UART1 on
GPIO17/18) at the same baud.

---

## 8. Bring-up order (electrical)

1. Power only: verify 3V3 and 5V rails, current draw, no shorts.
2. SWD connect + `STM32CubeProgrammer` read out; check 32 MHz HSE starts.
3. I²C scan (`i2cdetect`-style app or a 5-line test): expect `0x3F` only.
4. Display test pattern; measure full-frame time.
5. SPI1: read CC1101 `PARTNUM`/`VERSION` registers (`0x00`/`0x31` → `0x00`/`0x14`).
6. SPI2: SD init + `f_mount`; then nRF24 `CONFIG` readback (`0x08`).
7. Buttons: all six edges, including OK-at-reset → DFU.
8. IR: loopback TX→RX with a 38 kHz burst.
9. UART: echo between STM32 and companion at 921600, then framed protocol.

---

## 9. Reduced variant: WeAct WB55CG (UFQFPN48)

Only if you must use the 48-pin module. From DS11929's UFQFPN48 column, these
stock pins **do not exist**: `PB10`–`PB15`, `PC0`–`PC6`, `PC10`–`PC12`,
`PD0`–`PD1`, `PC13`. The 48-pin part exposes roughly PA0–PA15, PB0–PB9,
PC14/PC15, PH0/PH1/PH3.

Minimum-change mapping:

| Function | 48-pin mapping | Consequence |
| --- | --- | --- |
| Display I²C | `PA9`/`PA10` (unchanged) | OK |
| Buttons | **MCP23017** on I²C1 (`0x20`), INT → `PB1` | input driver patch; proven by `lamtranBKHN/diy_flipper_zero` |
| CC1101 + SD + nRF24 | all on SPI1 (`PB3/PB4/PB5`), CS: CC `PA15`, SD `PA4`, nRF24 `PB0`; nRF24 CE `PB2`, IRQ `PA3` | three devices share one bus; slower SD; careful CS arbitration |
| IR RX / TX | `PA0` / `PA8` (`TIM1_CH1`) | IR TX pin change in HAL |
| Companion UART | `PB6`/`PB7` | OK |
| Battery sense | `PA6` (`ADC1_IN11`) | divider moves |
| Speaker | `PB8` | OK |
| Vibro | `PB9` | OK |

This variant is a separate target (`targets/f55c`) and is **not** what the rest
of this repository assumes. Do not mix the two tables.
