# Power, battery and LED behaviour on this board

The stock Flipper power layer assumes two I²C devices that this board does not
have:

| Device | 7-bit addr | Function in stock firmware |
| --- | --- | --- |
| BQ25896 | `0x6B` | charger, OTG boost, power-off |
| BQ27220 | `0x55` | fuel gauge: voltage, current, SoC, temperature |
| LP5562 | `0x30` | RGB LED driver |

All three sit on **I²C1 (PA9/PA10)** — the bus this board reuses for the
display (`0x3F`). Their absence does **not** stop the firmware from booting:
`furi_hal_power_init()` stores `charger_ok = false` / `gauge_ok = false` when
the probes fail, and the display is unaffected because the addresses differ.

Two consequences remain:

1. **Battery UI reads 0 % / 0 V.** `furi_hal_power_get_battery_voltage()` and
   `furi_hal_power_get_pct()` consult the gauge unconditionally.
2. **Power-off does nothing.** `furi_hal_power_off()` asks the charger to cut
   the rail; with no charger, the MCU keeps running.

There are two ways to fix (1). Pick one.

---

## Option A — fit a BQ27220 module (no firmware changes)

A BQ27220 breakout wired to the same I²C1 bus (`0x55`) makes the stock gauge
path work as-is: `bq27220_init()` succeeds, `gauge_ok` becomes true, and the
status bar shows real percentages. Requirements:

- the module must have a proper sense resistor and be configured for your cell
  capacity (stock firmware writes a 2100 mAh CEDV profile);
- it must be on I²C1, not I²C3;
- the display (`0x3F`) and the gauge (`0x55`) coexist on the bus without
  conflict.

Charging stays independent: use a TP4056/TP5000 module with protection. This is
the least invasive option and is what the BOM's optional line suggests.

## Option B — ADC divider + a small power patch

If you do not want a gauge, wire the divider from
`hardware/PINOUT_AND_WIRING.md` §2 (`VBAT → 100k/100k → PC5`, 100 nF to GND) and
patch `targets/f7/furi_hal/furi_hal_power.c`:

```c
#include <furi_hal_adc.h>

/* f55: battery sense on PC5 = ADC1_IN14, 2:1 divider.
 * Only used when the BQ27220 is absent, so f7 behaviour is unchanged. */
static float furi_hal_power_board_battery_voltage(void) {
    FuriHalAdcHandle* adc = furi_hal_adc_acquire();
    furi_hal_adc_configure_ex(
        adc,
        FuriHalAdcScale2500,      /* divider gives up to 2.1 V at 4.2 V */
        FuriHalAdcClockSync64,
        FuriHalAdcOversample64,
        FuriHalAdcSamplingtime247_5);
    uint16_t raw = furi_hal_adc_read(adc, FuriHalAdcChannel14);
    float mv = furi_hal_adc_convert_to_voltage(adc, raw);
    furi_hal_adc_release(adc);
    return (mv * 2.0f) / 1000.0f; /* volts */
}
```

Then:

```c
float furi_hal_power_get_battery_voltage(FuriHalPowerIC ic) {
    if(!furi_hal_power.gauge_ok) {
        UNUSED(ic);
        return furi_hal_power_board_battery_voltage();
    }
    /* ... existing BQ25896/BQ27220 code ... */
}

uint8_t furi_hal_power_get_pct(void) {
    if(!furi_hal_power.gauge_ok) {
        static const struct { float v; uint8_t pct; } curve[] = {
            {4.20f, 100}, {4.05f, 90}, {3.95f, 75}, {3.85f, 60},
            {3.75f, 40},  {3.65f, 25}, {3.55f, 10}, {3.40f, 3}, {3.30f, 0},
        };
        float v = furi_hal_power_board_battery_voltage();
        if(v >= curve[0].v) return 100;
        for(size_t i = 1; i < COUNT_OF(curve); i++) {
            if(v >= curve[i].v) {
                float span = curve[i - 1].v - curve[i].v;
                float k = (v - curve[i].v) / span;
                return (uint8_t)(curve[i].pct + k * (curve[i - 1].pct - curve[i].pct));
            }
        }
        return 0;
    }
    /* ... existing gauge code ... */
}
```

`furi_hal_adc_init()` already runs before `furi_hal_power_init()` in
`furi_hal_init()`, so no init-order change is needed.

**Calibration:** measure `VBAT` with a multimeter, read the raw ADC value, and
adjust the `2.0f` divider factor. Keep the divider Thevenin impedance below
10 kΩ (100k/100k = 50 kΩ is too high for the ADC's 247.5-cycle sampling —
either accept ~2 % error or buffer it with an op-amp / use 20k/20k). If you use
20k/20k, add 1 µF to GND and expect ~100 µA drain.

## Power-off (both options)

Add a soft-latch if you want real power-off:

```
BAT+ ── P-MOSFET (e.g. AO3401) ── +3V3/+5V rails
                 gate ── N-MOSFET ── PWR_HOLD (PB13, free on VFQFPN68)
                         │
                 power button ── BAT+ (momentary to gate)
```

Firmware: drive `PWR_HOLD` high early in `furi_hal_init_early()` (before the
button is released), and drive it low in the shutdown path instead of calling
`furi_hal_power_off()`. This is board-specific code; it is not in the scaffold
yet.

## RGB LED

No LP5562 is fitted. `furi_hal_light_init()` writes to `0x30` and gets NACKs;
this costs a few hundred microseconds at boot and is otherwise harmless. If the
noise bothers you, override `furi_hal_light.c` in the target with a stub that
keeps the API but does nothing. Do **not** point the light API at the display
bus by accident: `0x30` ≠ `0x3F`, but a bug in a stub could still corrupt a
transaction.

If you want a status LED, drive it from a free pin through a resistor and
implement it as a board-specific notification (there is no stock mapping for
it). The LCD already provides all the feedback the UI needs.
