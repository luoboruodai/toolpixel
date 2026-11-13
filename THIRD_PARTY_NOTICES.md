# Third-party notices

## Adobe After Effects SDK

ToonPixel 使用 Adobe After Effects 的原生效果接口。SDK 头文件、资源与工具分别受 Adobe 的适用条款约束，不是本项目原创代码。当前开发 pkg 不分发 SDK 头文件、资源、Examples 或其他 SDK 文件。

本地 `vendor/ae-sdk/` 仅是可构建 CPU 路径的最小集合，不构成完整 SDK 或再分发许可证明。公开源码或单独分发 SDK 内容之前，需要另行核实 Adobe 的许可条件。

After Effects 是 Adobe 的产品名称。本项目未声称与 Adobe 存在隶属、认证或背书关系。

## 参考插件和素材

RetroDither、Pixelate、CR-LumaRamp、VC Color Vibrance、CHR0MA、RXXR 及用户提供的视频仅作本地功能方向参考。没有安装或复制这些插件的二进制、私有代码、字体、色板文件或视频资产到 ToonPixel 产品中，也不声称算法等同。

测试场景、原生插件源码、数学实现、色板常量和控制素材在本项目中生成。Oklab/Oklch 用于颜色计算；数学公式本身不等于取得任何第三方插件的界面或资源许可。

## 字符字形

`fixtures/generated/font8x8_basic.h` 及其中由 `src/ae/ToonPixelGlyphs.h` 使用的 Basic Latin 8×8 字形来自 Daniel Hepper 的 `font8x8_basic.h`，基于公共领域 VGA 字形。上游声明为公共领域：
https://github.com/dhepper/font8x8/blob/master/font8x8_basic.h

其他项目生成的字符/图形样式不冒充参考插件的私有字体资源。完整 Unicode、IME 和非 BMP 支持不因该 Basic Latin 数据而自动具备。

## 产品包边界

开发 pkg 只包含 `ToonPixel.plugin`。以下项目不属于产品载荷：

- `ToonPixelHostProbe` 和任何 JSX 测试桥。
- AE 测试工程、EXR、PSD、预览和验证日志。
- 用户附件、私有参考资料、备份和构建缓存。
- Adobe SDK、第三方参考插件及其资源。

未附带的组件不代表取得了额外许可。公开发布前仍需独立审查源码、SDK、素材、签名和分发条款。
