# Hardware

| File | What it is |
| --- | --- |
| `PINOUT_AND_WIRING.md` | **Authoritative** pin/AF table, bus allocation, passives, wiring diagrams, bring-up order |
| `BOM.csv` | Bill of materials with reference designators and criticality |
| `netlist.csv` | Net-by-net connection list — the schematic *before* it is drawn |

## Schematic status

There is **no KiCad project yet**. The connection list in `netlist.csv` is the
source of truth for the first PCB revision; it is complete enough to draw a
schematic mechanically:

1. Create a KiCad project, add the sheets: `power`, `mcu`, `lcd`, `subghz`,
   `nrf24`, `ir`, `storage`, `ui`, `companion`.
2. Place the symbols from the BOM and import `netlist.csv` as a net list (or
   use it as a checklist while wiring).
3. Run ERC; every net in the CSV must exist exactly once.

## What the first PCB must get right

- **Antenna keep-out.** CC1101 and nRF24 modules are usually pre-certified
  breakouts with their own antennas. If you integrate the chips directly, copy
  the reference layout (matching network, ground plane cut) — do not improvise.
- **nRF24 supply.** 100 µF electrolytic + 100 nF ceramic within 5 mm of the
  module; a 10 Ω series resistor into the 3V3 pin with the caps on the module
  side is a common fix for PA/LNA brown-outs.
- **SD supply.** 10 µF + 100 nF at the socket; series 22 Ω on SCK/MOSI if the
  cable to the socket is long.
- **I²C pull-ups.** 4.7 kΩ to 3V3 on SCL/SDA, placed near the MCU; verify the
  LCD module does not already fit them (measure with the module unplugged).
- **Display backlight.** Hard-wired to 3V3 through R5 on the module. If you
  want dimming, plan the MOSFET cut now (see pinout §7.1).
- **BOOT0/OK.** `PH3` must have a 10 kΩ pull-down and must not be held high at
  power-on unless you intend to enter DFU.
- **USB.** 5.1 kΩ CC1/CC2 to GND, ESD array on D+/D−, VBUS fuse.
- **32 MHz crystal.** If using a bare WB55: load caps per the crystal's CL,
  guard ring, no traces under the crystal. If using a module (Nucleo/WeAct),
  this is already done.
- **Thermal pad.** VFQFPN68 has an exposed pad — via-stitch it to the ground
  plane; it is the only path for RF/thermal.

## Mechanical notes

- The GM12864-59N is 59 × 40 mm class with a metal frame and 4 corner holes;
  the 4-pin header is on the left edge (verify orientation on your batch).
- The FPC is delicate: strain-relieve it, do not bend it under the PCB.
- Keep the CC1101 and nRF24 antennas at opposite ends of the board and away
  from the SD socket and the Pi's Wi-Fi antenna.

## Netlist conventions

- `U1` = MCU, `U2` = LCD, `U3` = CC1101, `U4` = nRF24, `U5` = TSOP, `U6` = SD,
  `U7` = charger, `U8` = 5 V boost, `U9` = 3.3 V buck-boost, `U10`/`U11` =
  companion boards.
- `J1` = USB-C, `J2` = SWD, `J3` = companion UART, `J4` = Pi 5 V.
- Pin names on modules follow the silkscreen of the common breakouts; if your
  module labels differ (e.g. `SO/SI` vs `MISO/MOSI`), map by function, not by
  label order.
