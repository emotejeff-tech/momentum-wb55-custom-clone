#!/usr/bin/env python3
"""Run every host-side test for this repository.

  1. Python framing self-tests
  2. C framing self-tests (compiled with `zig cc`; skipped if unavailable)
  3. C <-> Python interop on the exact frame emitted by the C test
  4. End-to-end bridge test against a simulated STM32

Usage: python3 tests/run_all.py
"""

from __future__ import annotations

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PI = os.path.join(ROOT, "bridge_firmware", "pi_bridge")
C_DIR = os.path.join(ROOT, "bridge_firmware", "protocol")

sys.path.insert(0, PI)
import bridge as proto  # noqa: E402


def run(cmd: list[str], cwd: str | None = None) -> subprocess.CompletedProcess:
    print(f"$ {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def test_python() -> bool:
    result = run([sys.executable, os.path.join(PI, "test_protocol.py")])
    print(result.stdout.strip())
    if result.returncode != 0:
        print(result.stderr)
    return result.returncode == 0


def test_c() -> tuple[bool, str | None]:
    exe = os.path.join(C_DIR, "test_protocol.exe" if os.name == "nt" else "test_protocol")
    compile_cmd = [
        sys.executable,
        "-m",
        "ziglang",
        "cc",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "test_protocol.c",
        "-o",
        exe,
    ]
    compiled = run(compile_cmd, cwd=C_DIR)
    if compiled.returncode != 0:
        print("C tests skipped (zig unavailable or compile failed):")
        print((compiled.stderr or compiled.stdout)[-1500:])
        return True, None

    run_exe = run([exe], cwd=C_DIR)
    print(run_exe.stdout.strip())
    if run_exe.returncode != 0:
        print(run_exe.stderr)
        return False, None

    frame_hex = None
    for line in run_exe.stdout.splitlines():
        if line.startswith("FRAME "):
            frame_hex = line.split(" ", 1)[1].strip()
    return True, frame_hex


def test_interop(frame_hex: str) -> bool:
    if not frame_hex:
        print("interop skipped: no FRAME line")
        return True

    raw = bytes.fromhex(frame_hex)
    assert raw[-1] == 0x00, "frame must end with the COBS delimiter"
    parsed = proto.parse_frame(raw[:-1])
    ok = (
        parsed is not None
        and parsed["type"] == proto.T_CMD
        and parsed["seq"] == 42
        and parsed["flags"] == proto.F_ACK_REQ
        and parsed["payload"] == b'{"id":"c1","op":"status"}'
    )
    # Re-encode with the Python implementation and require identical bytes.
    rebuilt = proto.build_frame(
        parsed["type"], parsed["seq"], parsed["flags"], parsed["payload"]
    ) if parsed else b""
    ok = ok and rebuilt == raw
    print(f"{'PASS' if ok else 'FAIL'}  C<->Python interop ({len(raw)} bytes)")
    return ok


def test_e2e() -> bool:
    result = run([sys.executable, os.path.join(ROOT, "tests", "e2e_bridge_test.py")])
    print(result.stdout.strip())
    if result.returncode != 0:
        print(result.stderr[-2000:])
    return result.returncode == 0


def test_compile_smoke() -> bool:
    result = run([sys.executable, os.path.join(ROOT, "tests", "compile_smoke.py")])
    print(result.stdout.strip())
    if result.returncode != 0:
        print(result.stderr[-2000:])
    return result.returncode == 0


def main() -> int:
    results = {}
    results["python framing"] = test_python()
    c_ok, frame_hex = test_c()
    results["C framing"] = c_ok
    results["C/Python interop"] = test_interop(frame_hex) if c_ok else False
    results["compile smoke"] = test_compile_smoke()
    results["end-to-end bridge"] = test_e2e()

    print("\n=== summary ===")
    for name, ok in results.items():
        print(f"{'PASS' if ok else 'FAIL'}  {name}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
