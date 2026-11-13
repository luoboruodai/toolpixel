#pragma once

#include "ToonPixelSettings.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace localization {

enum Language {
    english = 1,
    chinese = 2,
};

enum class Text {
    preserve,
    ramp,
    automatic,
    flat,
    darkest,
    shadow,
    middle,
    light,
    highlight,
    softness,
    palette,
    original8,
    style10,
    custom,
    ramp_linked,
    dither,
    off,
    bayer,
    floyd,
    threshold,
    tint,
    exposure,
    vibrance,
    hue,
    chroma,
    ascii_hint,
};

struct ParameterName {
    PF_ParamIndex index;
    const char* english;
    const char* chinese;
};

inline constexpr ParameterName kParameterNames[] = {
    {kMode, "Mode", "模式"},
    {kToonStrength, "Toon Strength", "卡通强度"},
    {kPixelSize, "Pixel Size", "像素大小"},
    {kDither, "Dither", "抖动强度"},
    {kOutline, "Outline", "轮廓"},
    {kEdgeThreshold, "Edge Threshold", "边缘阈值"},
    {kToonModel, "Toon Model", "卡通模式"},
    {kRampBoundary1, "Ramp Boundary 1", "色带边界 1"},
    {kRampBoundary2, "Ramp Boundary 2", "色带边界 2"},
    {kRampBoundary3, "Ramp Boundary 3", "色带边界 3"},
    {kRampBoundary4, "Ramp Boundary 4", "色带边界 4"},
    {kRampSoftness, "Ramp Softness", "色带柔化"},
    {kRampRepeat, "Ramp Repeat", "色带重复"},
    {kRampColor1, "Ramp Color 1", "色带颜色 1"},
    {kRampColor2, "Ramp Color 2", "色带颜色 2"},
    {kRampColor3, "Ramp Color 3", "色带颜色 3"},
    {kRampColor4, "Ramp Color 4", "色带颜色 4"},
    {kRampColor5, "Ramp Color 5", "色带颜色 5"},
    {kPaletteMode, "Palette", "色板"},
    {kPaletteCount, "Custom Count", "自定义色数"},
    {kDitherMode, "Dither Mode", "抖动模式"},
    {kDitherThreshold, "Dither Threshold", "抖动阈值"},
    {kExposure, "Exposure", "曝光"},
    {kVibrance, "Vibrance", "鲜艳度"},
    {kHue, "Hue", "色相"},
    {kChroma, "Chroma", "彩度"},
    {kTintColor, "Tint Color", "染色颜色"},
    {kTintAmount, "Tint Amount", "染色强度"},
    {kCustomColor1, "Custom 01", "自定义 01"},
    {kCustomColor2, "Custom 02", "自定义 02"},
    {kCustomColor3, "Custom 03", "自定义 03"},
    {kCustomColor4, "Custom 04", "自定义 04"},
    {kCustomColor5, "Custom 05", "自定义 05"},
    {kCustomColor6, "Custom 06", "自定义 06"},
    {kCustomColor7, "Custom 07", "自定义 07"},
    {kCustomColor8, "Custom 08", "自定义 08"},
    {kCustomColor9, "Custom 09", "自定义 09"},
    {kCustomColor10, "Custom 10", "自定义 10"},
    {kOutlineTopic, "Outline Settings", "轮廓设置"},
    {kPixelOutline, "Pixel Outline", "像素轮廓"},
    {kOutlineColor, "Outline Color", "轮廓颜色"},
    {kOutlineStrength, "Outline Strength", "轮廓强度"},
    {kOutlineWidth, "Outline Width", "轮廓宽度"},
    {kOutlineTopicEnd, "Outline Settings End", "轮廓设置结束"},
    {kWidgetTopic, "Color Studio", "颜色工作室"},
    {kRampBar, "Luma Ramp", "亮度色带"},
    {kColorWheel, "Oklch Tint", "Oklch 染色"},
    {kDitherTopic, "Dither Decay", "抖动衰减"},
    {kDitherMatrix, "Dither Matrix", "抖动矩阵"},
    {kDecayShape, "Decay Shape", "衰减形状"},
    {kDecayAmount, "Decay Amount", "衰减强度"},
    {kDecayCenterX, "Decay Center X", "衰减中心 X"},
    {kDecayCenterY, "Decay Center Y", "衰减中心 Y"},
    {kDecayScale, "Decay Scale", "衰减尺度"},
    {kDecayAngle, "Decay Angle", "衰减角度"},
    {kDitherTopicEnd, "Dither Decay End", "抖动衰减结束"},
    {kGamutTopic, "Oklab Gamut", "Oklab 色域"},
    {kGamutMode, "Gamut Mapping", "色域映射"},
    {kGamutMargin, "Gamut Margin", "色域余量"},
    {kGamutTopicEnd, "Oklab Gamut End", "Oklab 色域结束"},
    {kAsciiTopic, "ASCII / RXXR", "ASCII / RXXR"},
    {kAsciiEnabled, "ASCII Enabled", "ASCII 启用"},
    {kAsciiStyle, "Character Style", "字符样式"},
    {kAsciiRamp, "Character Ramp", "字符序列"},
    {kAsciiCellSize, "Character Cell Size", "字符单元格"},
    {kAsciiDrive, "Character Drive", "字符驱动"},
    {kAsciiChannel, "Drive Channel", "驱动通道"},
    {kAsciiMapLayer, "Luma / Mask Map", "亮度 / 遮罩映射"},
    {kAsciiMapMode, "Map Mode", "映射模式"},
    {kAsciiAmount, "Drive Amount", "驱动强度"},
    {kAsciiColorMode, "Character Color", "字符颜色"},
    {kAsciiColor, "Character Fill", "字符填充"},
    {kAsciiColor2, "Character Gradient End", "渐变终点"},
    {kAsciiBackground, "Character Background", "字符背景"},
    {kAsciiFlicker, "Flicker", "闪烁"},
    {kAsciiGlitch, "Glitch", "故障"},
    {kAsciiAberration, "Chromatic Aberration", "色差像差"},
    {kAsciiAudioLayer, "Audio Source", "音频来源"},
    {kAsciiAudioGain, "Audio Gain", "音频增益"},
    {kAsciiTopicEnd, "ASCII / RXXR End", "ASCII / RXXR 结束"},
    {kContourTopic, "Contour & Edge AA", "轮廓与抗锯齿"},
    {kContourMode, "Contour Method", "轮廓方法"},
    {kContourWidth, "Contour Width", "轮廓搜索宽度"},
    {kContourAA, "Contour AA", "轮廓抗锯齿"},
    {kContourSource, "Contour Source", "轮廓来源"},
    {kContourBackground, "Contour Background", "轮廓背景"},
    {kContourTopicEnd, "Contour & Edge AA End", "轮廓与抗锯齿结束"},
    {kLanguage, "Language / 语言", "Language / 语言"},
};

inline const char* parameter_name(PF_ParamIndex index, int language) {
    for (const auto& entry : kParameterNames) {
        if (entry.index == index) return language == chinese ? entry.chinese : entry.english;
    }
    return nullptr;
}

inline const char* popup_items(PF_ParamIndex index, int language) {
    switch (index) {
        case kMode: return "Toon Only 仅卡通|Pixel Only 仅像素|Toon + Pixel 卡通 + 像素";
        case kToonModel: return "Preserve Hue 保留色相|Luma Ramp 亮度色带";
        case kPaletteMode: return "Original 8 原色|Style 10 风格|Custom 自定义";
        case kDitherMode: return "Off 关闭|Bayer|Floyd-Steinberg 误差扩散";
        case kPixelOutline: return "Grid Edge 网格边缘|Legacy Block 旧式像素块";
        case kDitherMatrix: return "4 x 4 Bayer|8 x 8 Bayer";
        case kDecayShape: return "Off 关闭|Linear 线性|Radial 径向";
        case kGamutMode: return "Off 关闭|Adaptive Hue-dependent 自适应色相|Chroma Clipped 彩度裁切|Truncate 截断|Debug 调试";
        case kAsciiStyle:
            return "Custom 自定义|Soft 柔和|Heavy 浓重|Binary 二进制|Hex 十六进制|Letters 字母|Blocks 像素块|Kana 假名|Pixel Graphics 像素图形|Braille 盲文|Quadrants 象限|Bars 条纹|Waveform 波形|Data Glitch 数据故障";
        case kAsciiDrive: return "Off 关闭|Audio 音频|Map 映射|Audio + Map 音频 + 映射";
        case kAsciiChannel: return "Luma 亮度|Red 红|Green 绿|Blue 蓝|Alpha";
        case kAsciiMapMode: return "Off 关闭|Luma Map 亮度映射|Mask / Alpha 遮罩";
        case kAsciiColorMode: return "Fill 填充|Source 源颜色|Gradient 渐变";
        case kContourMode: return "Luma Gradient 亮度渐变|Silhouette Coverage 轮廓覆盖";
        case kContourAA: return "Off 关闭 (1x)|Fine 精细 (2x)|High 高 (4x)";
        case kContourSource: return "Alpha|Background Color 背景颜色|Alpha + Background 背景";
        case kLanguage: return "English|中文";
        default: return nullptr;
    }
}

inline const char* checkbox_name(PF_ParamIndex index, int language) {
    if (index == kOutline || index == kAsciiEnabled) return "";
    return nullptr;
}

inline const char* text(Text id, int language) {
    const bool zh = language == chinese;
    switch (id) {
        case Text::preserve: return zh ? "保留" : "Preserve";
        case Text::ramp: return zh ? "色带" : "Ramp";
        case Text::automatic: return "AUTO";
        case Text::flat: return zh ? "均匀" : "FLAT";
        case Text::darkest: return zh ? "最暗" : "D";
        case Text::shadow: return zh ? "暗部" : "S";
        case Text::middle: return zh ? "中灰" : "M";
        case Text::light: return zh ? "亮部" : "L";
        case Text::highlight: return zh ? "最亮" : "H";
        case Text::softness: return zh ? "柔化" : "Soft";
        case Text::palette: return zh ? "色板：" : "Palette: ";
        case Text::original8: return zh ? "原色 8" : "Original 8";
        case Text::style10: return zh ? "风格 10" : "Style 10";
        case Text::custom: return zh ? "自定义" : "Custom";
        case Text::ramp_linked: return zh ? "色带联动" : "Ramp Linked";
        case Text::dither: return zh ? "抖动：" : "Dither: ";
        case Text::off: return zh ? "关闭" : "Off";
        case Text::bayer: return "Bayer";
        case Text::floyd: return zh ? "误差扩散" : "Floyd";
        case Text::threshold: return zh ? "阈值" : "Threshold";
        case Text::tint: return zh ? "染色" : "Tint";
        case Text::exposure: return zh ? "曝光" : "Exp.";
        case Text::vibrance: return zh ? "鲜艳" : "Vibr.";
        case Text::hue: return zh ? "色相" : "Hue";
        case Text::chroma: return zh ? "彩度" : "Chroma";
        case Text::ascii_hint: return zh ? "点击输入；Del 删除" : "Click, type; Del removes";
    }
    return "";
}

inline std::u16string utf16_from_utf8(std::string_view input) {
    std::u16string output;
    output.reserve(input.size());
    std::size_t offset = 0;
    while (offset < input.size()) {
        const auto first = static_cast<std::uint8_t>(input[offset]);
        std::uint32_t codepoint = 0xFFFD;
        std::size_t length = 1;
        if (first < 0x80) {
            codepoint = first;
        } else {
            int needed = 0;
            std::uint32_t value = 0;
            std::uint32_t minimum = 0;
            if ((first & 0xE0) == 0xC0) {
                needed = 1;
                value = first & 0x1F;
                minimum = 0x80;
            } else if ((first & 0xF0) == 0xE0) {
                needed = 2;
                value = first & 0x0F;
                minimum = 0x800;
            } else if ((first & 0xF8) == 0xF0) {
                needed = 3;
                value = first & 0x07;
                minimum = 0x10000;
            }
            if (needed && offset + needed < input.size()) {
                bool valid = true;
                for (int index = 1; index <= needed; ++index) {
                    const auto next = static_cast<std::uint8_t>(input[offset + index]);
                    if ((next & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                    value = (value << 6) | (next & 0x3F);
                }
                if (valid && value >= minimum && value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF)) {
                    codepoint = value;
                    length = static_cast<std::size_t>(needed + 1);
                }
            }
        }
        if (codepoint <= 0xFFFF) {
            output.push_back(static_cast<char16_t>(codepoint));
        } else {
            codepoint -= 0x10000;
            output.push_back(static_cast<char16_t>(0xD800 + (codepoint >> 10)));
            output.push_back(static_cast<char16_t>(0xDC00 + (codepoint & 0x3FF)));
        }
        offset += length;
    }
    return output;
}

}  // namespace localization
