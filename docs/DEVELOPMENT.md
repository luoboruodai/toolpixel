# ToonPixel Development Overview / 开发概览

**English** | [简体中文](#简体中文)

## English

This page summarizes the public development history of ToonPixel SmartFX. Detailed local handoffs, validation logs and host projects remain private; this document intentionally excludes personal paths, private fixtures, test bridges, logs and unpublished media.

### Project goal

Build a native After Effects C++ effect that converts Beauty/RGB and Alpha into a stylized 2D look:

- Cel shading through Preserve Hue or a five-band Luma Ramp.
- Pixel-art palettes with Original 8, Style 10 or up to ten custom colors.
- Ordered Bayer and Floyd–Steinberg dithering with spatial decay.
- Oklch color controls, Oklab gamut mapping, contour AA and an ASCII raster mode.
- English/Chinese parameter names and custom-widget captions without changing stored parameter values.

ToonPixel is a **RGB approximation**, not a 3D renderer. It does not read normals, depth, materials, object IDs or reconstruct geometry.

### Architecture

- `src/core/ToonMath.h` — color math, palettes, Oklab/Oklch conversion, dithering and alpha-safe mixing.
- `src/ae/ToonPixel.cpp` — native entry point, parameters, SmartFX/Classic branching and render orchestration.
- `src/ae/ToonPixelSettings.h` — parameter indices and defaults. The language selector is appended at index 97 so earlier indices remain stable.
- `src/ae/ToonPixelLocalization.h` — parameter names, static bilingual popup items and custom-widget text.
- `src/ae/ToonPixelStudio.h`, `ToonPixelStudioEvents.h`, `ToonPixelWidgets.h` — native custom UI, Ramp controls, Oklch wheel and ASCII editor.
- `src/ae/ToonPixelContour.h` and `ToonPixelScene.h` — contour coverage and scene luma analysis.
- `src/ae/ToonPixelColorPicker.mm` — macOS native color panel with cancel/accept lifecycle and sRGB/linear handling.
- `tools/` — build, Windows PiPL generation, PE validation and release packaging. The Adobe SDK and HostProbe are not public.

### Key decisions

- **Stable parameter identity.** New controls append at the end; old parameter indices and default English behavior remain unchanged.
- **Static popup strings.** Runtime `PF_UpdateParamUI` updates names and UI state but does not replace host-owned menu string pointers. Bilingual popup items are registered once.
- **Index-based scripting across languages.** After switching to Chinese, English-name lookups such as `effect.property("Mode")` are expected to fail. Use numeric indices or restore English.
- **Correct PiPL identity.** The released native version is `0x9601` (`0.1.2 release 1`) and matches the compiled PiPL and Windows VERSIONINFO.
- **Platform boundary is explicit.** macOS has real AE validation. Windows has a real x64 cross-build and static checks, but no Windows AE host validation yet.

### Validation summary

| Area | Evidence boundary |
|---|---|
| macOS host | AE 26.5 on Apple Silicon: bilingual round-trip, 98-parameter mapping and 99/99 real output cases passed. Intel slice exists but was not host-tested. |
| Windows x64 | Cross-built PE `.aex`; `EffectMain`, imports, exact-case PiPL ID 16000, `0x9601` and VERSIONINFO checked. No Windows AE loading/rendering test. |
| Release integrity | Downloaded Release files matched the reviewed candidate byte-for-byte. macOS bundle is ad-hoc signed and not notarized; Windows is unsigned. |
| Not covered | Continuous drag and host idle, Windows AE, Windows on Arm, other AE versions, GPU/Classic rendering, 8/16 bpc and full color management. |

### Current status

Version `0.1.2` is published as a prerelease. macOS is the validated platform. Windows remains a preview for real Windows AE validation before any production claim.

### Next steps

1. Run the Windows x64 `.aex` through AE loading, parameter UI, bilingual text and rendering tests on a disposable project.
2. Verify continuous dragging and host idle events with real native input.
3. Add more host-version coverage before claiming broader AE compatibility.
4. Preserve private validation artifacts separately; public documents should summarize evidence without exposing local paths or test data.

---

## 简体中文

本页公开总结 ToonPixel SmartFX 的开发历程。详细本地交接、验证日志和宿主工程仍保持私有；本文不包含个人路径、私有夹具、测试桥、日志或未公开媒体。

### 项目目标

构建一个 After Effects 原生 C++ 效果，将 Beauty/RGB 与 Alpha 转换为风格化二维画面：

- 通过保留色相或五档亮度色带实现三渲二。
- 提供 Original 8、Style 10 或最多十色的像素风色板。
- 支持有序 Bayer 与 Floyd–Steinberg 误差扩散抖动，并提供空间衰减。
- 提供 Oklch 调色、Oklab 色域映射、轮廓抗锯齿和 ASCII 字符栅格。
- 支持中英文参数名和自绘控件文字，同时保持参数值不变。

ToonPixel 是 **RGB 近似效果**，不是三维渲染器；它不会读取法线、深度、材质或对象 ID，也不会重建几何。

### 架构

- `src/core/ToonMath.h`：颜色数学、色板、Oklab/Oklch 转换、抖动与 Alpha 安全混色。
- `src/ae/ToonPixel.cpp`：原生入口、参数、SmartFX/Classic 分支和渲染调度。
- `src/ae/ToonPixelSettings.h`：参数索引和默认值。语言选择追加在索引 97，以保持旧索引稳定。
- `src/ae/ToonPixelLocalization.h`：参数名、静态双语菜单项和自绘控件文案。
- `src/ae/ToonPixelStudio.h`、`ToonPixelStudioEvents.h`、`ToonPixelWidgets.h`：原生自定义 UI、Ramp 控件、Oklch 色轮和 ASCII 编辑器。
- `src/ae/ToonPixelContour.h` 与 `ToonPixelScene.h`：轮廓覆盖和场景亮度分析。
- `src/ae/ToonPixelColorPicker.mm`：macOS 原生取色器，含取消/确认生命周期和 sRGB/线性处理。
- `tools/`：构建、Windows PiPL 生成、PE 校验和发布打包。Adobe SDK 与 HostProbe 不公开。

### 关键取舍

- **参数身份稳定。** 新控件只追加到末尾，旧参数索引和默认英文行为不变。
- **菜单字符串静态注册。** 运行期 `PF_UpdateParamUI` 只更新名称和 UI 状态，不替换宿主拥有的菜单字符串指针；双语菜单项在注册时一次确定。
- **跨语言脚本使用索引。** 切换中文后，`effect.property("Mode")` 等英文名称查询按预期失败，应使用参数索引或恢复英文。
- **版本身份一致。** 发布版原生版本为 `0x9601`（`0.1.2 release 1`），并与编译后的 PiPL 和 Windows VERSIONINFO 一致。
- **平台边界明确。** macOS 有真实 AE 验证；Windows 有真实 x64 交叉构建和静态检查，但尚未进行 Windows AE 宿主验证。

### 验证摘要

| 范围 | 证据边界 |
|---|---|
| macOS 宿主 | Apple Silicon 的 AE 26.5：中英文往返、98 参数映射和 99/99 真实输出通过。Intel 切片已编译但未实机验证。 |
| Windows x64 | 已交叉构建 PE `.aex`；核验 `EffectMain`、导入、精确大小写的 PiPL ID 16000、`0x9601` 和 VERSIONINFO。未做 Windows AE 加载/渲染测试。 |
| 发布完整性 | 从 GitHub 重新下载的 Release 文件与已审核候选逐字节一致。macOS 仅 ad-hoc 签名且未经公证，Windows 未签名。 |
| 未覆盖 | 连续拖动与宿主 idle、Windows AE、Windows on Arm、其他 AE 版本、GPU/Classic、8/16 bpc 和完整色彩管理。 |

### 当前状态

`0.1.2` 以预发布形式发布。macOS 是已验证平台；Windows 仍是预览，必须在真实 Windows AE 中完成验证后才能宣称生产兼容。

### 下一步

1. 在可丢弃的 Windows AE 工程中验证 x64 `.aex` 的加载、参数 UI、双语文字和渲染。
2. 使用真实原生输入验证连续拖动和宿主 idle 事件。
3. 扩大 AE 宿主版本覆盖后，再声明更广泛的兼容性。
4. 私有验证材料继续单独保存；公开文档只总结证据，不暴露本地路径或测试数据。
