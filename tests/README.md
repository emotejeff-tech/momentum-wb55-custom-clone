# Host-side tests

These run on a PC, without an STM32, display or radios. They verify everything
that can be verified before hardware exists.

```bash
pip install pyserial ziglang     # ziglang is only needed for the C tests
python tests/run_all.py
```

| Test | What it proves |
| --- | --- |
| `bridge_firmware/pi_bridge/test_protocol.py` | COBS + CRC-16 framing, frame round-trips, corruption rejection, max payload |
| `bridge_firmware/protocol/test_protocol.c` | The **C** implementation of the same framing, compiled with `zig cc` |
| C↔Python interop (inside `run_all.py`) | The C test's emitted frame parses in Python and re-encodes byte-for-byte |
| `compile_smoke.py` | `st7567_i2c.c`, `u8g2_st7567_i2c.c` (against real u8g2 headers) and `nrf24.c` compile cleanly with `-Wall -Wextra -Werror` |
| `e2e_bridge_test.py` | Full bridge path: `socket://` serial transport, HELLO/ACK, `AI_REQ` → LLM tool call → `CMD status` → `CMD_RESULT` → second LLM call → `AI_CHUNK` + `AI_RESP`, with a fake STM32 and a fake OpenAI-compatible server |

## What is deliberately not tested

- **The Momentum target build.** `targets/f55` needs the ARM toolchain and the
  full Momentum tree; run `fbt TARGET_HW=55` for that. The host stubs in
  `host_stubs/` exist only to let the portable drivers compile — they do not
  replace the real STM32 LL headers, so the target files
  (`furi_hal_resources.c`, `furi_hal_spi_config.c`, `furi_hal_i2c_config.c`) are
  not compile-checked here.
- **Electrical behaviour and timing.** The 23 ms frame estimate, I²C pull-ups,
  nRF24 power integrity and IR range are hardware measurements (roadmap
  Phases 1–3).
- **Pwnagotchi plugin API compatibility.** The plugin guards its hooks with
  `try/except` and falls back to zeros; verify against your fork on the Pi.

## Stubs

`host_stubs/` contains minimal `furi.h`, `furi_hal.h`, `furi_hal_i2c.h`,
`furi_hal_spi.h` and `furi_hal_resources.h`. They are **test-only** and must
never be on the include path of a real firmware build.
