# Changelog

## v0.1.2 — 2026-11-13

### Added

- English/Chinese parameter names and custom Ramp, Oklch and ASCII captions; native popup items remain bilingual.
- Real Windows x64 `.aex` cross-build, big-endian PiPL and VERSIONINFO resources, and static PE validation.
- macOS universal release bundle, Windows preview package, offline bilingual installation guides, release manifest and SHA-256 files.

### Preserved

- Match name `ToonPixel SmartFX`, existing parameter indices and default English behavior.
- Language parameter appended at index 97; 98 native parameters including implicit input.
- RGB/Alpha approximation architecture; no claim of a true G-buffer or 3D reconstruction.

### Verified and limited

- macOS: all three test configurations 4/4, bilingual host round-trip and 99/99 real outputs on AE 26.5x89 / Apple Silicon.
- Windows: cross-build and static binary checks only; Windows AE loading and rendering remain unverified.
