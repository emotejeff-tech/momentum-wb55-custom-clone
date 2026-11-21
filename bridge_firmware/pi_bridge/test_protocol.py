#!/usr/bin/env python3
"""Self-tests for the bridge framing (run: python3 test_protocol.py)."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bridge  # noqa: E402


def main() -> int:
    # CRC-16/CCITT-FALSE known vector
    assert bridge.crc16_ccitt_false(b"123456789") == 0x29B1

    # COBS round-trips, including zero-heavy and maximum-length blocks
    cases = [
        b"",
        b"\x00",
        b"\x00\x00",
        bytes(range(256)),
        os.urandom(1000),
        b"\x00" * 300,
        bytes([0x11] * 254),
        bytes([0x11] * 255),
        bytes([0x00] + [0x22] * 253 + [0x00]),
    ]
    for case in cases:
        encoded = bridge.cobs_encode(case)
        assert 0 not in encoded, "COBS output contains a delimiter"
        assert bridge.cobs_decode(encoded) == case

    # Frame round-trip with a payload that contains zeros and quotes
    payload = b'{"id":"t1","op":"status","x":"' + b"a\x00b" * 100 + b'"}'
    frame = bridge.build_frame(bridge.T_CMD, 42, bridge.F_ACK_REQ, payload)
    assert frame[-1] == 0
    parsed = bridge.parse_frame(frame[:-1])
    assert parsed is not None
    assert parsed["type"] == bridge.T_CMD
    assert parsed["seq"] == 42
    assert parsed["flags"] == bridge.F_ACK_REQ
    assert parsed["payload"] == payload

    # A single flipped bit must fail CRC
    bad = bytearray(frame[:-1])
    bad[len(bad) // 2] ^= 0x5A
    assert bridge.parse_frame(bytes(bad)) is None

    # Maximum payload
    big = b"x" * bridge.MAX_PAYLOAD
    frame = bridge.build_frame(bridge.T_AI_RESP, 1, 0, big)
    assert bridge.parse_frame(frame[:-1])["payload"] == big

    print("protocol self-tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
