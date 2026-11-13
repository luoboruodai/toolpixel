#!/usr/bin/env python3
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "win"))
from generate_pipl import VERSION, OUT_FLAGS, OUT_FLAGS2, build as expected_pipl


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_windows_binary.py PLUGIN.aex")
    path = Path(sys.argv[1])
    blob = path.read_bytes()
    if blob[:2] != b"MZ":
        raise AssertionError("missing DOS header")
    pe_offset = struct.unpack_from("<I", blob, 0x3C)[0]
    if blob[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise AssertionError("missing PE signature")
    coff = pe_offset + 4
    machine, section_count, _, _, _, optional_size, flags = struct.unpack_from("<HHIIIHH", blob, coff)
    optional = coff + 20
    assert machine == 0x8664 and flags & 0x2000, "not an x64 DLL"
    assert struct.unpack_from("<H", blob, optional)[0] == 0x20B, "not PE32+"
    assert struct.unpack_from("<H", blob, optional + 68)[0] == 2, "not a Windows GUI DLL"
    sections = []
    for index in range(section_count):
        offset = optional + optional_size + index * 40
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", blob, offset + 8)
        sections.append((rva, max(virtual_size, raw_size), raw_offset))

    def file_offset(rva):
        for base, size, offset in sections:
            if base <= rva < base + size:
                return offset + rva - base
        raise AssertionError(f"unmapped RVA: 0x{rva:x}")

    def c_string(offset):
        return blob[offset:blob.index(b"\0", offset)].decode("ascii")

    def directory(index):
        return struct.unpack_from("<II", blob, optional + 112 + index * 8)

    export_rva, export_size = directory(0)
    assert export_rva and export_size, "missing exports"
    export = file_offset(export_rva)
    fields = struct.unpack_from("<IIHHIIIIIII", blob, export)
    names_rva = fields[9]
    names_count = fields[7]
    exports = {c_string(file_offset(struct.unpack_from("<I", blob, file_offset(names_rva) + i * 4)[0])) for i in range(names_count)}
    assert exports == {"EffectMain"}, f"unexpected exports: {sorted(exports)}"

    import_rva, _ = directory(1)
    imports = set()
    for index in range(128):
        offset = file_offset(import_rva) + index * 20
        lookup, _, _, name_rva, address = struct.unpack_from("<IIIII", blob, offset)
        if not any((lookup, name_rva, address)):
            break
        imports.add(c_string(file_offset(name_rva)).lower())
    else:
        raise AssertionError("unterminated import table")
    assert imports, "missing runtime imports"
    allowed = {"kernel32.dll", "msvcrt.dll", "ucrtbase.dll"}
    assert all(item in allowed or item.startswith("api-ms-win-crt-") for item in imports), f"unexpected imports: {sorted(imports)}"

    resource_rva, _ = directory(2)
    assert resource_rva, "missing resources"
    resource_base = file_offset(resource_rva)

    def entries(relative):
        offset = resource_base + relative
        named, numbered = struct.unpack_from("<HH", blob, offset + 12)
        for index in range(named + numbered):
            key, child = struct.unpack_from("<II", blob, offset + 16 + index * 8)
            if key & 0x80000000:
                name_offset = resource_base + (key & 0x7FFFFFFF)
                length = struct.unpack_from("<H", blob, name_offset)[0]
                key = blob[name_offset + 2:name_offset + 2 + length * 2].decode("utf-16le")
            yield key, child

    resources = {}
    for kind, child in entries(0):
        assert child & 0x80000000
        for identifier, child_id in entries(child & 0x7FFFFFFF):
            assert child_id & 0x80000000
            for language, leaf in entries(child_id & 0x7FFFFFFF):
                assert not leaf & 0x80000000
                rva, length = struct.unpack_from("<II", blob, resource_base + leaf)
                resources[(kind, identifier, language)] = blob[file_offset(rva):file_offset(rva) + length]
    pipl_values = [value for (kind, identifier, _), value in resources.items() if kind == "PiPL" and identifier == 16000]
    assert len(pipl_values) == 1, "expected exactly one PiPL resource at ID 16000"
    assert pipl_values[0] == expected_pipl(), "embedded PiPL differs from generator"

    pipl = pipl_values[0]
    version, count = struct.unpack_from(">II", pipl)
    assert version == 0 and count == 12, "unexpected PiPL header"
    properties = {}
    offset = 8
    for _ in range(count):
        assert pipl[offset:offset + 4] == b"8BIM", "invalid PiPL vendor"
        key = pipl[offset + 4:offset + 8].decode("ascii")
        identifier, length = struct.unpack_from(">II", pipl, offset + 8)
        assert identifier == 0 and key not in properties and length < 1024
        properties[key] = pipl[offset + 16:offset + 16 + length]
        offset += 16 + (length + 3 & ~3)
    assert offset == len(pipl), "PiPL alignment mismatch"
    assert properties["kind"] == b"eFKT"
    assert properties["name"] == bytes([len("ToonPixel SmartFX")]) + b"ToonPixel SmartFX"
    assert properties["eMNA"] == properties["name"]
    assert properties["8664"] == b"EffectMain\0"
    assert properties["eSVR"] == struct.pack(">HH", 13, 2)
    assert properties["eVER"] == struct.pack(">I", VERSION)
    assert properties["eGLO"] == struct.pack(">I", OUT_FLAGS)
    assert properties["eGL2"] == struct.pack(">I", OUT_FLAGS2)

    versions = [value for (kind, _, _), value in resources.items() if kind == 16]
    assert len(versions) == 1, "missing VERSIONINFO"
    fixed_offset = versions[0].find(struct.pack("<I", 0xFEEF04BD))
    assert fixed_offset >= 0, "missing VS_FIXEDFILEINFO"
    expected = (0xFEEF04BD, 0x00010000, 0x00000001, 0x00020001, 0x00000001, 0x00020001)
    assert struct.unpack_from("<6I", versions[0], fixed_offset) == expected, "wrong file/product version"
    assert "0.1.2".encode("utf-16le") in versions[0], "missing version string"
    print(f"PASS: {path.name}: x64 PE, EffectMain, static imports, PiPL #16000, version 0.1.2")


if __name__ == "__main__":
    main()
