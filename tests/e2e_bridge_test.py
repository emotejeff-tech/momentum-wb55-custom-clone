#!/usr/bin/env python3
"""End-to-end test of the Pi bridge + protocol against a simulated STM32.

What it exercises:
  1. pyserial socket:// transport (same framing as a real UART)
  2. HELLO/ACK/HEARTBEAT
  3. AI_REQ -> tool call -> CMD -> CMD_RESULT -> second LLM call -> AI_CHUNK + AI_RESP
  4. The fake STM32 sees a genuine CMD with an allow-listed op.

Run: python3 tests/e2e_bridge_test.py
Requires: pyserial (pip install pyserial). No hardware, no network, no API key.
"""

from __future__ import annotations

import json
import os
import socket
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PI = os.path.join(ROOT, "bridge_firmware", "pi_bridge")
sys.path.insert(0, PI)
import bridge as proto  # noqa: E402

FINAL_TEXT = "Battery is 87 percent."


# ---------------------------------------------------------------------------
# Fake LLM (OpenAI-compatible)
# ---------------------------------------------------------------------------


class LLMHandler(BaseHTTPRequestHandler):
    calls = 0
    saw_tool_result = False

    def log_message(self, *args):  # silence the default logger
        return

    def do_POST(self):  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        body = json.loads(self.rfile.read(length) or b"{}")
        LLMHandler.calls += 1

        for message in body.get("messages", []):
            if message.get("role") == "tool":
                LLMHandler.saw_tool_result = True

        if LLMHandler.calls == 1:
            reply = {
                "choices": [
                    {
                        "message": {
                            "role": "assistant",
                            "content": None,
                            "tool_calls": [
                                {
                                    "id": "call_1",
                                    "type": "function",
                                    "function": {"name": "device_status", "arguments": "{}"},
                                }
                            ],
                        }
                    }
                ]
            }
        else:
            reply = {
                "choices": [{"message": {"role": "assistant", "content": FINAL_TEXT}}]
            }

        data = json.dumps(reply).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


# ---------------------------------------------------------------------------
# Fake STM32 on a TCP socket (pyserial socket:// transport)
# ---------------------------------------------------------------------------


class SimDevice:
    def __init__(self, server: socket.socket):
        self.server = server
        self.seq = 0
        self.cmds: list[dict] = []
        self.ai_chunks: list[str] = []
        self.ai_text: str | None = None
        self.error: str | None = None
        self.done = threading.Event()

    def send(self, ftype: int, payload: dict | None, flags: int = 0) -> None:
        self.seq = (self.seq + 1) & 0xFF
        body = json.dumps(payload, separators=(",", ":")).encode() if payload is not None else b""
        self.conn.sendall(proto.build_frame(ftype, self.seq, flags, body))

    def run(self) -> None:
        try:
            self.conn, _ = self.server.accept()
            self.conn.settimeout(30)
            buf = bytearray()
            time.sleep(1.0)  # let the bridge send HELLO
            self.send(proto.T_AI_REQ, {"id": "a1", "prompt": "what is the battery?"}, proto.F_ACK_REQ)

            deadline = time.monotonic() + 25
            while time.monotonic() < deadline and not self.done.is_set():
                try:
                    chunk = self.conn.recv(4096)
                except socket.timeout:
                    break
                if not chunk:
                    break
                for byte in chunk:
                    if byte == 0:
                        if not buf:
                            continue
                        frame = proto.parse_frame(bytes(buf))
                        buf.clear()
                        if frame:
                            self.handle(frame)
                    else:
                        buf.append(byte)
        except Exception as exc:  # noqa: BLE001
            self.error = repr(exc)
        finally:
            self.done.set()

    def handle(self, frame: dict) -> None:
        if frame["flags"] & proto.F_ACK_REQ:
            self.send(proto.T_ACK, None, proto.F_RESPONSE)
        if frame["type"] == proto.T_CMD:
            cmd = json.loads(frame["payload"])
            self.cmds.append(cmd)
            result = {"battery": 87, "voltage": 3.92, "uptime": 123}
            self.send(
                proto.T_CMD_RESULT,
                {"id": cmd.get("id"), "ok": True, "result": result},
                proto.F_RESPONSE,
            )
        elif frame["type"] == proto.T_AI_CHUNK:
            payload = json.loads(frame["payload"])
            self.ai_chunks.append(payload.get("delta", ""))
        elif frame["type"] == proto.T_AI_RESP:
            payload = json.loads(frame["payload"])
            self.ai_text = payload.get("text")
            self.done.set()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> int:
    # LLM server
    llm = HTTPServer(("127.0.0.1", 0), LLMHandler)
    threading.Thread(target=llm.serve_forever, daemon=True).start()
    llm_port = llm.server_address[1]

    # Simulated device
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", 0))
    server.listen(1)
    dev_port = server.getsockname()[1]

    sim = SimDevice(server)
    threading.Thread(target=sim.run, daemon=True).start()

    state_dir = os.path.join(ROOT, ".test-state")
    os.makedirs(state_dir, exist_ok=True)

    env = os.environ.copy()
    env.update(
        {
            "WB55_PORT": f"socket://127.0.0.1:{dev_port}",
            "WB55_BAUD": "921600",
            "WB55_LLM_BASE_URL": f"http://127.0.0.1:{llm_port}/v1",
            "WB55_LLM_API_KEY": "test-key",
            "WB55_MODEL": "test-model",
            "WB55_STATE_FILE": os.path.join(state_dir, "state.json"),
            "WB55_EVENT_FIFO": os.path.join(state_dir, "events.fifo"),
            "WB55_ARMED_FILE": os.path.join(state_dir, "armed"),
            "WB55_ALLOW_TX": "0",
        }
    )

    bridge = subprocess.Popen(
        [sys.executable, os.path.join(PI, "bridge.py")],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    passed = False
    try:
        if not sim.done.wait(timeout=35):
            print("FAIL: simulated device timed out waiting for AI_RESP")
        else:
            checks = {
                "ai_text": sim.ai_text == FINAL_TEXT,
                "ai_chunks_reconstruct": "".join(sim.ai_chunks) == FINAL_TEXT,
                "cmd_executed": any(c.get("op") == "status" for c in sim.cmds),
                "llm_calls": LLMHandler.calls == 2,
                "tool_result_seen": LLMHandler.saw_tool_result,
                "no_sim_error": sim.error is None,
            }
            passed = all(checks.values())
            for name, ok in checks.items():
                print(f"  {'PASS' if ok else 'FAIL'}  {name}")
            if not passed:
                print(f"  device cmds: {sim.cmds}")
                print(f"  chunks: {sim.ai_chunks!r}")
                print(f"  text: {sim.ai_text!r}")
                print(f"  sim error: {sim.error}")
    finally:
        bridge.terminate()
        try:
            out, _ = bridge.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            bridge.kill()
            out, _ = bridge.communicate()
        if not passed:
            print("--- bridge output ---")
            print(out[-4000:])
        llm.shutdown()
        server.close()

    print("e2e bridge test passed" if passed else "e2e bridge test FAILED")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
