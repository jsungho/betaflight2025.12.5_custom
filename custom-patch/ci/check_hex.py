#!/usr/bin/env python3
"""Verify custom strings exist in built .hex files (Intel HEX decode). Usage: check_hex.py <glob>"""
import glob, sys

NEEDLES = [
    b"alt_hold_deadband_low",
    b"alt_hold_full_low_is_max_descend",
    b"alt_hold_hover_throttle",
    b"landing_disarm_airmode_off_only",
    b"ALTHOLD : LANDING",
    b"ALT WAIT",
]

def decode_hex(path):
    data, base = bytearray(), 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(":"):
                continue
            n = int(line[1:3], 16); addr = int(line[3:7], 16); rt = int(line[7:9], 16)
            payload = line[9:9 + n * 2]
            if rt == 0x04:
                base = int(payload, 16) << 16
            elif rt == 0x00:
                a = base + addr
                raw = bytes.fromhex(payload)
                if a + len(raw) > len(data):
                    data.extend(b"\x00" * (a + len(raw) - len(data)))
                data[a:a + len(raw)] = raw
    return bytes(data)

files = sorted(glob.glob(sys.argv[1]))
if not files:
    print("no hex files matched", sys.argv[1]); sys.exit(1)
bad = 0
for fp in files:
    blob = decode_hex(fp)
    print(fp.split("/")[-1])
    for n in NEEDLES:
        ok = n in blob
        bad += 0 if ok else 1
        print(f"   {n.decode()}: {'OK' if ok else 'MISSING'}")
sys.exit(1 if bad else 0)
