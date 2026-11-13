#define PF_DEEP_COLOR_AWARE 1

#if defined(__APPLE__) && defined(__MACH__)
#include <CoreServices/CoreServices.h>
#include <os/log.h>
#else
#include "ToonPixelLegacyTypes.h"
#endif
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_Macros.h"
#include "AE_EffectVers.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_EffectPixelFormat.h"
#include "SPBasic.h"
#include "Param_Utils.h"
#include "ToonMath.h"
#include "ToonPixelSettings.h"
#include "ToonPixelLocalization.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <utility>

#ifndef TOONPIXEL_DEBUG_LOG
#define TOONPIXEL_DEBUG_LOG 0
#endif

namespace {

#if TOONPIXEL_DEBUG_LOG
void debug_log(const char* format, ...) {
    char message[2048];
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (length < 0) {
        return;
    }
#if defined(__APPLE__) && defined(__MACH__)
    os_log(OS_LOG_DEFAULT, "ToonPixel %{public}s", message);
#endif
    const char* path = std::getenv("TOONPIXEL_DEBUG_LOG_PATH");
    if (path && path[0]) {
        std::FILE* file = std::fopen(path, "a");
        if (file) {
            std::fputs(message, file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }
    std::fprintf(stderr, "ToonPixel %s\n", message);
    std::fflush(stderr);
}
#define TOONPIXEL_LOG(...) debug_log(__VA_ARGS__)
#else
#define TOONPIXEL_LOG(...) ((void)0)
#endif

PF_Err add_param_traced(PF_InData* in_data, PF_ParamIndex index, PF_ParamDefPtr definition) {
    const PF_Err error = in_data->inter.add_param(in_data->effect_ref, index, definition);
    if (error) {
        TOONPIXEL_LOG("add_param_error=%ld type=%d id=%ld index=%ld name=%s",
            static_cast<long>(error), static_cast<int>(definition->param_type),
            static_cast<long>(definition->uu.id), static_cast<long>(index), definition->name);
    }
    return error;
}
#undef PF_ADD_PARAM
#define PF_ADD_PARAM(IN_DATA, INDEX, DEFINITION) add_param_traced((IN_DATA), (INDEX), (DEFINITION))

struct FloatPixel {
    float alpha;
    float red;
    float green;
    float blue;
};

template <typename Pixel>
struct PixelTraits;

template <>
struct PixelTraits<PF_Pixel8> {
    static FloatPixel read(const PF_Pixel8& pixel) {
        constexpr float scale = 1.0f / 255.0f;
        return {pixel.alpha * scale, pixel.red * scale, pixel.green * scale, pixel.blue * scale};
    }

    static PF_Pixel8 write(const FloatPixel& pixel) {
        const auto quantize = [](float value) {
            const float clamped = std::max(0.0f, std::min(1.0f, value));
            return static_cast<A_u_char>(std::lround(clamped * 255.0f));
        };
        return {quantize(pixel.alpha), quantize(pixel.red), quantize(pixel.green), quantize(pixel.blue)};
    }
};

template <>
struct PixelTraits<PF_Pixel16> {
    static FloatPixel read(const PF_Pixel16& pixel) {
        constexpr float scale = 1.0f / 32768.0f;
        return {pixel.alpha * scale, pixel.red * scale, pixel.green * scale, pixel.blue * scale};
    }

    static PF_Pixel16 write(const FloatPixel& pixel) {
        const auto quantize = [](float value) {
            const float clamped = std::max(0.0f, std::min(1.0f, value));
            return static_cast<A_u_short>(std::lround(clamped * 32768.0f));
        };
        return {quantize(pixel.alpha), quantize(pixel.red), quantize(pixel.green), quantize(pixel.blue)};
    }
};

template <>
struct PixelTraits<PF_PixelFloat> {
    static FloatPixel read(const PF_PixelFloat& pixel) {
        return {pixel.alpha, pixel.red, pixel.green, pixel.blue};
    }

    static PF_PixelFloat write(const FloatPixel& pixel) {
        return {pixel.alpha, pixel.red, pixel.green, pixel.blue};
    }
};

PF_LRect world_rect(const PF_EffectWorld* world) {
    return {world->origin_x, world->origin_y, world->origin_x + world->width, world->origin_y + world->height};
}

struct StraightPixel {
    float alpha;
    float red;
    float green;
    float blue;
};

template <typename Pixel>
StraightPixel sample_straight(const PF_EffectWorld* world, A_long x, A_long y) {
    const PF_LRect bounds = world_rect(world);
    x = std::clamp(x, bounds.left, bounds.right - 1);
    y = std::clamp(y, bounds.top, bounds.bottom - 1);
    const auto* source = reinterpret_cast<const Pixel*>(
        reinterpret_cast<const unsigned char*>(world->data) + (y - world->origin_y) * world->rowbytes);
    const FloatPixel pixel = PixelTraits<Pixel>::read(source[x - world->origin_x]);
    return {
        pixel.alpha,
        toonpixel::decode_unmanaged(pixel.red),
        toonpixel::decode_unmanaged(pixel.green),
        toonpixel::decode_unmanaged(pixel.blue),
    };
}

FloatPixel encode_pixel(const StraightPixel& pixel) {
    if (pixel.alpha == 0.0f) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return {
        pixel.alpha,
        toonpixel::encode_unmanaged(pixel.red),
        toonpixel::encode_unmanaged(pixel.green),
        toonpixel::encode_unmanaged(pixel.blue),
    };
}

#include "ToonPixelArb.h"
#include "ToonPixelGlyphs.h"
#include "ToonPixelContour.h"
#include "ToonPixelScene.h"
#include "ToonPixelStyle.h"

bool intersect_rect(PF_LRect& target, const PF_LRect& other) {
    target.left = std::max(target.left, other.left);
    target.top = std::max(target.top, other.top);
    target.right = std::min(target.right, other.right);
    target.bottom = std::min(target.bottom, other.bottom);
    return target.left < target.right && target.top < target.bottom;
}

template <typename Pixel>
PF_Err render_typed(
    const PF_EffectWorld* input,
    PF_EffectWorld* output,
    const PF_LRect& area,
    bool preserve_zero_alpha,
    PF_ChannelMask channel_mask,
    const RenderSettings& settings,
    const PF_EffectWorld* map = nullptr) {
    return style::render_typed<Pixel>(input, output, area, preserve_zero_alpha, channel_mask, settings, map);
}

class WorldSuiteHandle {
public:
    explicit WorldSuiteHandle(PF_InData* in_data) : basic_(in_data->pica_basicP) {
        if (basic_) {
            error_ = basic_->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, reinterpret_cast<const void**>(&suite_));
        } else {
            error_ = PF_Err_INVALID_CALLBACK;
        }
    }

    ~WorldSuiteHandle() {
        if (basic_ && suite_) {
            basic_->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);
        }
    }

    PF_Err error() const {
        return error_;
    }

    PF_Err format(const PF_EffectWorld* world, PF_PixelFormat& pixel_format) const {
        if (!suite_) {
            return error_;
        }
        return suite_->PF_GetPixelFormat(world, &pixel_format);
    }

private:
    const SPBasicSuite* basic_ = nullptr;
    const PF_WorldSuite2* suite_ = nullptr;
    PF_Err error_ = PF_Err_NONE;
};

class ParamUtilsHandle {
public:
    explicit ParamUtilsHandle(PF_InData* in_data) : basic_(in_data ? in_data->pica_basicP : nullptr) {
        if (basic_) {
            error_ = basic_->AcquireSuite(kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3,
                reinterpret_cast<const void**>(&suite_));
        } else {
            error_ = PF_Err_INVALID_CALLBACK;
        }
    }

    ~ParamUtilsHandle() {
        if (basic_ && suite_) {
            basic_->ReleaseSuite(kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
        }
    }

    PF_Err error() const {
        return error_;
    }

    const PF_ParamUtilsSuite3* suite() const {
        return suite_;
    }

private:
    const SPBasicSuite* basic_ = nullptr;
    const PF_ParamUtilsSuite3* suite_ = nullptr;
    PF_Err error_ = PF_Err_NONE;
};

PF_Err read_settings(PF_InData* in_data, RenderSettings& settings,
    const PF_ParamDef* const direct_params[] = nullptr) {
    if (!in_data || (!direct_params && (!in_data->inter.checkout_param || !in_data->inter.checkin_param))) {
        return PF_Err_INVALID_CALLBACK;
    }
    settings = make_default_settings();
    PF_Err error = PF_Err_NONE;
    auto checkout = [&](PF_ParamIndex index, PF_ParamDef& parameter) -> PF_Err {
        if (direct_params) {
            if (index < kInput || index >= kParamCount || !direct_params[index]) {
                return PF_Err_BAD_CALLBACK_PARAM;
            }
            parameter = *direct_params[index];
            return PF_Err_NONE;
        }
        parameter = PF_ParamDef{};
        const PF_Err checkout_error = PF_CHECKOUT_PARAM(in_data, index, in_data->current_time,
            in_data->time_step, in_data->time_scale, &parameter);
        return checkout_error ? checkout_error : PF_CHECKIN_PARAM(in_data, &parameter);
    };
    auto read_popup = [&](PF_ParamIndex index, int minimum, int maximum, int& target) {
        if (error) {
            return;
        }
        PF_ParamDef parameter{};
        error = checkout(index, parameter);
        if (!error) {
            target = std::clamp(static_cast<int>(parameter.u.pd.value), minimum, maximum);
        }
    };
    auto read_float = [&](PF_ParamIndex index, float minimum, float maximum, float& target) {
        if (error) {
            return;
        }
        PF_ParamDef parameter{};
        error = checkout(index, parameter);
        if (!error) {
            target = std::clamp(static_cast<float>(parameter.u.fs_d.value), minimum, maximum);
        }
    };
    auto read_int = [&](PF_ParamIndex index, int minimum, int maximum, int& target) {
        if (error) {
            return;
        }
        PF_ParamDef parameter{};
        error = checkout(index, parameter);
        if (!error) {
            target = std::clamp(static_cast<int>(std::lround(parameter.u.fs_d.value)), minimum, maximum);
        }
    };
    auto read_bool = [&](PF_ParamIndex index, bool& target) {
        if (error) {
            return;
        }
        PF_ParamDef parameter{};
        error = checkout(index, parameter);
        if (!error) {
            target = parameter.u.bd.value != 0;
        }
    };
    auto read_color = [&](PF_ParamIndex index, toonpixel::Rgb& target) {
        if (error) {
            return;
        }
        PF_ParamDef parameter{};
        error = checkout(index, parameter);
        if (!error) {
            target = {
                toonpixel::decode_unmanaged(parameter.u.cd.value.red / 255.0f),
                toonpixel::decode_unmanaged(parameter.u.cd.value.green / 255.0f),
                toonpixel::decode_unmanaged(parameter.u.cd.value.blue / 255.0f),
            };
        }
    };
    read_popup(kLanguage, 1, 2, settings.language);
    read_popup(kMode, 1, 3, settings.mode);
    read_float(kToonStrength, 0.0f, 1.0f, settings.strength);
    read_int(kPixelSize, 1, 64, settings.pixel_size);
    read_float(kDither, 0.0f, 1.0f, settings.dither);
    read_bool(kOutline, settings.outline);
    read_float(kEdgeThreshold, 0.0f, 1.0f, settings.edge_threshold);

    read_popup(kToonModel, 1, 2, settings.toon_model);
    for (std::size_t index = 0; index < settings.ramp.boundaries.size(); ++index) {
        read_float(kRampBoundaryParameters[index], 0.0f, 1.0f, settings.ramp.boundaries[index]);
    }
    read_float(kRampSoftness, 0.0f, 0.5f, settings.ramp.softness);
    read_int(kRampRepeat, 1, 8, settings.ramp.repeat);
    for (std::size_t index = 0; index < settings.ramp.colors.size(); ++index) {
        read_color(kRampColorParameters[index], settings.ramp.colors[index]);
    }

    read_popup(kPaletteMode, 1, 3, settings.palette_mode);
    read_int(kPaletteCount, 1, 10, settings.palette_count);
    read_popup(kDitherMode, 1, 3, settings.dither_mode);
    read_float(kDitherThreshold, 0.0f, 1.0f, settings.dither_threshold);

    read_float(kExposure, -2.0f, 2.0f, settings.grade.exposure);
    read_float(kVibrance, -1.0f, 1.0f, settings.grade.vibrance);
    read_float(kHue, -180.0f, 180.0f, settings.grade.hue);
    read_float(kChroma, 0.0f, 2.0f, settings.grade.chroma);
    read_color(kTintColor, settings.grade.tint);
    read_float(kTintAmount, 0.0f, 1.0f, settings.grade.tint_amount);

    read_popup(kPixelOutline, 1, 2, settings.pixel_outline);
    read_color(kOutlineColor, settings.outline_color);
    read_float(kOutlineStrength, 0.0f, 1.0f, settings.outline_strength);
    read_int(kOutlineWidth, 1, 4, settings.outline_width);
    for (std::size_t index = 0; index < settings.custom_colors.size(); ++index) {
        read_color(static_cast<PF_ParamIndex>(kCustomColor1 + index), settings.custom_colors[index]);
    }
    read_popup(kDitherMatrix, 1, 2, settings.dither_matrix);
    read_popup(kDecayShape, 1, 3, settings.decay.shape);
    read_float(kDecayAmount, 0.0f, 1.0f, settings.decay.amount);
    read_float(kDecayCenterX, 0.0f, 1.0f, settings.decay.center_x);
    read_float(kDecayCenterY, 0.0f, 1.0f, settings.decay.center_y);
    read_float(kDecayScale, 0.01f, 4.0f, settings.decay.scale);
    read_float(kDecayAngle, -180.0f, 180.0f, settings.decay.angle);
    read_popup(kGamutMode, 1, 5, settings.gamut_mode);
    read_float(kGamutMargin, 0.0f, 0.25f, settings.gamut_margin);
    read_bool(kAsciiEnabled, settings.ascii_enabled);
    read_popup(kAsciiStyle, 1, 14, settings.ascii_style);
    read_int(kAsciiCellSize, 4, 32, settings.ascii_cell_size);
    read_popup(kAsciiDrive, 1, 4, settings.ascii_drive);
    read_popup(kAsciiChannel, 1, 5, settings.ascii_channel);
    settings.ascii_map = kAsciiMapLayer;
    read_popup(kAsciiMapMode, 1, 3, settings.ascii_map_mode);
    read_float(kAsciiAmount, 0.0f, 2.0f, settings.ascii_amount);
    read_popup(kAsciiColorMode, 1, 3, settings.ascii_color_mode);
    read_color(kAsciiColor, settings.ascii_color);
    read_color(kAsciiColor2, settings.ascii_color2);
    read_color(kAsciiBackground, settings.ascii_background);
    read_float(kAsciiFlicker, 0.0f, 1.0f, settings.ascii_flicker);
    read_float(kAsciiGlitch, 0.0f, 1.0f, settings.ascii_glitch);
    read_float(kAsciiAberration, 0.0f, 4.0f, settings.ascii_aberration);
    settings.ascii_audio = kAsciiAudioLayer;
    read_float(kAsciiAudioGain, 0.0f, 8.0f, settings.ascii_audio_gain);
    read_popup(kContourMode, 1, 2, settings.contour_mode);
    read_float(kContourWidth, 0.25f, 4.0f, settings.contour_width);
    read_popup(kContourAA, 1, 3, settings.contour_aa);
    read_popup(kContourSource, 1, 3, settings.contour_source);
    read_color(kContourBackground, settings.contour_background);
    if (settings.mode != static_cast<int>(OutputMode::pixel_only) &&
        settings.toon_model == static_cast<int>(toonpixel::ToonModel::luma_ramp) &&
        settings.palette_mode == static_cast<int>(toonpixel::PaletteMode::style10)) {
        settings.palette_mode = static_cast<int>(toonpixel::PaletteMode::ramp_linked);
    }
    settings.dither_matrix = settings.dither_matrix == 1 ? 4 : 8;
    if (direct_params && direct_params[kAsciiRamp]) {
        settings.ascii_ramp = arbitrary::codepoints(direct_params[kAsciiRamp]->u.arb_d.value);
        const auto* ramp = arbitrary::get(direct_params[kAsciiRamp]->u.arb_d.value);
        if (arbitrary::validate(ramp) == PF_Err_NONE) settings.ascii_selected = ramp->selected;
    } else {
        PF_ParamDef parameter{};
        if (!error) error = checkout(kAsciiRamp, parameter);
        if (!error) settings.ascii_ramp = arbitrary::codepoints(parameter.u.arb_d.value);
    }
    settings.frame = in_data->time_step > 0 ? static_cast<int>(in_data->current_time / in_data->time_step) : 0;
    TOONPIXEL_LOG(
        "read_settings error=%ld mode=%d strength=%.6f pixel=%d palette=%d count=%d dither_mode=%d dither=%.6f threshold=%.6f toon_model=%d repeat=%d outline=%d edge=%.6f hue=%.6f time=%ld step=%ld scale=%lu frame=%d",
        static_cast<long>(error), settings.mode, settings.strength, settings.pixel_size,
        settings.palette_mode, settings.palette_count, settings.dither_mode,
        settings.dither, settings.dither_threshold, settings.toon_model, settings.ramp.repeat,
        settings.outline ? 1 : 0, settings.edge_threshold, settings.grade.hue,
        static_cast<long>(in_data->current_time), static_cast<long>(in_data->time_step),
        static_cast<unsigned long>(in_data->time_scale), settings.frame);
    TOONPIXEL_LOG("ramp_settings bounds=%.9f,%.9f,%.9f,%.9f gamut=%d margin=%.6f contour_source=%d",
        settings.ramp.boundaries[0], settings.ramp.boundaries[1], settings.ramp.boundaries[2],
        settings.ramp.boundaries[3], settings.gamut_mode, settings.gamut_margin, settings.contour_source);
    TOONPIXEL_LOG("ascii_settings enabled=%d style=%d ramp_size=%zu first=%u last=%u drive=%d map_mode=%d channel=%d",
        settings.ascii_enabled ? 1 : 0, settings.ascii_style, settings.ascii_ramp.size(),
        settings.ascii_ramp.empty() ? 0U : static_cast<unsigned>(settings.ascii_ramp.front()),
        settings.ascii_ramp.empty() ? 0U : static_cast<unsigned>(settings.ascii_ramp.back()),
        settings.ascii_drive, settings.ascii_map_mode, settings.ascii_channel);
    TOONPIXEL_LOG("decay_settings shape=%d amount=%.6f x=%.6f y=%.6f scale=%.6f angle=%.6f",
        settings.decay.shape, settings.decay.amount, settings.decay.center_x,
        settings.decay.center_y, settings.decay.scale, settings.decay.angle);
    TOONPIXEL_LOG("tint_settings rgb=%.6f,%.6f,%.6f amount=%.6f",
        settings.grade.tint.red, settings.grade.tint.green, settings.grade.tint.blue, settings.grade.tint_amount);
    return error;
}

#include "ToonPixelWidgets.h"

PF_Err update_params_ui(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[]) {
    if (!params) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    RenderSettings settings;
    PF_Err error = read_settings(in_data, settings, params);
    if (error) {
        return error;
    }
    ParamUtilsHandle suites(in_data);
    error = suites.error();
    if (error) {
        return error;
    }
    for (PF_ParamIndex index = kMode; index < kParamCount; ++index) {
        if (!params[index]) {
            if (!error) error = PF_Err_BAD_CALLBACK_PARAM;
            continue;
        }
        if (params[index]->param_type == PF_Param_NO_DATA) continue;
        if (const char* name = localization::parameter_name(index, settings.language)) {
            PF_STRCPY(params[index]->name, name);
        }
        const PF_Err update_error = suites.suite()->PF_UpdateParamUI(in_data->effect_ref, index, params[index]);
        if (!error && update_error) error = update_error;
    }
    TOONPIXEL_LOG("ui_language value=%d", settings.language);
    const bool toon_enabled = settings.mode != static_cast<int>(OutputMode::pixel_only);
    const bool pixel_enabled = settings.mode != static_cast<int>(OutputMode::toon_only);
    const bool ramp_enabled = toon_enabled;
    const bool custom_enabled = pixel_enabled && settings.palette_mode == static_cast<int>(toonpixel::PaletteMode::custom);
    const bool dither_enabled = pixel_enabled && settings.dither_mode != static_cast<int>(toonpixel::DitherMode::off);
    const bool decay_enabled = dither_enabled && settings.decay.shape != 1;
    const bool contour_enabled = toon_enabled && settings.contour_mode == 2;
    const bool map_enabled = settings.ascii_enabled && settings.ascii_map_mode != 1 &&
        (settings.ascii_drive == 3 || settings.ascii_drive == 4);
    const bool audio_enabled = settings.ascii_enabled && (settings.ascii_drive == 2 || settings.ascii_drive == 4);
    auto enable = [&](PF_ParamIndex index, bool enabled) {
        if (!params[index]) {
            if (!error) {
                error = PF_Err_BAD_CALLBACK_PARAM;
            }
            return;
        }
        if (enabled) {
            params[index]->ui_flags &= ~PF_PUI_DISABLED;
        } else {
            params[index]->ui_flags |= PF_PUI_DISABLED;
        }
        if (index == kRampBar || index == kColorWheel || index == kAsciiEditor) {
            TOONPIXEL_LOG("ui_control index=%ld type=%d flags=%x width=%d height=%d enabled=%d",
                static_cast<long>(index), static_cast<int>(params[index]->param_type), params[index]->ui_flags,
                params[index]->ui_width, params[index]->ui_height, enabled ? 1 : 0);
        }
        const PF_Err update_error = suites.suite()->PF_UpdateParamUI(in_data->effect_ref, index, params[index]);
        if (!error && update_error) {
            error = update_error;
        }
    };
    for (PF_ParamIndex index = kMode; index < kParamCount; ++index) {
        switch (index) {
            case kToonTopic:
            case kToonTopicEnd:
            case kPixelTopic:
            case kPixelTopicEnd:
            case kGradeTopic:
            case kGradeTopicEnd:
            case kOutlineTopic:
            case kOutlineTopicEnd:
            case kCustomTopic:
            case kCustomTopicEnd:
            case kWidgetTopic:
            case kWidgetTopicEnd:
            case kRampBar:
            case kColorWheel:
            case kDitherTopic:
            case kDitherTopicEnd:
            case kGamutTopic:
            case kGamutTopicEnd:
            case kAsciiTopic:
            case kAsciiTopicEnd:
            case kAsciiRamp:
            case kContourTopic:
            case kContourTopicEnd:
                continue;
            default:
                break;
        }
        switch (index) {
            case kToonStrength:
            case kOutline:
            case kEdgeThreshold:
            case kToonModel:
            case kOutlineColor:
            case kOutlineStrength:
                enable(index, toon_enabled);
                break;
            case kPixelSize:
                enable(index, pixel_enabled);
                break;
            case kDither:
            case kDitherThreshold:
                enable(index, dither_enabled);
                break;
            case kRampBoundary1:
            case kRampBoundary2:
            case kRampBoundary3:
            case kRampSoftness:
            case kRampRepeat:
            case kRampColor1:
            case kRampColor2:
            case kRampColor3:
            case kRampColor4:
                enable(index, ramp_enabled);
                break;
            case kPaletteMode:
            case kDitherMode:
                enable(index, pixel_enabled);
                break;
            case kPaletteCount:
                enable(index, custom_enabled);
                break;
            case kPixelOutline:
                enable(index, toon_enabled && pixel_enabled && settings.contour_mode == 1);
                break;
            case kOutlineWidth:
                enable(index, toon_enabled && settings.contour_mode == 1);
                break;
            case kDitherMatrix:
                enable(index, pixel_enabled && settings.dither_mode == static_cast<int>(toonpixel::DitherMode::bayer));
                break;
            case kDecayShape:
                enable(index, dither_enabled);
                break;
            case kDecayAmount:
            case kDecayCenterX:
            case kDecayCenterY:
            case kDecayScale:
                enable(index, decay_enabled);
                break;
            case kDecayAngle:
                enable(index, decay_enabled && settings.decay.shape == 2);
                break;
            case kGamutMargin:
                enable(index, settings.gamut_mode >= 2 && settings.gamut_mode <= 4);
                break;
            case kAsciiStyle:
            case kAsciiCellSize:
            case kAsciiDrive:
            case kAsciiAmount:
            case kAsciiColorMode:
            case kAsciiFlicker:
            case kAsciiGlitch:
            case kAsciiAberration:
                enable(index, settings.ascii_enabled);
                break;
            case kAsciiChannel:
                enable(index, map_enabled && settings.ascii_map_mode == 2);
                break;
            case kAsciiMapLayer:
            case kAsciiMapMode:
                enable(index, settings.ascii_enabled && (settings.ascii_drive == 3 || settings.ascii_drive == 4));
                break;
            case kAsciiColor:
            case kAsciiBackground:
                enable(index, settings.ascii_enabled);
                break;
            case kAsciiColor2:
                enable(index, settings.ascii_enabled && settings.ascii_color_mode == 3);
                break;
            case kAsciiAudioLayer:
            case kAsciiAudioGain:
                enable(index, audio_enabled);
                break;
            case kContourWidth:
                enable(index, contour_enabled);
                break;
            case kContourMode:
            case kContourAA:
                enable(index, toon_enabled);
                break;
            case kContourSource:
                enable(index, contour_enabled);
                break;
            case kContourBackground:
                enable(index, contour_enabled && settings.contour_source != 1);
                break;
            default:
                if (index >= kCustomColor1 && index <= kCustomColor10) {
                    enable(index, custom_enabled);
                } else {
                    enable(index, true);
                }
                break;
        }
    }
    enable(kRampBar, ramp_enabled);
    enable(kColorWheel, true);
    enable(kAsciiEditor, settings.ascii_enabled);
    TOONPIXEL_LOG("update_params_ui error=%ld", static_cast<long>(error));
    return error;
}

PF_Err params_setup(PF_InData* in_data, PF_OutData* out_data) {
    const auto defaults = make_default_settings();
    PF_ParamDef def{};
    PF_ADD_POPUPX("Mode", 3, 3, localization::popup_items(kMode, localization::english), PF_ParamFlag_SUPERVISE, kMode);
    PF_ADD_FLOAT_SLIDER("Toon Strength", 0.0, 1.0, 0.0, 1.0, 0.0, 1.0,
        PF_Precision_HUNDREDTHS, 0, 0, kToonStrength);
    PF_ADD_FLOAT_SLIDER("Pixel Size", 1.0, 64.0, 1.0, 16.0, 0.0, 2.0,
        PF_Precision_INTEGER, 0, 0, kPixelSize);
    PF_ADD_FLOAT_SLIDER("Dither", 0.0, 1.0, 0.0, 1.0, 0.0, 0.06,
        PF_Precision_HUNDREDTHS, 0, 0, kDither);
    PF_ADD_CHECKBOX("Outline", "", 1, 0, kOutline);
    PF_ADD_FLOAT_SLIDER("Edge Threshold", 0.0, 1.0, 0.0, 0.5, 0.0, 0.075,
        PF_Precision_THOUSANDTHS, 0, 0, kEdgeThreshold);

    auto hidden_native_group = [](PF_ParamIndex index) {
        return (index >= kToonTopic && index <= kToonTopicEnd) ||
            (index >= kPixelTopic && index <= kPixelTopicEnd) ||
            (index >= kGradeTopic && index <= kGradeTopicEnd) ||
            (index >= kCustomTopic && index <= kCustomTopicEnd) ||
            index == kRampBoundary4 || index == kRampColor5;
    };
    auto placeholder_name = [](PF_ParamIndex index) -> const char* {
        switch (index) {
            case kToonTopic: return "Toon Ramp";
            case kToonTopicEnd: return "Toon Ramp End";
            case kPixelTopic: return "Pixel & Dither";
            case kPixelTopicEnd: return "Pixel & Dither End";
            case kGradeTopic: return "Color Grade";
            case kGradeTopicEnd: return "Color Grade End";
            case kCustomTopic: return "Custom Palette";
            case kCustomTopicEnd: return "Custom Palette End";
            default: return nullptr;
        }
    };
    auto add_topic = [&](const char* name, PF_ParamIndex index) -> PF_Err {
        def = PF_ParamDef{};
        def.ui_flags = hidden_native_group(index) ? PF_PUI_NO_ECW_UI : 0;
        const char* placeholder = placeholder_name(index);
        if (placeholder) PF_ADD_NULL(placeholder, index);
        else PF_ADD_TOPIC(name, index);
        return static_cast<PF_Err>(PF_Err_NONE);
    };
    auto add_popup = [&](const char* name, int choices, int value, const char* items, PF_ParamIndex index) -> PF_Err {
        def = PF_ParamDef{};
        def.flags = PF_ParamFlag_SUPERVISE;
        def.ui_flags = hidden_native_group(index) ? PF_PUI_NO_ECW_UI : 0;
        PF_ADD_POPUP(name, choices, value, localization::popup_items(index, localization::english), index);
        return static_cast<PF_Err>(PF_Err_NONE);
    };
    auto add_float = [&](const char* name, double minimum, double maximum, double slider_max,
        double value, int precision, PF_ParamIndex index) -> PF_Err {
        def = PF_ParamDef{};
        def.ui_flags = hidden_native_group(index) ? PF_PUI_NO_ECW_UI : 0;
        PF_ADD_FLOAT_SLIDER(name, minimum, maximum, 0.0, slider_max, 0.0, value, precision, 0, 0, index);
        return static_cast<PF_Err>(PF_Err_NONE);
    };
    auto add_color = [&](const char* name, const toonpixel::Rgb& color, PF_ParamIndex index) -> PF_Err {
        const auto channel = [](float value) {
            return static_cast<A_u_char>(std::lround(std::clamp(toonpixel::encode_unmanaged(value), 0.0f, 1.0f) * 255.0f));
        };
        def = PF_ParamDef{};
        def.ui_flags = hidden_native_group(index) ? PF_PUI_NO_ECW_UI : 0;
        PF_ADD_COLOR(name, channel(color.red), channel(color.green), channel(color.blue), index);
        return static_cast<PF_Err>(PF_Err_NONE);
    };
    auto add_ramp_color = [&](PF_ParamIndex index) -> PF_Err {
        const std::size_t color_index = index == kRampColor5 ? defaults.ramp.colors.size() - 1 :
            static_cast<std::size_t>(index - kRampColor1);
        char name[32];
        std::snprintf(name, sizeof(name), "Ramp Color %zu", color_index + 1);
        return add_color(name, defaults.ramp.colors[color_index], index);
    };
    auto end_topic = [&](PF_ParamIndex index) -> PF_Err {
        def = PF_ParamDef{};
        def.ui_flags = hidden_native_group(index) ? PF_PUI_NO_ECW_UI : 0;
        const char* placeholder = placeholder_name(index);
        if (placeholder) PF_ADD_NULL(placeholder, index);
        else PF_END_TOPIC(index);
        return static_cast<PF_Err>(PF_Err_NONE);
    };

    add_topic("Toon Ramp", kToonTopic);
    add_popup("Toon Model", 2, defaults.toon_model, "Preserve Hue|Luma Ramp", kToonModel);
    for (std::size_t index = 0; index < 3; ++index) {
        char name[32];
        std::snprintf(name, sizeof(name), "Ramp Boundary %zu", index + 1);
        add_float(name, 0.0, 1.0, 1.0, defaults.ramp.boundaries[index], PF_Precision_THOUSANDTHS,
            kRampBoundaryParameters[index]);
    }
    add_float("Ramp Softness", 0.0, 0.5, 0.5, defaults.ramp.softness, PF_Precision_THOUSANDTHS, kRampSoftness);
    add_float("Ramp Repeat", 1.0, 8.0, 8.0, defaults.ramp.repeat, PF_Precision_INTEGER, kRampRepeat);
    for (PF_ParamIndex index = kRampColor1; index <= kRampColor4; ++index) {
        add_ramp_color(index);
    }
    end_topic(kToonTopicEnd);

    add_topic("Pixel & Dither", kPixelTopic);
    add_popup("Palette", 3, defaults.palette_mode, "Original 8|Style 10|Custom", kPaletteMode);
    add_float("Custom Count", 1.0, 10.0, 10.0, defaults.palette_count, PF_Precision_INTEGER, kPaletteCount);
    add_popup("Dither Mode", 3, defaults.dither_mode, "Off|Bayer|Floyd-Steinberg", kDitherMode);
    add_float("Dither Threshold", 0.0, 1.0, 1.0, defaults.dither_threshold, PF_Precision_THOUSANDTHS, kDitherThreshold);
    end_topic(kPixelTopicEnd);

    add_topic("Color Grade", kGradeTopic);
    add_float("Exposure", -2.0, 2.0, 2.0, defaults.grade.exposure, PF_Precision_HUNDREDTHS, kExposure);
    add_float("Vibrance", -1.0, 1.0, 1.0, defaults.grade.vibrance, PF_Precision_HUNDREDTHS, kVibrance);
    add_float("Hue", -180.0, 180.0, 180.0, defaults.grade.hue, PF_Precision_HUNDREDTHS, kHue);
    add_float("Chroma", 0.0, 2.0, 2.0, defaults.grade.chroma, PF_Precision_HUNDREDTHS, kChroma);
    add_color("Tint Color", defaults.grade.tint, kTintColor);
    add_float("Tint Amount", 0.0, 1.0, 1.0, defaults.grade.tint_amount, PF_Precision_HUNDREDTHS, kTintAmount);
    end_topic(kGradeTopicEnd);

    add_topic("Outline Settings", kOutlineTopic);
    add_popup("Pixel Outline", 2, defaults.pixel_outline, "Grid Edge|Legacy Block", kPixelOutline);
    add_color("Outline Color", defaults.outline_color, kOutlineColor);
    add_float("Outline Strength", 0.0, 1.0, 1.0, defaults.outline_strength, PF_Precision_HUNDREDTHS, kOutlineStrength);
    add_float("Outline Width", 1.0, 4.0, 4.0, defaults.outline_width, PF_Precision_INTEGER, kOutlineWidth);
    end_topic(kOutlineTopicEnd);

    add_topic("Custom Palette", kCustomTopic);
    for (std::size_t index = 0; index < defaults.custom_colors.size(); ++index) {
        char name[32];
        std::snprintf(name, sizeof(name), "Custom %02zu", index + 1);
        add_color(name, defaults.custom_colors[index], static_cast<PF_ParamIndex>(kCustomColor1 + index));
    }
    end_topic(kCustomTopicEnd);

    auto add_widget = [&](const char* name, A_short width, A_short height, PF_ParamIndex index) -> PF_Err {
        def = PF_ParamDef{};
        PF_ArbitraryH initial = nullptr;
        const PF_Err create_error = arbitrary::create_with_host(in_data, arbitrary::RampData{}, &initial);
        if (create_error) return create_error;
        PF_ADD_ARBITRARY2(name, width, height, PF_ParamFlag_SUPERVISE,
            PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL, initial, index, nullptr);
        return PF_Err_NONE;
    };
    auto add_layer = [&](const char* name, PF_ParamIndex index) -> PF_Err {
        def = PF_ParamDef{};
        def.flags = PF_ParamFlag_SUPERVISE;
        PF_ADD_LAYER(name, 0, index);
        return PF_Err_NONE;
    };
    add_topic("Color Studio", kWidgetTopic);
    add_widget("Luma Ramp", 220, 196, kRampBar);
    add_widget("Oklch Tint", 220, 112, kColorWheel);
    end_topic(kWidgetTopicEnd);

    add_topic("Dither Decay", kDitherTopic);
    add_popup("Dither Matrix", 2, 1, "4 x 4 Bayer|8 x 8 Bayer", kDitherMatrix);
    add_popup("Decay Shape", 3, defaults.decay.shape, "Off|Linear|Radial", kDecayShape);
    add_float("Decay Amount", 0.0, 1.0, 1.0, defaults.decay.amount, PF_Precision_HUNDREDTHS, kDecayAmount);
    add_float("Decay Center X", 0.0, 1.0, 1.0, defaults.decay.center_x, PF_Precision_HUNDREDTHS, kDecayCenterX);
    add_float("Decay Center Y", 0.0, 1.0, 1.0, defaults.decay.center_y, PF_Precision_HUNDREDTHS, kDecayCenterY);
    add_float("Decay Scale", 0.01, 4.0, 2.0, defaults.decay.scale, PF_Precision_HUNDREDTHS, kDecayScale);
    add_float("Decay Angle", -180.0, 180.0, 180.0, defaults.decay.angle, PF_Precision_HUNDREDTHS, kDecayAngle);
    end_topic(kDitherTopicEnd);

    add_topic("Oklab Gamut", kGamutTopic);
    add_popup("Gamut Mapping", 5, defaults.gamut_mode,
        "Off|Adaptive Hue-dependent|Chroma Clipped|Truncate|Debug", kGamutMode);
    add_float("Gamut Margin", 0.0, 0.25, 0.1, defaults.gamut_margin, PF_Precision_THOUSANDTHS, kGamutMargin);
    end_topic(kGamutTopicEnd);

    add_topic("ASCII / RXXR", kAsciiTopic);
    PF_ADD_CHECKBOX("ASCII Enabled", "", 0, PF_ParamFlag_SUPERVISE, kAsciiEnabled);
    add_popup("Character Style", 14, defaults.ascii_style,
        "Custom|Soft|Heavy|Binary|Hex|Letters|Blocks|Kana|Pixel Graphics|Braille|Quadrants|Bars|Waveform|Data Glitch",
        kAsciiStyle);
    def = PF_ParamDef{};
    def.param_type = PF_Param_ARBITRARY_DATA;
    PF_STRCPY(def.name, "Character Ramp");
    def.flags = PF_ParamFlag_SUPERVISE;
    def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
    def.ui_width = 220;
    def.ui_height = 64;
    def.uu.id = kAsciiRamp;
    def.u.arb_d.id = 1;
    def.u.arb_d.dephault = PF_NEW_HANDLE(sizeof(arbitrary::RampData));
    if (!def.u.arb_d.dephault) return PF_Err_OUT_OF_MEMORY;
    const arbitrary::RampData initial_ramp;
    std::memcpy(*def.u.arb_d.dephault, &initial_ramp, sizeof(initial_ramp));
    {
        const PF_Err arb_error = PF_ADD_PARAM(in_data, -1, &def);
        if (arb_error) {
            PF_DISPOSE_HANDLE(def.u.arb_d.dephault);
            return arb_error;
        }
    }
    add_float("Character Cell Size", 4.0, 32.0, 16.0, defaults.ascii_cell_size, PF_Precision_INTEGER, kAsciiCellSize);
    add_popup("Character Drive", 4, defaults.ascii_drive, "Off|Audio|Map|Audio + Map", kAsciiDrive);
    add_popup("Drive Channel", 5, defaults.ascii_channel, "Luma|Red|Green|Blue|Alpha", kAsciiChannel);
    add_layer("Luma / Mask Map", kAsciiMapLayer);
    add_popup("Map Mode", 3, defaults.ascii_map_mode, "Off|Luma Map|Mask / Alpha", kAsciiMapMode);
    add_float("Drive Amount", 0.0, 2.0, 1.0, defaults.ascii_amount, PF_Precision_HUNDREDTHS, kAsciiAmount);
    add_popup("Character Color", 3, defaults.ascii_color_mode, "Fill|Source|Gradient", kAsciiColorMode);
    add_color("Character Fill", defaults.ascii_color, kAsciiColor);
    add_color("Character Gradient End", defaults.ascii_color2, kAsciiColor2);
    add_color("Character Background", defaults.ascii_background, kAsciiBackground);
    add_float("Flicker", 0.0, 1.0, 1.0, defaults.ascii_flicker, PF_Precision_HUNDREDTHS, kAsciiFlicker);
    add_float("Glitch", 0.0, 1.0, 1.0, defaults.ascii_glitch, PF_Precision_HUNDREDTHS, kAsciiGlitch);
    add_float("Chromatic Aberration", 0.0, 4.0, 2.0, defaults.ascii_aberration, PF_Precision_HUNDREDTHS, kAsciiAberration);
    add_layer("Audio Source", kAsciiAudioLayer);
    add_float("Audio Gain", 0.0, 8.0, 4.0, defaults.ascii_audio_gain, PF_Precision_HUNDREDTHS, kAsciiAudioGain);
    end_topic(kAsciiTopicEnd);

    add_topic("Contour & Edge AA", kContourTopic);
    add_popup("Contour Method", 2, defaults.contour_mode, "Luma Gradient|Silhouette Coverage", kContourMode);
    add_float("Contour Width", 0.25, 4.0, 2.0, defaults.contour_width, PF_Precision_HUNDREDTHS, kContourWidth);
    add_popup("Contour AA", 3, defaults.contour_aa, "Off (1x)|Fine (2x)|High (4x)", kContourAA);
    add_popup("Contour Source", 3, defaults.contour_source, "Alpha|Background Color|Alpha + Background", kContourSource);
    add_color("Contour Background", defaults.contour_background, kContourBackground);
    end_topic(kContourTopicEnd);
    add_float("Ramp Boundary 4", 0.0, 1.0, 1.0, defaults.ramp.boundaries[3],
        PF_Precision_THOUSANDTHS, kRampBoundary4);
    add_ramp_color(kRampColor5);
    add_popup("Language / 语言", 2, localization::english, "English|中文", kLanguage);
    out_data->num_params = in_data->num_params + kAddedParamCount;
    if (in_data->inter.register_ui) {
        PF_CustomUIInfo custom_info{};
        custom_info.events = PF_CustomEFlag_EFFECT;
        const PF_Err register_error = PF_REGISTER_UI(in_data, &custom_info);
        TOONPIXEL_LOG("register_effect_ui error=%ld", static_cast<long>(register_error));
        if (register_error) return register_error;
    }
    TOONPIXEL_LOG("params_setup count=%ld", static_cast<long>(out_data->num_params));
    return PF_Err_NONE;
}

#include "ToonPixelAudio.h"

struct PreRenderState {
    RenderSettings settings;
    A_long map_checkout_id = 0;
};

PF_Err smart_pre_render(PF_InData* in_data, PF_PreRenderExtra* extra) {
    RenderSettings settings;
    const PF_Err settings_error = read_settings(in_data, settings);
    if (settings_error) {
        return settings_error;
    }
    extra->output->pre_render_data = nullptr;
    extra->output->delete_pre_render_data_func = nullptr;
    extra->output->flags = 0;
    PF_CheckoutResult checkout_result{};
    PF_Err error = extra->cb->checkout_layer(
        in_data->effect_ref,
        kInput,
        1,
        &extra->input->output_request,
        in_data->current_time,
        in_data->time_step,
        in_data->time_scale,
        &checkout_result);
    TOONPIXEL_LOG("smart_pre_render checkout error=%ld", static_cast<long>(error));
    if (error) {
        return error;
    }
    TOONPIXEL_LOG(
        "smart_pre_render result=%ld,%ld,%ld,%ld max=%ld,%ld,%ld,%ld solid=%d",
        static_cast<long>(checkout_result.result_rect.left),
        static_cast<long>(checkout_result.result_rect.top),
        static_cast<long>(checkout_result.result_rect.right),
        static_cast<long>(checkout_result.result_rect.bottom),
        static_cast<long>(checkout_result.max_result_rect.left),
        static_cast<long>(checkout_result.max_result_rect.top),
        static_cast<long>(checkout_result.max_result_rect.right),
        static_cast<long>(checkout_result.max_result_rect.bottom),
        static_cast<int>(checkout_result.solid));
    A_long map_checkout_id = 0;
    if (settings.ascii_enabled && settings.ascii_map != 0 && settings.ascii_map_mode != 1 &&
        (settings.ascii_drive == 3 || settings.ascii_drive == 4)) {
        map_checkout_id = settings.ascii_map;
        PF_CheckoutResult map_result{};
        error = extra->cb->checkout_layer(
            in_data->effect_ref,
            settings.ascii_map,
            map_checkout_id,
            &extra->input->output_request,
            in_data->current_time,
            in_data->time_step,
            in_data->time_scale,
            &map_result);
        TOONPIXEL_LOG("smart_pre_render map index=%d id=%ld error=%ld rect=%ld,%ld,%ld,%ld",
            settings.ascii_map, static_cast<long>(map_checkout_id), static_cast<long>(error),
            static_cast<long>(map_result.result_rect.left), static_cast<long>(map_result.result_rect.top),
            static_cast<long>(map_result.result_rect.right), static_cast<long>(map_result.result_rect.bottom));
        if (error) {
            return error;
        }
    }
    settings.full_rect = {
        static_cast<int>(checkout_result.result_rect.left),
        static_cast<int>(checkout_result.result_rect.top),
        static_cast<int>(checkout_result.result_rect.right),
        static_cast<int>(checkout_result.result_rect.bottom),
    };
    extra->output->result_rect = checkout_result.result_rect;
    extra->output->max_result_rect = checkout_result.max_result_rect;
    extra->output->solid = checkout_result.solid;
    auto* data = new (std::nothrow) PreRenderState{settings, map_checkout_id};
    if (!data) {
        return PF_Err_OUT_OF_MEMORY;
    }
    extra->output->pre_render_data = data;
    extra->output->delete_pre_render_data_func = [](void* opaque) {
        delete static_cast<PreRenderState*>(opaque);
    };
    return PF_Err_NONE;
}

PF_Err smart_render(PF_InData* in_data, PF_SmartRenderExtra* extra) {
    const auto* state = static_cast<const PreRenderState*>(extra->input->pre_render_data);
    if (!state) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    const auto* settings = &state->settings;
    RenderSettings render_settings = *settings;
    PF_Err error = audio::read_level(in_data, render_settings);
    if (error) {
        TOONPIXEL_LOG("smart_render audio_error=%ld", static_cast<long>(error));
        return error;
    }
    PF_EffectWorld* input = nullptr;
    PF_EffectWorld* output = nullptr;
    error = extra->cb->checkout_layer_pixels(in_data->effect_ref, 1, &input);
    TOONPIXEL_LOG("smart_render checkout input error=%ld input=%p", static_cast<long>(error), static_cast<void*>(input));
    if (error) {
        return error;
    }
    error = extra->cb->checkout_output(in_data->effect_ref, &output);
    TOONPIXEL_LOG("smart_render checkout output error=%ld output=%p", static_cast<long>(error), static_cast<void*>(output));
    if (!error && input && output && input->data && output->data) {
        PF_EffectWorld* map_ptr = nullptr;
        if (state->map_checkout_id > 0) {
            error = extra->cb->checkout_layer_pixels(in_data->effect_ref, state->map_checkout_id, &map_ptr);
            if (!error && map_ptr && !map_ptr->data) map_ptr = nullptr;
            TOONPIXEL_LOG("map_checkout id=%ld error=%ld map=%p", static_cast<long>(state->map_checkout_id),
                static_cast<long>(error), static_cast<void*>(map_ptr));
        }
        WorldSuiteHandle suites(in_data);
        if (!error) error = suites.error();
        TOONPIXEL_LOG("smart_render acquire world suite error=%ld", static_cast<long>(error));
        PF_PixelFormat input_format = PF_PixelFormat_INVALID;
        PF_PixelFormat output_format = PF_PixelFormat_INVALID;
        PF_PixelFormat map_format = PF_PixelFormat_INVALID;
        if (!error) error = suites.format(input, input_format);
        if (!error) error = suites.format(output, output_format);
        if (!error && map_ptr) error = suites.format(map_ptr, map_format);
        if (!error && (input_format != output_format || (map_ptr && map_format != input_format))) {
            error = PF_Err_BAD_CALLBACK_PARAM;
        }
        TOONPIXEL_LOG("smart_render formats input=%d output=%d map=%d error=%ld",
            static_cast<int>(input_format), static_cast<int>(output_format), static_cast<int>(map_format), static_cast<long>(error));
        TOONPIXEL_LOG("smart_render input origin=%ld,%ld size=%ldx%ld rowbytes=%ld output origin=%ld,%ld size=%ldx%ld rowbytes=%ld",
            static_cast<long>(input->origin_x), static_cast<long>(input->origin_y),
            static_cast<long>(input->width), static_cast<long>(input->height), static_cast<long>(input->rowbytes),
            static_cast<long>(output->origin_x), static_cast<long>(output->origin_y),
            static_cast<long>(output->width), static_cast<long>(output->height), static_cast<long>(output->rowbytes));
        PF_LRect area = extra->input->output_request.rect;
        const bool preserve = extra->input->output_request.preserve_rgb_of_zero_alpha;
        const PF_ChannelMask mask = extra->input->output_request.channel_mask;
        TOONPIXEL_LOG("smart_render area=%ld,%ld,%ld,%ld preserve=%d mask=%lx",
            static_cast<long>(area.left), static_cast<long>(area.top),
            static_cast<long>(area.right), static_cast<long>(area.bottom),
            static_cast<int>(preserve), static_cast<unsigned long>(mask));
        if (!error && intersect_rect(area, world_rect(input)) && intersect_rect(area, world_rect(output))) {
            switch (input_format) {
                case PF_PixelFormat_ARGB32:
                    error = render_typed<PF_Pixel8>(input, output, area, preserve, mask, render_settings, map_ptr);
                    break;
                case PF_PixelFormat_ARGB64:
                    error = render_typed<PF_Pixel16>(input, output, area, preserve, mask, render_settings, map_ptr);
                    break;
                case PF_PixelFormat_ARGB128:
                    error = render_typed<PF_PixelFloat>(input, output, area, preserve, mask, render_settings, map_ptr);
                    break;
                default:
                    error = PF_Err_BAD_CALLBACK_PARAM;
                    break;
            }
        }
        if (state->map_checkout_id > 0) {
            const PF_Err map_error = extra->cb->checkin_layer_pixels(in_data->effect_ref, state->map_checkout_id);
            if (!error) error = map_error;
        }
    } else if (!error) {
        error = PF_Err_BAD_CALLBACK_PARAM;
    }
    const PF_Err checkin_error = extra->cb->checkin_layer_pixels(in_data->effect_ref, 1);
    TOONPIXEL_LOG("smart_render end error=%ld checkin=%ld", static_cast<long>(error), static_cast<long>(checkin_error));
    (void)checkin_error;
    return error;
}

PF_Err classic_render(PF_InData* in_data, PF_ParamDef* params[], PF_LayerDef* output) {
    PF_EffectWorld* input = &params[kInput]->u.ld;
    if (!input->data || !output->data) {
        TOONPIXEL_LOG("classic_render missing pixel data input=%p output=%p", input->data, output->data);
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    RenderSettings settings;
    PF_Err error = read_settings(in_data, settings);
    if (error) {
        return error;
    }
    error = audio::read_level(in_data, settings);
    if (error) return error;
    const PF_LRect input_rect = world_rect(input);
    settings.full_rect = {
        static_cast<int>(input_rect.left),
        static_cast<int>(input_rect.top),
        static_cast<int>(input_rect.right),
        static_cast<int>(input_rect.bottom),
    };
    PF_ParamDef map_parameter{};
    const PF_EffectWorld* map_ptr = nullptr;
    bool map_checked_out = false;
    if (settings.ascii_enabled && settings.ascii_map != 0 && settings.ascii_map_mode != 1 &&
        (settings.ascii_drive == 3 || settings.ascii_drive == 4)) {
        error = PF_CHECKOUT_PARAM(in_data, settings.ascii_map, in_data->current_time,
            in_data->time_step, in_data->time_scale, &map_parameter);
        map_checked_out = !error;
        map_ptr = &map_parameter.u.ld;
    }
    WorldSuiteHandle suites(in_data);
    if (!error) error = suites.error();
    PF_PixelFormat input_format = PF_PixelFormat_INVALID;
    PF_PixelFormat output_format = PF_PixelFormat_INVALID;
    PF_PixelFormat map_format = PF_PixelFormat_INVALID;
    if (!error) error = suites.format(input, input_format);
    if (!error) error = suites.format(output, output_format);
    if (!error && map_ptr) error = suites.format(map_ptr, map_format);
    if (!error && map_ptr && map_format != input_format) error = PF_Err_BAD_CALLBACK_PARAM;
    TOONPIXEL_LOG(
        "classic_render formats input=%d output=%d suite_error=%ld",
        static_cast<int>(input_format), static_cast<int>(output_format), static_cast<long>(error));
    TOONPIXEL_LOG(
        "classic_render input origin=%ld,%ld size=%ldx%ld rowbytes=%ld output origin=%ld,%ld size=%ldx%ld rowbytes=%ld",
        static_cast<long>(input->origin_x), static_cast<long>(input->origin_y),
        static_cast<long>(input->width), static_cast<long>(input->height), static_cast<long>(input->rowbytes),
        static_cast<long>(output->origin_x), static_cast<long>(output->origin_y),
        static_cast<long>(output->width), static_cast<long>(output->height), static_cast<long>(output->rowbytes));
    if (!error && input_format != output_format) {
        TOONPIXEL_LOG("classic_render mismatched formats");
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    PF_LRect area = output->extent_hint;
    TOONPIXEL_LOG(
        "classic_render area=%ld,%ld,%ld,%ld",
        static_cast<long>(area.left), static_cast<long>(area.top),
        static_cast<long>(area.right), static_cast<long>(area.bottom));
    if (!intersect_rect(area, world_rect(input)) || !intersect_rect(area, world_rect(output))) {
        TOONPIXEL_LOG("classic_render empty area");
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    switch (input_format) {
        case PF_PixelFormat_ARGB32:
            error = render_typed<PF_Pixel8>(input, output, area, false, PF_ChannelMask_ARGB, settings, map_ptr);
            break;
        case PF_PixelFormat_ARGB64:
            error = render_typed<PF_Pixel16>(input, output, area, false, PF_ChannelMask_ARGB, settings, map_ptr);
            break;
        case PF_PixelFormat_ARGB128:
            error = render_typed<PF_PixelFloat>(input, output, area, false, PF_ChannelMask_ARGB, settings, map_ptr);
            break;
        default:
            error = PF_Err_BAD_CALLBACK_PARAM;
            break;
    }
    if (map_checked_out) {
        const PF_Err map_error = PF_CHECKIN_PARAM(in_data, &map_parameter);
        if (!error) error = map_error;
    }
    TOONPIXEL_LOG("classic_render end error=%ld", static_cast<long>(error));
    return error;
}

}  // namespace

extern "C" __attribute__((visibility("default"))) A_Err PluginDataEntryFunction2(
    void* inPtr,
    A_Err (*inPluginDataCallBackPtr)(
        void*,
        const A_u_char*,
        const A_u_char*,
        const A_u_char*,
        const A_u_char*,
        A_long,
        A_long,
        A_long,
        A_long,
        const A_u_char*),
    SPBasicSuite* inSPBasicSuitePtr,
    const char* inHostName,
    const char* inHostVersion) {
    TOONPIXEL_LOG("registration_begin");
    if (!inPluginDataCallBackPtr || !inSPBasicSuitePtr || !inHostName || !inHostVersion) {
        TOONPIXEL_LOG("registration_invalid_callback");
        return PF_Err_INVALID_CALLBACK;
    }
    const A_Err result = inPluginDataCallBackPtr(
        inPtr,
        reinterpret_cast<const A_u_char*>("ToonPixel SmartFX"),
        reinterpret_cast<const A_u_char*>("ToonPixel SmartFX"),
        reinterpret_cast<const A_u_char*>("ToonPixel"),
        reinterpret_cast<const A_u_char*>("EffectMain"),
        0x65464b54L,
        PF_PLUG_IN_VERSION,
        PF_PLUG_IN_SUBVERS,
        8,
        reinterpret_cast<const A_u_char*>(""));
    TOONPIXEL_LOG("registration_result=%ld", static_cast<long>(result));
    return result;
}

extern "C" __attribute__((visibility("default"))) PF_Err EffectMain(
    PF_Cmd cmd,
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output,
    void* extra) {
    TOONPIXEL_LOG(
        "EffectMain cmd=%ld in_data=%p out_data=%p params=%p output=%p extra=%p",
        static_cast<long>(cmd), static_cast<void*>(in_data), static_cast<void*>(out_data),
        static_cast<void*>(params), static_cast<void*>(output), extra);
    PF_Err error = PF_Err_NONE;
    switch (cmd) {
        case PF_Cmd_GLOBAL_SETUP:
            out_data->my_version = PF_VERSION(0, 1, 2, PF_Stage_RELEASE, 1);
            out_data->out_flags = PF_OutFlag_I_EXPAND_BUFFER | PF_OutFlag_SEND_UPDATE_PARAMS_UI |
                PF_OutFlag_CUSTOM_UI | PF_OutFlag_I_USE_AUDIO;
            out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE;
            TOONPIXEL_LOG(
                "global_setup host_version=%d.%d version=%lx flags=%lx flags2=%lx",
                static_cast<int>(in_data->version.major), static_cast<int>(in_data->version.minor),
                static_cast<unsigned long>(out_data->my_version),
                static_cast<unsigned long>(out_data->out_flags),
                static_cast<unsigned long>(out_data->out_flags2));
            break;
        case PF_Cmd_PARAMS_SETUP:
            error = params_setup(in_data, out_data);
            break;
        case PF_Cmd_USER_CHANGED_PARAM:
        case PF_Cmd_UPDATE_PARAMS_UI:
            error = update_params_ui(in_data, out_data, params);
            break;
        case PF_Cmd_ARBITRARY_CALLBACK: {
            auto* arb = static_cast<PF_ArbParamsExtra*>(extra);
            error = arbitrary::callback(in_data, arb);
            TOONPIXEL_LOG("arbitrary_callback selector=%ld id=%d error=%ld",
                arb ? static_cast<long>(arb->which_function) : -1L, arb ? arb->id : -1, static_cast<long>(error));
            break;
        }
        case PF_Cmd_EVENT: {
            auto* event = static_cast<PF_EventExtra*>(extra);
            if (event && event->e_type == PF_Event_DRAW) {
                const auto& frame = event->effect_win.current_frame;
                TOONPIXEL_LOG("widget_draw_dispatch area=%ld frame=%ld,%ld,%ld,%ld",
                    static_cast<long>(event->effect_win.area), static_cast<long>(frame.left),
                    static_cast<long>(frame.top), static_cast<long>(frame.right), static_cast<long>(frame.bottom));
                if (params) {
                    for (const PF_ParamIndex index : {kRampBar, kColorWheel, kWidgetTopicEnd}) {
                        const PF_ParamDef* parameter = params[index];
                        if (parameter) {
                            TOONPIXEL_LOG("widget_event_param index=%ld type=%d id=%ld size=%d,%d flags=%x name=%s",
                                static_cast<long>(index), static_cast<int>(parameter->param_type),
                                static_cast<long>(parameter->uu.id), parameter->ui_width, parameter->ui_height,
                                parameter->ui_flags, parameter->name);
                        }
                    }
                }
            }
            error = widgets::widget_event(in_data, out_data, params, event);
            break;
        }
        case PF_Cmd_RENDER:
            error = classic_render(in_data, params, output);
            break;
        case PF_Cmd_SMART_PRE_RENDER:
            error = smart_pre_render(in_data, static_cast<PF_PreRenderExtra*>(extra));
            break;
        case PF_Cmd_SMART_RENDER:
            error = smart_render(in_data, static_cast<PF_SmartRenderExtra*>(extra));
            break;
        default:
            break;
    }
    TOONPIXEL_LOG("EffectMain cmd=%ld return=%ld", static_cast<long>(cmd), static_cast<long>(error));
    return error;
}
