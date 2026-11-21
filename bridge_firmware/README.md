# Companion bridge firmware

Two reference implementations of the [bridge protocol](../architecture/AI_AND_PWNAGOTCHI_BRIDGE.md):

| Directory | Target | Role | Status |
| --- | --- | --- | --- |
| `esp32s3_bridge/` | ESP32-S3 (PlatformIO/Arduino) | Wi-Fi scan telemetry + HTTPS AI answers | compiles after `include/secrets.h`; AI path untested on hardware |
| `pi_bridge/` | Pi Zero 2 W (Python 3) | Pwnagotchi telemetry + AI agent with tool calls | protocol path implemented; Pwnagotchi hooks tolerant to fork differences |
| `protocol/bridge_protocol.h` | C/C++ | shared frame layout, CRC, COBS, build/parse helpers | reference implementation |

## Wiring

| STM32 | Companion |
| --- | --- |
| `PB6` USART1_TX | RX |
| `PB7` USART1_RX | TX |
| GND | GND |

3.3 V logic on both sides; 921600 8N1; no flow control. Do not power the Pi
from the STM32 board's 3.3 V rail.

## ESP32-S3 quick start

```bash
cd esp32s3_bridge
cp include/secrets.h.example include/secrets.h   # fill in Wi-Fi + API key
pio run -e esp32s3 -t upload
pio device monitor
```

The pins default to GPIO17 (TX) / GPIO18 (RX); change `kUartTxPin`/`kUartRxPin`
in `src/main.cpp` if your board uses others.

## Pi Zero 2 W quick start

```bash
# on the Pi
sudo apt install python3-serial python3-requests
sudo mkdir -p /opt/wb55-bridge /run/wb55-bridge
sudo cp bridge.py pwnagotchi_plugin.py /opt/wb55-bridge/
sudo cp wb55-bridge.service /etc/systemd/system/

# API key, not in the unit file
echo 'WB55_LLM_API_KEY=sk-...' | sudo tee /etc/wb55-bridge.env
sudo chmod 600 /etc/wb55-bridge.env

sudo systemctl daemon-reload
sudo systemctl enable --now wb55-bridge
```

Enable the Pwnagotchi plugin:

```toml
# /etc/pwnagotchi/config.toml
main.plugins.wb55_bridge.enabled = true
main.plugins.wb55_bridge.state_dir = "/run/wb55-bridge"
```

UART setup for the Pi Zero 2 W:

```ini
# /boot/firmware/config.txt
enable_uart=1
dtoverlay=disable-bt
```

Remove `console=serial0,115200` from `/boot/firmware/cmdline.txt`, reboot, and
use `/dev/ttyAMA0` (the PL011). The mini-UART is not reliable at 921600.

## Arming transmissions

`ir_tx`, `subghz_tx` and `nrf24_tx` are refused unless **both**:

1. `WB55_ALLOW_TX=1` in the bridge environment, and
2. `<state_dir>/armed` exists (the device/UI is expected to create it after a
   physical confirmation).

This is a deliberate two-key gate so a prompt-injected model cannot key a
transmitter. The STM32 side must enforce the same rule; never rely on the
companion alone.

## What is not implemented

- Protobuf payloads (JSON is the v1 choice; `bridge_protocol.h` is payload
  agnostic).
- BLE serial transport (USART1 and USB CDC are enough for bring-up).
- STM32-side `bridge` app — the protocol is specified and the companion halves
  are runnable, but the device-side dispatcher is still a task in
  `docs/BUILD_ROADMAP.md`.
- `subghz_tx`/`nrf24_tx` on the device side: the ops are allow-listed in the
  protocol but return `unsupported` until the STM32 app implements them.

## Testing without hardware

`python3 - <<'PY'` style loopback:

```python
import bridge, serial
# build a frame, parse it back, check CRC
f = bridge.build_frame(bridge.T_CMD, 1, bridge.F_ACK_REQ, b'{"id":"x","op":"status"}')
assert bridge.parse_frame(f[:-1])["type"] == bridge.T_CMD
```

For a full loop, run `socat -d -d pty,raw,echo=0 pty,raw,echo=0` and point both
`bridge.py` and a small test script at the two PTYs.
