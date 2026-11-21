# nRF24L01+ driver (SPI2)

A small, dependency-light driver for the nRF24L01+ on this board's second SPI
bus. It is deliberately **not** a Flipper app: first get the radio working, then
expose it to the UI.

## Where it sits in the bus topology

```
STM32WB55                nRF24L01+
PD1  ── SCK  ───────────  SCK
PB15 ── MOSI ───────────  MOSI
PC2  ── MISO ───────────  MISO
PB0  ── CSN  ───────────  CSN   (active low, 10k pull-up)
PC11 ── CE   ───────────  CE    (10k pull-down)
PB1  ── IRQ  ───────────  IRQ   (active low, EXTI1)
```

CSN is owned by `furi_hal_spi_bus_handle_nrf24`, so every helper brackets its
transfer with `furi_hal_spi_acquire()` / `furi_hal_spi_release()`. The SD card
shares the bus and is protected by the same Furi mutex, so a filesystem write
cannot interleave with a radio burst.

## Build integration

### Option 1 — compile into the firmware (fastest path)

```python
# applications/services/nrf24/application.fam (add your app/service)
App(
    appid="nrf24_service",
    name="[NRF24]",
    apptype=FlipperAppType.SERVICE,
    entry_point="nrf24_service_app",
    requires=["gui"],
    sources=["nrf24.c", "nrf24_service.c"],
    cdefines=["APP_NRF24"],
)
```

Copy `nrf24.c/.h` next to your service source, or add
`firmware/nrf24` to `include_paths` in `targets/f55/target.json`.

### Option 2 — FAP

FAPs can only call symbols exported by the firmware SDK. This driver is not in
`api_symbols.csv`, so a FAP either:

- vendors `nrf24.c` into the FAP and uses the exported
  `furi_hal_spi_bus_handle_nrf24` handle (add it to the SDK export list), or
- talks to a compiled-in service over a record/RPC.

Option 1 is recommended until the API stabilises.

## Minimal usage

```c
#include "nrf24.h"

if(!nrf24_init()) {
    FURI_LOG_E("APP", "nRF24 not found");
    return;
}

/* 2.4 GHz channel 76 = 2476 MHz, 1 Mbps, 0 dBm */
nrf24_set_channel(76);
nrf24_set_rate(Nrf24Rate1M);
nrf24_set_pa(Nrf24Pa0dBm);

/* pipe 0 listens on the same 5-byte address we transmit to */
static const uint8_t addr[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
nrf24_set_tx_address(addr, sizeof(addr));
nrf24_set_rx_address(0, addr, sizeof(addr));
nrf24_set_payload_width(0, 8);

nrf24_power_up(true);
if(nrf24_tx((const uint8_t*)"hello!!!", 8)) {
    /* TX_DS received */
}
```

Receiving:

```c
nrf24_rx_mode();                 /* CE high, PRIM_RX = 1 */
if(nrf24_rx_available()) {
    uint8_t buf[8];
    nrf24_rx_read(buf, sizeof(buf));
}
```

## Bring-up tests

1. **Presence:** `nrf24_init()` reads `CONFIG` (reset value `0x08`) and writes
   it. A read of `0x00`/`0xFF` means wiring, power or CSN is wrong.
2. **Register round-trip:** write `RF_CH = 76`, read it back.
3. **Carrier:** `nrf24_test_carrier(76)` emits an unmodulated carrier — visible
   as a peak at 2476 MHz on any spectrum analyzer, or as
   `nrf24_read_rpd() == 1` on a second nRF24 tuned to the same channel.
4. **Link:** two modules, one `nrf24_tx()`, one polling `nrf24_rx_available()`.

## Notes and limits

- **Power:** the PA/LNA variants draw ~115 mA peaks. 100 µF + 100 nF at the
  module is not optional; brown-outs look like "random register corruption".
- **5 V modules:** the common "nRF24L01+ 5 V adapter" boards contain a 3.3 V
  regulator and level shifters. They work, but add latency; a bare 3.3 V module
  is preferable.
- **IRQ:** the driver polls; `PB1`/EXTI1 is wired for an interrupt-driven RX
  path but not implemented yet.
- **Coexistence:** nRF24 and SD both use SPI2 at different speeds. Furi HAL
  re-initialises the SPI peripheral on each acquire, so this is safe but adds
  ~10 µs per transaction. Do not hold the bus across a `furi_delay`.
- **Legal:** transmitting on 2.4 GHz is regulated. The driver defaults to
  receive/standby; test carriers only into a dummy load or shielded setup.
