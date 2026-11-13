# Install ToonPixel 0.1.2 on macOS / macOS 安装指南

## English

1. **Quit After Effects completely** and save a backup of your project. Replacing a plug-in while AE has it loaded can leave the old code in memory.
2. Download `ToonPixel-v0.1.2-macOS-universal.zip` and verify its SHA-256 against `SHA256SUMS.txt` from Release v0.1.2.
3. Extract the ZIP. Preserve the entire `ToonPixel.plugin` bundle, including `Contents/Info.plist`, `Contents/PkgInfo` and `Contents/Resources/ToonPixel.rsrc`.
4. Back up any older **ToonPixel** bundle, then copy the new bundle to:
   `~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/ToonPixel/ToonPixel.plugin`
5. Restart AE. On a test layer, choose **Effect → ToonPixel → ToonPixel SmartFX**. The default language is English; use **Language / 语言** to switch parameter names and custom-widget captions to Chinese.
6. To remove it, quit AE and delete only this ToonPixel bundle. Leave unrelated effects untouched during this upgrade.

The bundle contains `arm64` and `x86_64` code and targets macOS 11 or later. Only AE 26.5 on Apple Silicon has been exercised: bilingual switching and 99/99 real output cases passed. Intel, other AE versions, GPU/Classic rendering and full color management are not covered by that result.

**Security:** the bundle is ad-hoc signed and not notarized. A hash proves file identity, not publisher identity or notarization. Do not bypass your organization's security policy.

## 简体中文

1. **完全退出 After Effects** 并备份工程。不要在 AE 已加载插件时覆盖运行中的二进制。
2. 下载 `ToonPixel-v0.1.2-macOS-universal.zip`，用 Release v0.1.2 的 `SHA256SUMS.txt` 核对 SHA-256。
3. 解压后保留完整的 `ToonPixel.plugin` 目录，包括 `Contents/Info.plist`、`Contents/PkgInfo` 和 `Contents/Resources/ToonPixel.rsrc`。
4. 备份旧 **ToonPixel** 文件，然后将新目录复制到：
   `~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/ToonPixel/ToonPixel.plugin`
5. 重启 AE，在测试图层上选择 **效果 → ToonPixel → ToonPixel SmartFX**。默认英文；通过 **Language / 语言** 将参数名与自绘控件文字切换为中文。
6. 卸载时先退出 AE，再只删除这个 ToonPixel 目录。不要改动其他无关效果。

插件包含 `arm64` 与 `x86_64`，目标为 macOS 11 或更新版本。目前仅在 Apple Silicon 的 AE 26.5 完成实测：中英文切换与 99/99 真实输出通过。Intel、其他 AE 版本、GPU/Classic 和完整色彩管理未被该结果覆盖。

**安全说明：**插件仅有 ad-hoc 签名，未经公证。哈希只能证明文件身份，不能证明发布者身份或公证状态。请勿绕过所在组织的安全策略。
