#!/usr/bin/env python3
"""Compile-smoke the portable firmware modules on the host.

Compiles, with `zig cc` and minimal Furi HAL stubs:
  * firmware/display_driver_patch/st7567_i2c.c
  * firmware/display_driver_patch/u8g2_st7567_i2c.c   (needs real u8g2 headers)
  * firmware/nrf24/nrf24.c

The STM32-specific target files (resources, SPI/I2C config) are NOT covered:
they need the real STM32 LL headers and are validated by an actual Momentum
build (`fbt TARGET_HW=55`).

u8g2 headers are searched in:
  $MOMENTUM_LIB_U8G2
  .research/Momentum-Firmware/lib/u8g2
  ../Momentum-Firmware/lib/u8g2

Usage: python3 tests/compile_smoke.py
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STUBS = os.path.join(ROOT, "tests", "host_stubs")


def find_u8g2() -> str | None:
    candidates = [
        os.environ.get("MOMENTUM_LIB_U8G2"),
        os.path.join(ROOT, ".research", "Momentum-Firmware", "lib", "u8g2"),
        os.path.join(os.path.dirname(ROOT), ".research", "Momentum-Firmware", "lib", "u8g2"),
        os.path.join(os.path.dirname(ROOT), "Momentum-Firmware", "lib", "u8g2"),
    ]
    for candidate in candidates:
        if candidate and os.path.exists(os.path.join(candidate, "u8g2.h")):
            return candidate
    return None


def compile_one(source: str, include_extra: list[str], out_dir: str) -> bool:
    out = os.path.join(out_dir, os.path.basename(source) + ".o")
    cmd = [
        sys.executable,
        "-m",
        "ziglang",
        "cc",
        "-c",
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I",
        STUBS,
        *[f"-I{p}" for p in include_extra],
        source,
        "-o",
        out,
    ]
    print(f"$ {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        return False
    return True


def main() -> int:
    try:
        version = subprocess.run(
            [sys.executable, "-m", "ziglang", "version"], capture_output=True, text=True
        )
    except Exception:  # noqa: BLE001
        print("SKIP: ziglang not installed (pip install ziglang)")
        return 0
    if version.returncode != 0:
        print("SKIP: ziglang not installed (pip install ziglang)")
        return 0
    print(f"zig {version.stdout.strip()}")

    u8g2 = find_u8g2()
    if not u8g2:
        print("SKIP u8g2 glue: no Momentum lib/u8g2 found (set MOMENTUM_LIB_U8G2)")

    sources = [
        (os.path.join(ROOT, "firmware", "display_driver_patch", "st7567_i2c.c"), [], True),
        (os.path.join(ROOT, "firmware", "nrf24", "nrf24.c"), [], True),
    ]
    if u8g2:
        sources.append(
            (
                os.path.join(ROOT, "firmware", "display_driver_patch", "u8g2_st7567_i2c.c"),
                [u8g2, os.path.join(ROOT, "firmware", "display_driver_patch")],
                True,
            )
        )

    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        for source, extra, _required in sources:
            name = os.path.relpath(source, ROOT)
            good = compile_one(source, extra, tmp)
            print(f"{'PASS' if good else 'FAIL'}  {name}")
            ok = ok and good

    print("compile smoke passed" if ok else "compile smoke FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
