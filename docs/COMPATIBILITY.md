# Compatibility and verification / 兼容性与验证

## Current version / 当前版本

- Effect name and match name: **ToonPixel SmartFX**
- Native version: **0.1.2 release 1** (`0x00009601`)
- Windows file version: **0.1.2.1**
- Parameter count: **98 including implicit input**
- Language parameter index: **97**; ExtendScript property 98 is host-provided Compositing Options.

## Evidence by platform / 平台证据

| Target | Built | Executed | Host result |
|---|---|---|---|
| macOS arm64, AE 26.5x89 | Universal bundle, ad-hoc signature verified | Debug / Release / ASan each 4/4 | **Passed:** bilingual UI round-trip, 99/99 real output cases, Ramp color contract |
| macOS x86_64 | `x86_64` slice present | CTest on arm64 only | **Not host-tested** |
| Windows x64 | MinGW-w64 PE `.aex`; static binary/resource checks passed | Cross-compiled feature smoke, not executed on Windows | **Not tested in Windows AE** |
| Windows on Arm | No ARM64 build | — | **Not supported by this attachment** |

The verified macOS environment was 32 bpc, `workingSpace=None`, `linearBlending=false`, CPU SmartFX and `MFR=OFF`. That result does not extend to GPU/Classic rendering, 8/16 bpc, full color management or other AE versions.

## Known limits / 已知限制

- RGB and Alpha approximation only. No normal, depth, material or object-ID G-buffer and no reconstructed 3D geometry.
- Continuous drag and host `studio_idle` remain unverified. The offline interaction state machine is not a host-input certificate.
- Windows parameter encoding, native dialogs, IME, project paths with non-ASCII text, load/unload lifetime and rendering still require real Windows AE validation.
- No notarized macOS bundle, Developer ID certificate, Authenticode signature or commercial signing claim.
- No claim is made that all algorithms equal those of third-party reference plug-ins.

## Bilingual scripting / 双语脚本

Use stable numeric parameter indices for scripts that must work in either language. `effect.property("Mode")` works in English but is not a cross-language lookup after switching to Chinese. Use `effect.property(1)` for Mode or restore English. The language selector is at index 97; do not use `effect.numProperties` as its index.
