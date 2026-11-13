# Publishing status — v0.1.2

## Candidate scope

- **Source:** ToonPixel SmartFX, native C++17, bilingual parameter names and custom captions.
- **macOS:** universal `arm64` + `x86_64` bundle, ad-hoc signed. AE 26.5x89 / Apple Silicon bilingual UI and 99/99 real output cases verified. Intel not host-tested.
- **Windows x64:** cross-built PE `.aex`; export/import/PiPL/VERSIONINFO static checks passed. Windows AE not host-tested.
- **Release form:** preview for Windows validation. Not a claim of certified dual-platform production compatibility.

## Public / private boundary

The public tree includes original ToonPixel source, build scripts and bilingual documentation. It excludes the Adobe SDK, HostProbe, private projects, logs, attachments, test media, backups and local build caches. Release attachments are produced from explicitly mapped files, not by archiving the whole working directory.

## Remaining release gates

- Freeze and review the candidate files, actual staged blobs and archive members.
- Rebuild and test from the public source tree.
- Package and verify the exact binaries, offline guides, hashes and ZIP members.
- Push through the repository's release flow, then read back the tag, commit, attachments and downloaded bytes.
- Record any unperformed Windows AE validation as pending, not passed.

See [release notes](docs/RELEASE_NOTES-v0.1.2.md) for installation and validation details.
