#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import subprocess
import zipfile

VERSION = "0.1.2"
ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
MAC = f"ToonPixel-v{VERSION}-macOS-universal.zip"
WINDOWS = f"ToonPixel-v{VERSION}-Windows-x64.zip"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked(command):
    return subprocess.run(command, cwd=ROOT, check=True, text=True, capture_output=True).stdout


def package(kind, input_path, output_name, members):
    output = DIST / output_name
    temporary = DIST / (output_name + ".candidate")
    with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as archive:
        for source, destination in members:
            archive.write(source, destination)
    expected = sorted(destination for _, destination in members)
    with zipfile.ZipFile(temporary) as archive:
        assert archive.testzip() is None, f"CRC failure: {output_name}"
        assert sorted(archive.namelist()) == expected, f"Unexpected members: {archive.namelist()}"
        assert all(not Path(name).is_absolute() and ".." not in Path(name).parts for name in expected)
        assert all(not Path(name).name.startswith("._") for name in expected)
        for source, destination in members:
            assert archive.read(destination) == source.read_bytes(), f"Content mismatch: {destination}"
    temporary.replace(output)
    return output, expected


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--commit", required=True, help="Reviewed source commit to record in the manifest")
    args = parser.parse_args()
    if len(args.commit) != 40 or any(character not in "0123456789abcdef" for character in args.commit.lower()):
        raise SystemExit("--commit must be a full hexadecimal Git commit")
    mac_bundle = ROOT / "build-fusion/ToonPixel.plugin"
    mac_binary = mac_bundle / "Contents/MacOS/ToonPixel"
    windows_binary = ROOT / "build-win/ToonPixel.aex"
    for path in (mac_binary, windows_binary):
        if not path.is_file():
            raise SystemExit(f"Missing built input: {path}")
    checked(["codesign", "--verify", "--deep", "--strict", str(mac_bundle)])
    architectures = checked(["lipo", "-info", str(mac_binary)])
    assert "arm64" in architectures and "x86_64" in architectures, architectures
    metadata = plistlib.loads((mac_bundle / "Contents/Info.plist").read_bytes())
    assert metadata["CFBundleShortVersionString"] == VERSION
    checked(["python3", str(ROOT / "tools/check_windows_binary.py"), str(windows_binary)])
    DIST.mkdir(exist_ok=True)
    notice_members = [(ROOT / name, name) for name in ("LICENSE.md", "NOTICE.md", "THIRD_PARTY_NOTICES.md")]
    mac_members = [(mac_bundle / "Contents/Info.plist", "ToonPixel.plugin/Contents/Info.plist"),
                   (mac_bundle / "Contents/PkgInfo", "ToonPixel.plugin/Contents/PkgInfo"),
                   (mac_bundle / "Contents/MacOS/ToonPixel", "ToonPixel.plugin/Contents/MacOS/ToonPixel"),
                   (mac_bundle / "Contents/Resources/ToonPixel.rsrc", "ToonPixel.plugin/Contents/Resources/ToonPixel.rsrc"),
                   (mac_bundle / "Contents/_CodeSignature/CodeResources", "ToonPixel.plugin/Contents/_CodeSignature/CodeResources"),
                   (ROOT / "docs/INSTALL-MACOS.md", "INSTALL-MACOS.md")] + notice_members
    windows_members = [(windows_binary, "ToonPixel.aex"),
                       (ROOT / "docs/INSTALL-WINDOWS.md", "INSTALL-WINDOWS.md")] + notice_members
    mac_zip, mac_contents = package("macOS", mac_binary, MAC, mac_members)
    windows_zip, windows_contents = package("Windows", windows_binary, WINDOWS, windows_members)
    checksums = DIST / "SHA256SUMS.txt"
    checksums.write_text(f"{sha256(mac_zip)}  {MAC}\n{sha256(windows_zip)}  {WINDOWS}\n", encoding="utf-8")
    manifest = {
        "version": VERSION,
        "source_commit": args.commit,
        "macOS": {"path": MAC, "sha256": sha256(mac_zip), "bundleSha256": sha256(mac_binary),
                  "architectures": ["arm64", "x86_64"], "signature": "ad-hoc", "members": mac_contents},
        "windows": {"path": WINDOWS, "sha256": sha256(windows_zip), "binarySha256": sha256(windows_binary),
                    "architecture": "x64", "signature": "unsigned", "members": windows_contents},
        "validation": {"macOS": "AE 26.5x89 on Apple Silicon: bilingual round-trip and 99/99 real outputs",
                       "windows": "cross-built and static checks; Windows AE not host-tested"},
    }
    (DIST / "release-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"macOS": manifest["macOS"], "windows": manifest["windows"]}, indent=2))


if __name__ == "__main__":
    main()
