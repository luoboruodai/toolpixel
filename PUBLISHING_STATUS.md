# Publishing status — v0.1.2

## Published

- Repository: [`luoboruodai/toolpixel`](https://github.com/luoboruodai/toolpixel)
- Release: [`v0.1.2`](https://github.com/luoboruodai/toolpixel/releases/tag/v0.1.2)
- Release tag object: `51f0a689172d3227aa833d8df96a6c5fe9577620`
- Tagged commit: `5e31503410eead4921110d49432fb576645d8823`
- Reviewed source commit: `4909bf2e2a12020cb6479f07beecd315aeb201ce`
- Release form: prerelease, because Windows AE has not been host-tested.

## Verified artifacts

| Artifact | SHA-256 |
|---|---|
| `ToonPixel-v0.1.2-macOS-universal.zip` | `0b9c98ef037dd781ab7de1dd371235fb5be1bcc953460d68d496329fda4d492e` |
| `ToonPixel-v0.1.2-Windows-x64.zip` | `dc65f92ee18262a061692f806ea80baa441cd7cdb05e49af0d40556b16a4d9a4` |

The uploaded Release files were downloaded again from GitHub and matched the reviewed local candidate byte-for-byte. The macOS installed binary used for validation was `a7523bad5cf73a4891b3c9752acd170614d112f1791ed9b307ac7a3d1b2765b7`.

## Validation

- macOS: Debug / Release / ASan each 4/4; AE 26.5x89 on Apple Silicon; bilingual round-trip and 99/99 real outputs; PiPL `0x9601` confirmed in the installed bundle.
- Windows x64: cross-built PE `.aex`; `EffectMain`, imports, PiPL ID 16000 and VERSIONINFO static checks passed.
- Public/private boundary: no Adobe SDK, HostProbe, private projects, logs, attachments, test media, backups or local caches in the public tree.

## Not claimed

- Windows AE loading or rendering.
- Windows on Arm, Intel-host testing, other AE versions, GPU/Classic rendering, 8/16 bpc or full color management.
- Continuous drag or host `studio_idle` verification.

See [release notes](docs/RELEASE_NOTES-v0.1.2.md), [installation: macOS](docs/INSTALL-MACOS.md), and [installation: Windows](docs/INSTALL-WINDOWS.md).
