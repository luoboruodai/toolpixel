# Build ToonPixel 0.1.2 / 构建说明

## Requirements / 要求

- A licensed Adobe After Effects SDK. **Do not commit it.** Set `AE_SDK_ROOT` to its directory. The directory must contain `Headers/AE_Effect.h`.
- macOS: CMake 3.20+, C++17 compiler, macOS SDK, Cocoa/CoreServices, `Rez`, `codesign`.
- Windows cross-build: MinGW-w64 `g++`, `windres`, `strip`, Python 3 and Bash. Current validation used MinGW-w64 GCC 16.2.0 on Apple Silicon.

The source uses native C++ effects. It does not require CEP, Node.js or a Python runtime after installation.

## macOS

```bash
AE_SDK_ROOT=/path/to/AfterEffectsSDK ./build.sh
```

The wrapper configures `build-fusion`, builds the universal bundle, runs all four CTest checks and creates a development pkg under `artifacts/installer/`. The bundle targets macOS 11+, `arm64` and `x86_64`, with an ad-hoc signature. Intel and other AE versions still require separate host testing.

For diagnostics:

```bash
AE_SDK_ROOT=/path/to/AfterEffectsSDK cmake -S . -B build-fusion-debug -DTOONPIXEL_DEBUG_LOG=ON
cmake --build build-fusion-debug --parallel 2
ctest --test-dir build-fusion-debug --output-on-failure
```

Diagnostic builds are not substitutes for testing the actual Release bundle. Do not ship `ToonPixelHostProbe`, private AE projects, EXR/PSD files, logs or private media.

## Windows x64

```bash
AE_SDK_ROOT=/path/to/AfterEffectsSDK ./build_win.sh
```

The script generates a big-endian PiPL, preserves its exact resource-case spelling, compiles `src/ae/ToonPixel.cpp` into a real PE DLL, links statically and runs `tools/check_windows_binary.py`. Output is `build-win/ToonPixel.aex` with version `0.1.2 release 1` and file version `0.1.2.1`.

Static checks cover the x64 PE format, `EffectMain`, imports, PiPL ID 16000, native version/flags, and VERSIONINFO. They do **not** prove AE loading, parameter encoding, dialogs, rendering, IME or long-session stability on Windows. Run those tests in a real target Windows/AE environment before production use.

## Package / 打包

After reviewing and freezing a source commit:

```bash
python3 tools/package_release.py --commit REVIEWED_COMMIT_SHA
```

The packager accepts only the built bundle, Windows `.aex`, offline installation guides and notices. It verifies both binaries, expected members, ZIP CRC, hashes and the bundle signature. Release ZIPs and `SHA256SUMS.txt` are written to `dist/`. No source archive from GitHub should be presented as an installer attachment.

## 简体中文

Adobe SDK 不随仓库分发，请将其放在你有权使用的本地目录，并设置 `AE_SDK_ROOT`。macOS 使用 `./build.sh` 完成双架构构建与回归；Windows 使用 `./build_win.sh` 生成 `.aex` 并检查资源、导出与依赖。真实宿主兼容验证不能由交叉构建或静态检查替代。冻结提交后使用 `tools/package_release.py` 生成两个安装 ZIP 和校验文件，所有候选包通过成员、CRC、哈希和签名校验后才可用于发布。
