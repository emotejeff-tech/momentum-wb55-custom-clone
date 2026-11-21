"""Pwnagotchi plugin that publishes state and events for the WB55 bridge.

Install:
    cp pwnagotchi_plugin.py /usr/local/share/pwnagotchi/custom-plugins/wb55_bridge.py
    # /etc/pwnagotchi/config.toml
    # main.plugins.wb55_bridge.enabled = true
    # main.plugins.wb55_bridge.state_dir = "/run/wb55-bridge"

The plugin writes:
    <state_dir>/state.json   atomically replaced, read by bridge.py
    <state_dir>/events.fifo  newline-delimited JSON events

It never blocks the Pwnagotchi loop: writes are best-effort and exceptions are
swallowed (a missing bridge must not break Pwnagotchi).
"""

from __future__ import annotations

import json
import os
import time

DEFAULT_STATE_DIR = "/run/wb55-bridge"


class Plugin:
    """Pwnagotchi plugin API (duck-typed, version tolerant)."""

    def __init__(self, *args, **kwargs):  # noqa: D401, ANN002, ANN003
        self.options = {}
        # Pwnagotchi passes (agent, config) or (config) depending on version.
        for arg in args:
            if isinstance(arg, dict):
                self.options = arg.get("main", {}).get("plugins", {}).get("wb55_bridge", {})
                break
        self.state_dir = self.options.get("state_dir", DEFAULT_STATE_DIR)
        self.state_file = os.path.join(self.state_dir, "state.json")
        self.fifo_path = os.path.join(self.state_dir, "events.fifo")
        self._fifo = None
        self._ready = False

    # -- helpers ---------------------------------------------------------
    def _ensure_paths(self) -> None:
        os.makedirs(self.state_dir, exist_ok=True)
        if not os.path.exists(self.fifo_path):
            try:
                os.mkfifo(self.fifo_path, 0o600)
            except FileExistsError:
                pass

    def _write_state(self, payload: dict) -> None:
        try:
            self._ensure_paths()
            tmp = self.state_file + ".tmp"
            with open(tmp, "w", encoding="utf-8") as fh:
                json.dump(payload, fh)
            os.replace(tmp, self.state_file)
        except Exception:  # noqa: BLE001
            pass

    def _emit(self, event: dict) -> None:
        try:
            self._ensure_paths()
            if self._fifo is None:
                self._fifo = os.open(self.fifo_path, os.O_WRONLY | os.O_NONBLOCK)
            data = (json.dumps(event) + "\n").encode()
            os.write(self._fifo, data)
        except (OSError, BrokenPipeError):
            self._fifo = None
        except Exception:  # noqa: BLE001
            pass

    def _snapshot(self, agent) -> dict:
        session = {}
        try:
            session = agent.session() or {}
        except Exception:  # noqa: BLE001
            pass

        aps = session.get("access_points") or []
        handshakes = session.get("handshakes") or []
        peers = session.get("peers") or []

        return {
            "pwn": {
                "epoch": session.get("epoch", 0),
                "reward": session.get("reward", 0),
                "aps": len(aps),
                "hs": len(handshakes),
                "peers": len(peers),
                "mode": session.get("status", "unknown"),
                "uptime": int(time.time()),
            },
            "wifi": {"ssid": session.get("ssid", ""), "connected": True},
            "networks": [
                {"ssid": ap.get("hostname") or ap.get("ssid", ""),
                 "rssi": ap.get("rssi", 0),
                 "ch": ap.get("channel", 0)}
                for ap in aps[:5]
                if isinstance(ap, dict)
            ],
        }

    # -- hooks -----------------------------------------------------------
    def on_ready(self, agent) -> None:
        self._ready = True
        self._write_state(self._snapshot(agent))
        self._emit({"kind": "ready", "data": {"version": "0.1"}})

    def on_epoch(self, agent) -> None:
        self._write_state(self._snapshot(agent))

    def on_wifi_update(self, agent, networks) -> None:  # noqa: ANN001
        self._write_state(self._snapshot(agent))

    def on_handshake(self, agent, filename, access_point, client_station) -> None:  # noqa: ANN001
        ap = access_point if isinstance(access_point, dict) else {}
        self._emit(
            {
                "kind": "handshake",
                "data": {
                    "file": os.path.basename(str(filename)),
                    "ssid": ap.get("hostname") or ap.get("ssid", ""),
                    "bssid": ap.get("mac", ""),
                    "channel": ap.get("channel", 0),
                    "client": (client_station or {}).get("mac", ""),
                },
            }
        )
        self._write_state(self._snapshot(agent))

    def on_unload(self, agent) -> None:  # noqa: ANN001
        self._emit({"kind": "unload", "data": {}})
        if self._fifo is not None:
            try:
                os.close(self._fifo)
            except OSError:
                pass
            self._fifo = None
