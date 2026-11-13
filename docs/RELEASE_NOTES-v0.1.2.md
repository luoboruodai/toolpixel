# ToonPixel SmartFX v0.1.2

**English** | [简体中文](#简体中文)

## English

ToonPixel is a native After Effects C++ effect for cel shading, pixel-art palettes, dithering, contour coverage and ASCII raster output. This release adds English/Chinese parameter and custom-widget localization and introduces a real Windows x64 cross-build alongside the universal macOS bundle.

### Download attachments

- `ToonPixel-v0.1.2-macOS-universal.zip` — complete `ToonPixel.plugin`, `arm64` and `x86_64`, with offline installation instructions and notices. SHA-256: `0b9c98ef037dd781ab7de1dd371235fb5be1bcc953460d68d496329fda4d492e`.
- `ToonPixel-v0.1.2-Windows-x64.zip` — `ToonPixel.aex`, offline installation instructions and notices. SHA-256: `dc65f92ee18262a061692f806ea80baa441cd7cdb05e49af0d40556b16a4d9a4`.
- `SHA256SUMS.txt` — SHA-256 for both ZIPs.

These are installer attachments. GitHub's automatic Source code archives contain source, not the compiled plug-ins.

### Highlights

- Five-band Luma Ramp or Preserve Hue, Toon/Pixel/combined output, custom palettes, Bayer and Floyd–Steinberg dithering, spatial decay, Oklch tint, Oklab gamut mapping, contour AA, and fourteen ASCII/graphic styles.
- English by default. Parameter names and custom Ramp, Oklch and ASCII captions switch to Chinese; popup items remain bilingual.
- Existing parameter indices are preserved. Language is appended at index 97; 98 native parameters include the implicit input. ExtendScript property 98 is Compositing Options.

### Verified

- macOS: Debug / Release / ASan each 4/4; AE 26.5x89 on Apple Silicon, bilingual round-trip and **99/99 real outputs**, including the Ramp color contract. Installed binary SHA-256: `a7523bad5cf73a4891b3c9752acd170614d112f1791ed9b307ac7a3d1b2765b7`.
- Windows: actual x64 PE DLL and feature-smoke executable cross-built. `EffectMain`, static dependencies, exact-case PiPL ID 16000, native `0.1.2 release 1`, and Windows `0.1.2.1` resources checked.
- Reviewed source commit: `4909bf2e2a12020cb6479f07beecd315aeb201ce`.

### Windows preview and other limits

**Windows AE has not been host-tested.** Test this x64 `.aex` in a disposable project first. Cross-build/static checks do not prove loading, text rendering, dialogs or rendering compatibility. Windows on Arm is not included. Continuous drag and host idle, Intel hosts, other AE versions, GPU/Classic rendering, 8/16 bpc and full color management remain outside the verified scope.

The macOS bundle is ad-hoc signed, not notarized. Windows has no Authenticode signature. Verify the downloaded ZIP before installation and keep a recoverable backup of the previous plug-in.

[Installation: macOS](INSTALL-MACOS.md) · [Installation: Windows](INSTALL-WINDOWS.md) · [Compatibility](COMPATIBILITY.md) · [Build](BUILDING.md)

## 简体中文

ToonPixel 是 After Effects 原生 C++ 效果，提供三渲二色带、像素色板、抖动、轮廓覆盖和 ASCII 字符栅格。本版增加中英文参数名与自绘控件切换，并在通用 macOS 包之外提供真实 Windows x64 交叉构建。

### 下载附件

- `ToonPixel-v0.1.2-macOS-universal.zip`：完整 `ToonPixel.plugin`，含 `arm64` 和 `x86_64`，附离线安装指南与声明。SHA-256：`0b9c98ef037dd781ab7de1dd371235fb5be1bcc953460d68d496329fda4d492e`。
- `ToonPixel-v0.1.2-Windows-x64.zip`：`ToonPixel.aex`，附离线安装指南与声明。SHA-256：`dc65f92ee18262a061692f806ea80baa441cd7cdb05e49af0d40556b16a4d9a4`。
- `SHA256SUMS.txt`：两个 ZIP 的 SHA-256。

以上是实际安装附件。GitHub 自动 Source code 包是源码，不含已编译插件。

### 主要变化

- 五档亮度色带或保留色相、卡通/像素/组合输出、自定义色板、Bayer 与 Floyd–Steinberg 抖动、空间衰减、Oklch 调色、Oklab 色域映射、轮廓抗锯齿及 14 种字符/图形样式。
- 默认英文；参数名和 Ramp、Oklch、ASCII 自绘文字切换中文，原生菜单保持双语。
- 保留旧参数索引；语言追加在索引 97，含隐式输入共 98 项。ExtendScript 第 98 项为“合成选项”。

### 验证范围

- macOS：Debug / Release / ASan 各 4/4；Apple Silicon 的 AE 26.5x89 完成中英文往返和 **99/99 真实输出**，Ramp 色彩契约通过。安装版二进制 SHA-256：`a7523bad5cf73a4891b3c9752acd170614d112f1791ed9b307ac7a3d1b2765b7`。
- Windows：已交叉构建真实 x64 PE DLL 和测试程序。核验 `EffectMain`、静态依赖、精确大小写的 PiPL 16000、原生 `0.1.2 release 1` 与 Windows `0.1.2.1` 资源。
- 已审核源码提交：`4909bf2e2a12020cb6479f07beecd315aeb201ce`。

### Windows 预发布与其他限制

**尚未在 Windows AE 中实测。**请先在可丢弃的测试工程中验证此 x64 `.aex`。交叉构建和静态检查不证明加载、文字渲染、对话框或渲染兼容性。不包含 Windows on Arm 原生版本。连续拖动与宿主 idle、Intel 实机、其他 AE 版本、GPU/Classic、8/16 bpc 和完整色彩管理未覆盖。

macOS 仅有 ad-hoc 签名，未经公证；Windows 无 Authenticode 签名。安装前请核对 ZIP 校验值，并保留旧插件的可恢复备份。

[macOS 安装](INSTALL-MACOS.md) · [Windows 安装](INSTALL-WINDOWS.md) · [兼容性](COMPATIBILITY.md) · [构建](BUILDING.md)
