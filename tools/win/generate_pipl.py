#!/usr/bin/env python3
from pathlib import Path
import struct
import sys

VERSION = 0x0000B001
OUT_FLAGS = 0x04108200
OUT_FLAGS2 = 0x00001400


def text(value, null_terminated=False):
    data = value.encode("ascii")
    if null_terminated:
        return data + b"\0"
    if len(data) > 255:
        raise ValueError("PiPL text exceeds 255 bytes")
    return bytes([len(data)]) + data


def entry(key, payload):
    result = b"8BIM" + key.encode("ascii") + struct.pack(">II", 0, len(payload)) + payload
    return result + bytes((-len(payload)) % 4)


def build():
    properties = [
        ("kind", b"eFKT"),
        ("name", text("ToonPixel SmartFX")),
        ("catg", text("ToonPixel")),
        ("8664", text("EffectMain", True)),
        ("ePVR", struct.pack(">I", 0x00020000)),
        ("eSVR", struct.pack(">HH", 13, 2)),
        ("eVER", struct.pack(">I", VERSION)),
        ("eINF", struct.pack(">h", 0)),
        ("eGLO", struct.pack(">I", OUT_FLAGS)),
        ("eGL2", struct.pack(">I", OUT_FLAGS2)),
        ("eMNA", text("ToonPixel SmartFX")),
        ("aeFL", struct.pack(">I", 0)),
    ]
    return struct.pack(">II", 0, len(properties)) + b"".join(entry(*item) for item in properties)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: generate_pipl.py OUTPUT")
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(build())
    print(f"PiPL: {output} ({output.stat().st_size} bytes, version=0x{VERSION:08x})")


if __name__ == "__main__":
    main()
