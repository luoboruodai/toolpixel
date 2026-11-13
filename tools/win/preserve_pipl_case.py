#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: preserve_pipl_case.py RESOURCE_OBJECT")
path = Path(sys.argv[1])
data = path.read_bytes()
upper = b"\x04\x00" + "PIPL".encode("utf-16le")
canonical = b"\x04\x00" + "PiPL".encode("utf-16le")
if data.count(upper) != 1:
    raise SystemExit("expected one uppercase PIPL resource type")
path.write_bytes(data.replace(upper, canonical, 1))
print("PiPL resource case preserved")
