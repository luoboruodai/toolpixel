#pragma once

#include "ToonMath.h"

#include <array>
#include <vector>

enum class OutputMode {
    toon_only = 1,
    pixel_only = 2,
    toon_pixel = 3,
};

struct GridRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct RenderSettings {
    int mode = static_cast<int>(OutputMode::toon_pixel);
    float strength = 1.0f;
    int pixel_size = 2;
    float dither = 0.06f;
    bool outline = true;
    float edge_threshold = 0.075f;

    int toon_model = static_cast<int>(toonpixel::ToonModel::preserve_hue);
    toonpixel::RampSettings ramp{};

    int palette_mode = static_cast<int>(toonpixel::PaletteMode::style10);
    int palette_count = 10;
    std::array<toonpixel::Rgb, 10> custom_colors = toonpixel::kStylePalette;
    int dither_mode = static_cast<int>(toonpixel::DitherMode::bayer);
    float dither_threshold = 0.5f;

    toonpixel::GradeSettings grade{};

    int pixel_outline = static_cast<int>(toonpixel::PixelOutlineMode::grid_edge);
    toonpixel::Rgb outline_color = toonpixel::kPalette[0];
    float outline_strength = 1.0f;
    int outline_width = 1;

    int dither_matrix = 4;
    toonpixel::DecaySettings decay{};
    int gamut_mode = 1;
    float gamut_margin = 0.0f;
    bool ascii_enabled = false;
    int ascii_style = 2;
    std::vector<A_u_short> ascii_ramp{32, 46, 58, 45, 61, 43, 42, 35, 37, 64};
    int ascii_selected = 0;
    int ascii_cell_size = 8;
    int ascii_drive = 1;
    int ascii_channel = 1;
    int ascii_map = 0;
    int ascii_map_mode = 2;
    float ascii_amount = 1.0f;
    int ascii_color_mode = 1;
    toonpixel::Rgb ascii_color{0.7f, 0.85f, 0.65f};
    toonpixel::Rgb ascii_color2{0.1f, 0.45f, 0.75f};
    toonpixel::Rgb ascii_background{0.008f, 0.01f, 0.018f};
    float ascii_flicker = 0.0f;
    float ascii_glitch = 0.0f;
    float ascii_aberration = 0.0f;
    int ascii_audio = 0;
    float ascii_audio_gain = 1.0f;
    float audio_level = 0.0f;
    int audio_samples = 0;
    int contour_mode = 1;
    float contour_width = 1.0f;
    int contour_aa = 2;
    int contour_source = 3;
    toonpixel::Rgb contour_background{0.25f, 0.25f, 0.25f};
    int language = 1;
    int frame = 0;

    GridRect full_rect{};

    std::vector<toonpixel::Rgb> palette() const {
        if (palette_mode == static_cast<int>(toonpixel::PaletteMode::ramp_linked)) {
            return {ramp.colors.begin(), ramp.colors.end()};
        }
        if (palette_mode == static_cast<int>(toonpixel::PaletteMode::original8)) {
            return {toonpixel::kPalette.begin(), toonpixel::kPalette.end()};
        }
        if (palette_mode == static_cast<int>(toonpixel::PaletteMode::style10)) {
            return {toonpixel::kStylePalette.begin(), toonpixel::kStylePalette.end()};
        }
        const auto count = static_cast<std::size_t>(std::clamp(palette_count, 1, 10));
        return {custom_colors.begin(), custom_colors.begin() + count};
    }
};

inline RenderSettings make_default_settings() {
    return {};
}

enum ParameterIndex {
    kInput = 0,
    kMode = 1,
    kToonStrength = 2,
    kPixelSize = 3,
    kDither = 4,
    kOutline = 5,
    kEdgeThreshold = 6,

    kToonTopic = 7,
    kToonModel = 8,
    kRampBoundary1 = 9,
    kRampBoundary2 = 10,
    kRampBoundary3 = 11,
    kRampSoftness = 12,
    kRampRepeat = 13,
    kRampColor1 = 14,
    kRampColor2 = 15,
    kRampColor3 = 16,
    kRampColor4 = 17,
    kToonTopicEnd = 18,

    kPixelTopic = 19,
    kPaletteMode = 20,
    kPaletteCount = 21,
    kDitherMode = 22,
    kDitherThreshold = 23,
    kPixelTopicEnd = 24,

    kGradeTopic = 25,
    kExposure = 26,
    kVibrance = 27,
    kHue = 28,
    kChroma = 29,
    kTintColor = 30,
    kTintAmount = 31,
    kGradeTopicEnd = 32,

    kOutlineTopic = 33,
    kPixelOutline = 34,
    kOutlineColor = 35,
    kOutlineStrength = 36,
    kOutlineWidth = 37,
    kOutlineTopicEnd = 38,

    kCustomTopic = 39,
    kCustomColor1 = 40,
    kCustomColor2 = 41,
    kCustomColor3 = 42,
    kCustomColor4 = 43,
    kCustomColor5 = 44,
    kCustomColor6 = 45,
    kCustomColor7 = 46,
    kCustomColor8 = 47,
    kCustomColor9 = 48,
    kCustomColor10 = 49,
    kCustomTopicEnd = 50,

    kWidgetTopic = 51,
    kRampBar = 52,
    kColorWheel = 53,
    kWidgetTopicEnd = 54,

    kDitherTopic = 55,
    kDitherMatrix = 56,
    kDecayShape = 57,
    kDecayAmount = 58,
    kDecayCenterX = 59,
    kDecayCenterY = 60,
    kDecayScale = 61,
    kDecayAngle = 62,
    kDitherTopicEnd = 63,

    kGamutTopic = 64,
    kGamutMode = 65,
    kGamutMargin = 66,
    kGamutTopicEnd = 67,

    kAsciiTopic = 68,
    kAsciiEnabled = 69,
    kAsciiStyle = 70,
    kAsciiRamp = 71,
    kAsciiEditor = kAsciiRamp,
    kAsciiCellSize = 72,
    kAsciiDrive = 73,
    kAsciiChannel = 74,
    kAsciiMapLayer = 75,
    kAsciiMapMode = 76,
    kAsciiAmount = 77,
    kAsciiColorMode = 78,
    kAsciiColor = 79,
    kAsciiColor2 = 80,
    kAsciiBackground = 81,
    kAsciiFlicker = 82,
    kAsciiGlitch = 83,
    kAsciiAberration = 84,
    kAsciiAudioLayer = 85,
    kAsciiAudioGain = 86,
    kAsciiTopicEnd = 87,

    kContourTopic = 88,
    kContourMode = 89,
    kContourWidth = 90,
    kContourAA = 91,
    kContourSource = 92,
    kContourBackground = 93,
    kContourTopicEnd = 94,

    kRampBoundary4 = 95,
    kRampColor5 = 96,
    kLanguage = 97,

    kAddedParamCount = kLanguage,
    kParamCount = kAddedParamCount + 1,
};

inline constexpr std::array<PF_ParamIndex, 4> kRampBoundaryParameters{
    kRampBoundary1, kRampBoundary2, kRampBoundary3, kRampBoundary4};
inline constexpr std::array<PF_ParamIndex, 5> kRampColorParameters{
    kRampColor1, kRampColor2, kRampColor3, kRampColor4, kRampColor5};
