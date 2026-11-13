# Install ToonPixel 0.1.2 on Windows / Windows 安装指南

## English

**Preview:** this x64 `.aex` was cross-built with MinGW-w64 and statically checked. It has not been loaded or rendered in Windows After Effects. Test on a disposable project before considering deployment.

1. **Quit After Effects completely** and save a backup of your project.
2. Download `ToonPixel-v0.1.2-Windows-x64.zip` and verify its SHA-256 against `SHA256SUMS.txt` from Release v0.1.2.
3. Extract `ToonPixel.aex`. Keep it separate from any other plug-in binary.
4. Back up any earlier **ToonPixel** copy, then copy `ToonPixel.aex` to:
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\ToonPixel.aex`
   Administrator permission may be required.
5. Restart AE on a test project and choose **Effect → ToonPixel → ToonPixel SmartFX**. Check both the parameter list and the **Language / 语言** selector before rendering. Native menu items remain bilingual.
6. If AE cannot scan or load the plug-in, quit AE and remove this `.aex` to restore the previous state. Record the exact AE version, Windows version, installation path and crash report; the path in a scan dialog does not by itself prove which module failed.

This is a **Windows x64** build, not a Windows on Arm native build. A successful PE/resource check is not an AE compatibility test. Other AE versions, non-ASCII project paths, IME, continuous drag, GPU/Classic rendering, 8/16 bpc and full color management still require host validation.

**Security:** the DLL has no Authenticode signature. A hash proves file identity, not publisher identity or code-signing trust. Do not bypass your organization's security policy.

## 简体中文

**预发布说明：**此 x64 `.aex` 使用 MinGW-w64 交叉构建并通过静态检查，尚未在 Windows After Effects 中加载或渲染。请先在可丢弃的测试工程中验证，不要直接用于生产环境。

1. **完全退出 After Effects** 并备份工程。
2. 下载 `ToonPixel-v0.1.2-Windows-x64.zip`，用 Release v0.1.2 的 `SHA256SUMS.txt` 核对 SHA-256。
3. 解压得到 `ToonPixel.aex`，与其他插件文件分开保存。
4. 备份旧 **ToonPixel** 文件，再将 `ToonPixel.aex` 复制到：
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\ToonPixel.aex`
   可能需要管理员权限。
5. 在测试工程中重启 AE，选择 **效果 → ToonPixel → ToonPixel SmartFX**。先检查参数列表和 **Language / 语言** 下拉，再渲染。原生菜单项保持双语。
6. 若 AE 扫描或加载插件失败，退出 AE 并移除此 `.aex` 恢复旧状态。记录具体 AE 版本、Windows 版本、安装路径和崩溃报告；扫描提示中的路径不能单独证明是哪一个模块出错。

本附件仅适用于 **Windows x64**，不是 Windows on Arm 原生版本。PE/资源检查通过不等于 AE 兼容测试通过。其他 AE 版本、非 ASCII 工程路径、IME、连续拖动、GPU/Classic、8/16 bpc 和完整色彩管理仍需实机验证。

**安全说明：**DLL 没有 Authenticode 签名。哈希只能证明文件身份，不能证明发布者身份或签名信任。请勿绕过所在组织的安全策略。
