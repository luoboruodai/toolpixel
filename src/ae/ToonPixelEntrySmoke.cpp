#include <CoreServices/CoreServices.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectVers.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_EffectPixelFormat.h"
#include "ToonPixelSettings.h"
#include "ToonPixelLocalization.h"
#include "ToonPixelArb.h"
#include "ToonPixelGlyphs.h"
#include "adobesdk/DrawbotSuite.h"
struct StraightPixel;
#include "ToonPixelStudio.h"

#include <dlfcn.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

using EffectMain = PF_Err (*)(
    PF_Cmd,
    PF_InData*,
    PF_OutData*,
    PF_ParamDef*[],
    PF_LayerDef*,
    void*);

std::array<PF_ParamDef, 128> added_parameters{};
int added_parameter_count = 0;
int update_call_count = 0;
int checkout_param_calls = 0;
PF_WorldSuite2 fake_world_suite{};
PF_ParamUtilsSuite3 fake_param_utils{};
PFAppSuite4 fake_app_suite{};
PF_EffectWorld fake_input_world{};
PF_EffectWorld fake_output_world{};
PF_LRect fake_max_result_rect{};
PF_EffectWorld fake_map_world{};
A_long fake_map_checkout_id = 0;
bool fake_map_available = false;
int fake_map_checkins = 0;

PF_Err add_parameter(PF_ProgPtr, PF_ParamIndex index, PF_ParamDefPtr definition) {
    if (index != -1 || !definition || added_parameter_count < 0 ||
        added_parameter_count >= static_cast<int>(added_parameters.size()) ||
        definition->uu.id != added_parameter_count + 1) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    added_parameters[added_parameter_count] = *definition;
    ++added_parameter_count;
    return PF_Err_NONE;
}

int host_alloc_calls = 0;

PF_Handle fake_host_new_handle(A_u_longlong size) {
    ++host_alloc_calls;
    return NewHandle(static_cast<std::size_t>(size));
}

void fake_host_dispose_handle(PF_Handle handle) {
    DisposeHandle(handle);
}

int acquire_fake_suite(const char* name, int version, const void** suite) {
    if (!suite) {
        return 1;
    }
    if (std::strcmp(name, kPFWorldSuite) == 0 && version == kPFWorldSuiteVersion2) {
        *suite = &fake_world_suite;
        return 0;
    }
    if (std::strcmp(name, kPFParamUtilsSuite) == 0 && version == kPFParamUtilsSuiteVersion3) {
        *suite = &fake_param_utils;
        return 0;
    }
    if (std::strcmp(name, kPFAppSuite) == 0 && version == kPFAppSuiteVersion4) {
        *suite = &fake_app_suite;
        return 0;
    }
    if ((std::strcmp(name, kPFEffectCustomUISuite) == 0 && version == kPFEffectCustomUISuiteVersion1) ||
        (std::strcmp(name, kDRAWBOT_DrawSuite) == 0 && version == kDRAWBOT_DrawSuite_Version1) ||
        (std::strcmp(name, kDRAWBOT_SupplierSuite) == 0 && version == kDRAWBOT_SupplierSuite_Version1) ||
        (std::strcmp(name, kDRAWBOT_SurfaceSuite) == 0 && version == kDRAWBOT_SurfaceSuite_Version1)) {
        static unsigned char storage[4096]{};
        *suite = storage;
        return 0;
    }
    return 1;
}

int release_fake_suite(const char*, int) {
    return 0;
}

bool picker_accept = true;
int picker_calls = 0;
PF_PixelFloat picker_color{1.0f, 0.2f, 0.4f, 0.6f};

PF_Err fake_color_picker(const char*, const PF_PixelFloat*, PF_Boolean, PF_PixelFloat* picked) {
    ++picker_calls;
    if (!picker_accept) return PF_Interrupt_CANCEL;
    *picked = picker_color;
    return PF_Err_NONE;
}

PF_Point fake_mouse{160, 110};

PF_Err fake_get_mouse(PF_Point* point) {
    if (!point) return PF_Err_BAD_CALLBACK_PARAM;
    *point = fake_mouse;
    return PF_Err_NONE;
}

int invalidate_calls = 0;
PF_Rect invalidated_rect{};

PF_Err fake_invalidate_rect(const PF_ContextH, const PF_Rect* rect) {
    if (!rect) return PF_Err_BAD_CALLBACK_PARAM;
    ++invalidate_calls;
    invalidated_rect = *rect;
    return PF_Err_NONE;
}

PF_Err fake_pixel_format(const PF_EffectWorld*, PF_PixelFormat* format) {
    *format = PF_PixelFormat_ARGB128;
    return 0;
}

PF_Err fake_checkout_param(PF_ProgPtr, PF_ParamIndex index, A_long, A_long, A_u_long, PF_ParamDef* parameter) {
    if (!parameter || index < 0 || index > added_parameter_count) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    ++checkout_param_calls;
    *parameter = PF_ParamDef{};
    if (index == kInput) {
        parameter->u.ld = fake_input_world;
    } else {
        *parameter = added_parameters[index - 1];
    }
    return PF_Err_NONE;
}

PF_Err fake_checkin_param(PF_ProgPtr, PF_ParamDef*) {
    return PF_Err_NONE;
}

PF_Err fake_update_param_ui(PF_ProgPtr, PF_ParamIndex index, const PF_ParamDef* definition) {
    if (!definition || index < 1 || index > added_parameter_count) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    added_parameters[index - 1] = *definition;
    ++update_call_count;
    return PF_Err_NONE;
}

PF_Err fake_convert_global(const PF_Point* local, PF_Point* global) {
    if (!local || !global) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    global->h = local->h + 2;
    global->v = local->v + 298;
    return PF_Err_NONE;
}

PF_Err fake_checkout_layer(PF_ProgPtr, PF_ParamIndex index, A_long checkout_id, const PF_RenderRequest* request,
    A_long, A_long, A_u_long, PF_CheckoutResult* result) {
    if (!result) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    if (index == kAsciiMapLayer) fake_map_checkout_id = checkout_id;
    *result = PF_CheckoutResult{};
    result->result_rect = request ? request->rect : PF_LRect{0, 0, 0, 0};
    result->max_result_rect = fake_max_result_rect.right > fake_max_result_rect.left ? fake_max_result_rect :
        PF_LRect{0, 0, fake_input_world.width, fake_input_world.height};
    result->ref_width = fake_input_world.width;
    result->ref_height = fake_input_world.height;
    result->solid = false;
    return PF_Err_NONE;
}

std::vector<int16_t> fake_audio_samples{16384, -16384};
int fake_audio_calls = 0;
int fake_audio_checkins = 0;

PF_Err fake_checkout_audio(PF_ProgPtr, PF_ParamIndex, A_long, A_long, A_u_long,
    PF_UFixed rate, A_long bytes, A_long channels, A_long format, PF_LayerAudio* audio) {
    ++fake_audio_calls;
    if (!audio || rate != static_cast<PF_UFixed>(44100.0 * 65536.0) || bytes != PF_SSS_2 ||
        channels != PF_Channels_MONO || format != PF_SIGNED_PCM) return PF_Err_BAD_CALLBACK_PARAM;
    *audio = reinterpret_cast<PF_LayerAudio>(fake_audio_samples.data());
    return PF_Err_NONE;
}

PF_Err fake_get_audio(PF_ProgPtr, PF_LayerAudio, PF_SndSamplePtr* data, A_long* count,
    PF_UFixed* rate, A_long* bytes, A_long* channels, A_long* format) {
    *data = fake_audio_samples.data();
    *count = static_cast<A_long>(fake_audio_samples.size());
    *rate = static_cast<PF_UFixed>(44100.0 * 65536.0);
    *bytes = PF_SSS_2;
    *channels = PF_Channels_MONO;
    *format = PF_SIGNED_PCM;
    return PF_Err_NONE;
}

PF_Err fake_checkin_audio(PF_ProgPtr, PF_LayerAudio) {
    ++fake_audio_checkins;
    return PF_Err_NONE;
}

PF_Err fake_checkout_input(PF_ProgPtr, A_long checkout_id, PF_EffectWorld** world) {
    if (checkout_id == kAsciiMapLayer) {
        if (!fake_map_available || fake_map_checkout_id != checkout_id) return PF_Err_BAD_CALLBACK_PARAM;
        *world = &fake_map_world;
    } else {
        *world = &fake_input_world;
    }
    return PF_Err_NONE;
}

PF_Err fake_checkout_output(PF_ProgPtr, PF_EffectWorld** world) {
    *world = &fake_output_world;
    return PF_Err_NONE;
}

PF_Err fake_checkin(PF_ProgPtr, A_long checkout_id) {
    if (checkout_id == kAsciiMapLayer) ++fake_map_checkins;
    return PF_Err_NONE;
}

struct RenderAttempt {
    PF_Err error = PF_Err_NONE;
    PF_RenderOutputFlags flags = 0;
};

RenderAttempt render_alpha(EffectMain entry, PF_LRect area, PF_PixelFloat* source, PF_PixelFloat* destination,
    int width = 2, int height = 1) {
    fake_map_checkout_id = 0;
    fake_map_available = fake_map_world.data != nullptr;
    fake_map_checkins = 0;
    added_parameters[kAsciiBackground - 1].u.cd.value = PF_Pixel{255, 0, 0, 0};
    fake_input_world = PF_EffectWorld{};
    fake_input_world.data = reinterpret_cast<PF_PixelPtr>(source);
    fake_input_world.width = width;
    fake_input_world.height = height;
    fake_input_world.rowbytes = width * sizeof(PF_PixelFloat);
    fake_output_world = PF_EffectWorld{};
    fake_output_world.data = reinterpret_cast<PF_PixelPtr>(destination);
    fake_output_world.width = width;
    fake_output_world.height = height;
    fake_output_world.rowbytes = width * sizeof(PF_PixelFloat);
    fake_world_suite.PF_GetPixelFormat = fake_pixel_format;
    fake_param_utils.PF_UpdateParamUI = fake_update_param_ui;

    SPBasicSuite basic{};
    basic.AcquireSuite = acquire_fake_suite;
    basic.ReleaseSuite = release_fake_suite;
    PF_InData in_data{};
    in_data.pica_basicP = &basic;
    in_data.inter.checkout_param = fake_checkout_param;
    in_data.inter.checkin_param = fake_checkin_param;
    in_data.inter.checkout_layer_audio = fake_checkout_audio;
    in_data.inter.checkin_layer_audio = fake_checkin_audio;
    in_data.inter.get_audio_data = fake_get_audio;
    in_data.current_time = 0;
    in_data.time_step = 1;
    in_data.time_scale = 24;
    PF_OutData out_data{};

    PF_PreRenderInput pre_input{};
    pre_input.output_request.rect = area;
    pre_input.output_request.channel_mask = PF_ChannelMask_ARGB;
    pre_input.output_request.preserve_rgb_of_zero_alpha = false;
    pre_input.bitdepth = 32;
    PF_PreRenderOutput pre_output{};
    PF_PreRenderCallbacks pre_callbacks{};
    pre_callbacks.checkout_layer = fake_checkout_layer;
    PF_PreRenderExtra pre_extra{&pre_input, &pre_output, &pre_callbacks};
    RenderAttempt attempt;
    attempt.error = entry(PF_Cmd_SMART_PRE_RENDER, &in_data, &out_data, nullptr, nullptr, &pre_extra);
    if (attempt.error) {
        return attempt;
    }

    PF_SmartRenderInput smart_input{};
    smart_input.output_request.rect = area;
    smart_input.output_request.channel_mask = PF_ChannelMask_ARGB;
    smart_input.output_request.preserve_rgb_of_zero_alpha = false;
    smart_input.pre_render_data = pre_output.pre_render_data;
    PF_SmartRenderCallbacks callbacks{};
    callbacks.checkout_layer_pixels = fake_checkout_input;
    callbacks.checkout_output = fake_checkout_output;
    callbacks.checkin_layer_pixels = fake_checkin;
    PF_SmartRenderExtra smart_extra{&smart_input, &callbacks};
    attempt.error = entry(PF_Cmd_SMART_RENDER, &in_data, &out_data, nullptr, nullptr, &smart_extra);
    attempt.flags = pre_output.flags;
    if (pre_output.delete_pre_render_data_func) {
        pre_output.delete_pre_render_data_func(pre_output.pre_render_data);
    }
    fake_map_world = PF_EffectWorld{};
    fake_map_available = false;
    return attempt;
}

bool alpha_smart_render_smoke(EffectMain entry) {
    added_parameters[kDither - 1].u.fs_d.value = 0.0;
    added_parameters[kDitherMode - 1].u.pd.value = static_cast<A_long>(toonpixel::DitherMode::floyd_steinberg);
    PF_PixelFloat source[] = {
        {0.0f, 0.6318189f, 0.3136705f, 0.2668504f},
        {0.5f, 0.6318189f, 0.3136705f, 0.2668504f},
    };
    PF_PixelFloat full[2]{};
    PF_PixelFloat partial[2]{};
    PF_PixelFloat expanded[2]{};
    const auto full_attempt = render_alpha(entry, {0, 0, 2, 1}, source, full);
    const auto partial_attempt = render_alpha(entry, {0, 0, 1, 1}, source, partial);
    fake_max_result_rect = {0, -1, 3, 2};
    const auto expanded_attempt = render_alpha(entry, {0, 0, 2, 1}, source, expanded);
    fake_max_result_rect = {};
    const auto expected = toonpixel::kPalette[2];
    const auto correct_color = [&](const PF_PixelFloat& pixel) {
        return std::fabs(pixel.alpha - 0.25f) < 1e-6f &&
            std::fabs(pixel.red - toonpixel::encode_unmanaged(expected.red)) < 2e-5f &&
            std::fabs(pixel.green - toonpixel::encode_unmanaged(expected.green)) < 2e-5f &&
            std::fabs(pixel.blue - toonpixel::encode_unmanaged(expected.blue)) < 2e-5f;
    };
    const auto clear_pixel = [](const PF_PixelFloat& pixel) {
        return pixel.alpha == 0.0f && pixel.red == 0.0f && pixel.green == 0.0f && pixel.blue == 0.0f;
    };
    const bool valid = full_attempt.error == PF_Err_NONE && partial_attempt.error == PF_Err_NONE &&
        expanded_attempt.error == PF_Err_NONE &&
        (partial_attempt.flags & PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS) == 0 &&
        (expanded_attempt.flags & PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS) == 0 &&
        correct_color(full[0]) && correct_color(full[1]) &&
        correct_color(expanded[0]) && correct_color(expanded[1]) &&
        clear_pixel(partial[0]) && clear_pixel(partial[1]);
    std::printf(
        "alpha_smart_render_error=%ld,%ld,%ld alpha=%.6f rgb=%.6f,%.6f,%.6f flags=%x,%x valid=%d\n",
        static_cast<long>(full_attempt.error), static_cast<long>(partial_attempt.error),
        static_cast<long>(expanded_attempt.error), full[0].alpha, full[0].red, full[0].green, full[0].blue,
        static_cast<unsigned>(partial_attempt.flags), static_cast<unsigned>(expanded_attempt.flags), valid ? 1 : 0);
    return valid;
}

bool feature_render_smoke(EffectMain entry) {
    added_parameters[kDitherMode - 1].u.pd.value = static_cast<A_long>(toonpixel::DitherMode::off);
    added_parameters[kDither - 1].u.fs_d.value = 0.0;
    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::toon_only);
    added_parameters[kToonStrength - 1].u.fs_d.value = 0.0;
    added_parameters[kOutline - 1].u.bd.value = 1;
    added_parameters[kOutlineColor - 1].u.cd.value = PF_Pixel{255, 0, 0, 0};
    added_parameters[kContourMode - 1].u.pd.value = 2;
    added_parameters[kContourSource - 1].u.pd.value = 1;
    added_parameters[kContourWidth - 1].u.fs_d.value = 2.0;
    added_parameters[kContourAA - 1].u.pd.value = 2;
    std::vector<PF_PixelFloat> circle(13 * 13, PF_PixelFloat{0.0f, 0.65f, 0.65f, 0.65f});
    for (int y = 0; y < 13; ++y) for (int x = 0; x < 13; ++x) {
        circle[y * 13 + x].alpha = std::hypot(x - 6.0f, y - 6.0f) <= 3.0f ? 1.0f : 0.0f;
    }
    std::vector<PF_PixelFloat> outlined(circle.size());
    const auto continuous = render_alpha(entry, {0, 0, 13, 13}, circle.data(), outlined.data(), 13, 13);
    bool partial = false;
    bool dark = false;
    for (const auto& pixel : outlined) {
        partial = partial || (pixel.red > 0.01f && pixel.red < 0.64f);
        dark = dark || (pixel.alpha > 0.0f && pixel.red < 0.01f);
    }
    bool valid = continuous.error == PF_Err_NONE && dark && partial &&
        std::fabs(outlined[6 * 13 + 6].red - 0.65f) < 0.0001f;
    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::toon_pixel);
    added_parameters[kPixelSize - 1].u.fs_d.value = 2.0;
    std::vector<PF_PixelFloat> pixel_outline(circle.size());
    const auto pixel_result = render_alpha(entry, {0, 0, 13, 13}, circle.data(), pixel_outline.data(), 13, 13);
    bool pixel_dark = false;
    bool pixel_partial = false;
    for (const auto& pixel : pixel_outline) {
        pixel_dark = pixel_dark || (pixel.alpha > 0.0f && pixel.red < 0.01f);
        pixel_partial = pixel_partial || (pixel.red > 0.01f && pixel.red < 0.3f);
    }
    valid = valid && pixel_result.error == PF_Err_NONE && pixel_dark && pixel_partial;

    std::vector<PF_PixelFloat> source(8 * 8, PF_PixelFloat{1.0f, 0.4f, 0.4f, 0.4f});
    std::vector<PF_PixelFloat> ascii(source.size());
    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::pixel_only);
    added_parameters[kOutline - 1].u.bd.value = 0;
    added_parameters[kContourMode - 1].u.pd.value = 1;
    added_parameters[kAsciiEnabled - 1].u.bd.value = 1;
    added_parameters[kAsciiStyle - 1].u.pd.value = 1;
    added_parameters[kAsciiCellSize - 1].u.fs_d.value = 4.0;
    added_parameters[kAsciiDrive - 1].u.pd.value = 2;
    added_parameters[kAsciiAudioGain - 1].u.fs_d.value = 1.0;
    arbitrary::replace(&added_parameters[kAsciiRamp - 1], {32, '#'}, 0);
    fake_audio_calls = 0;
    fake_audio_checkins = 0;
    const auto audio = render_alpha(entry, {0, 0, 8, 8}, source.data(), ascii.data(), 8, 8);
    int bright_audio = 0;
    for (const auto& pixel : ascii) bright_audio += pixel.green > 0.1f;
    valid = valid && audio.error == PF_Err_NONE && bright_audio > 0 && fake_audio_calls == 1 && fake_audio_checkins == 1;

    std::vector<PF_PixelFloat> map_pixels(8 * 8, PF_PixelFloat{1.0f, 0.0f, 0.0f, 0.0f});
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 4; ++x) map_pixels[y * 8 + x].red = 1.0f;
    PF_EffectWorld map_world{};
    map_world.data = reinterpret_cast<PF_PixelPtr>(map_pixels.data());
    map_world.width = 8;
    map_world.height = 8;
    map_world.rowbytes = 8 * sizeof(PF_PixelFloat);
    fake_map_world = map_world;
    fake_map_available = true;
    added_parameters[kAsciiMapLayer - 1].u.pd.value = kAsciiMapLayer;
    added_parameters[kAsciiMapLayer - 1].u.ld = map_world;
    added_parameters[kAsciiDrive - 1].u.pd.value = 3;
    added_parameters[kAsciiMapMode - 1].u.pd.value = 2;
    added_parameters[kAsciiChannel - 1].u.pd.value = 2;
    std::fill(ascii.begin(), ascii.end(), PF_PixelFloat{});
    const auto mapped = render_alpha(entry, {0, 0, 8, 8}, source.data(), ascii.data(), 8, 8);
    int left_bright = 0;
    int right_bright = 0;
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        if (ascii[y * 8 + x].green > 0.1f) {
            if (x < 4) ++left_bright;
            else ++right_bright;
        }
    }
    valid = valid && mapped.error == PF_Err_NONE && left_bright > 0 && right_bright == 0 &&
        fake_map_checkout_id == kAsciiMapLayer && fake_map_checkins == 1;
    added_parameters[kAsciiEnabled - 1].u.bd.value = 0;
    added_parameters[kOutline - 1].u.bd.value = 0;

    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::toon_pixel);
    added_parameters[kToonModel - 1].u.pd.value = static_cast<A_long>(toonpixel::ToonModel::luma_ramp);
    added_parameters[kPaletteMode - 1].u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::style10);
    added_parameters[kPixelSize - 1].u.fs_d.value = 1.0;
    added_parameters[kToonStrength - 1].u.fs_d.value = 1.0;
    added_parameters[kDitherMode - 1].u.pd.value = static_cast<A_long>(toonpixel::DitherMode::off);
    added_parameters[kDither - 1].u.fs_d.value = 0.0;
    added_parameters[kContourMode - 1].u.pd.value = 1;
    added_parameters[kGamutMode - 1].u.pd.value = 1;
    toonpixel::RampSettings ramp;
    ramp.boundaries = {0.2, 0.4, 0.6, 0.8};
    ramp.colors = {{{0.01f, 0.02f, 0.1f}, {0.6f, 0.1f, 0.02f}, {0.02f, 0.1f, 0.5f},
        {0.1f, 0.6f, 0.2f}, {0.9f, 0.8f, 0.1f}}};
    const auto parameter_channel = [](float value) {
        return toonpixel::decode_unmanaged(studio::channel(value) / 255.0f);
    };
    for (auto& color : ramp.colors) {
        color = {parameter_channel(color.red), parameter_channel(color.green), parameter_channel(color.blue)};
    }
    for (std::size_t index = 0; index < ramp.boundaries.size(); ++index) {
        added_parameters[kRampBoundaryParameters[index] - 1].u.fs_d.value = ramp.boundaries[index];
    }
    for (std::size_t index = 0; index < ramp.colors.size(); ++index) {
        auto& color = added_parameters[kRampColorParameters[index] - 1].u.cd.value;
        color.red = studio::channel(ramp.colors[index].red);
        color.green = studio::channel(ramp.colors[index].green);
        color.blue = studio::channel(ramp.colors[index].blue);
    }
    const std::array<float, 5> gray{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    std::array<PF_PixelFloat, 5> levels{};
    for (std::size_t index = 0; index < gray.size(); ++index) {
        levels[index] = {1.0f, gray[index], gray[index], gray[index]};
    }
    const auto close_encoded = [](const PF_PixelFloat& pixel, const toonpixel::Rgb& color) {
        return std::fabs(pixel.red - toonpixel::encode_unmanaged(color.red)) < 0.0001f &&
            std::fabs(pixel.green - toonpixel::encode_unmanaged(color.green)) < 0.0001f &&
            std::fabs(pixel.blue - toonpixel::encode_unmanaged(color.blue)) < 0.0001f;
    };
    const auto render_levels = [&]() {
        std::array<PF_PixelFloat, 5> result{};
        const auto attempt = render_alpha(entry, {0, 0, 5, 1}, levels.data(), result.data(), 5, 1);
        return std::pair{attempt, result};
    };
    const auto neutral = render_levels();
    bool linked = neutral.first.error == PF_Err_NONE;
    for (std::size_t index = 0; index < gray.size(); ++index) {
        const auto expected = toonpixel::sample_ramp(toonpixel::decode_unmanaged(gray[index]), ramp);
        linked = linked && close_encoded(neutral.second[index], expected);
        if (!close_encoded(neutral.second[index], expected)) {
            std::fprintf(stderr, "FAILED linked_ramp_pixel_%zu actual=%.6f,%.6f,%.6f expected=%.6f,%.6f,%.6f\n",
                index, neutral.second[index].red, neutral.second[index].green, neutral.second[index].blue,
                toonpixel::encode_unmanaged(expected.red), toonpixel::encode_unmanaged(expected.green),
                toonpixel::encode_unmanaged(expected.blue));
        }
    }
    toonpixel::GradeSettings grade;
    grade.exposure = 0.2f;
    grade.vibrance = 0.35f;
    grade.hue = 60.0f;
    grade.chroma = 1.25f;
    grade.tint = {parameter_channel(0.8f), parameter_channel(0.55f), parameter_channel(0.3f)};
    grade.tint_amount = 0.4f;
    added_parameters[kExposure - 1].u.fs_d.value = grade.exposure;
    added_parameters[kVibrance - 1].u.fs_d.value = grade.vibrance;
    added_parameters[kHue - 1].u.fs_d.value = grade.hue;
    added_parameters[kChroma - 1].u.fs_d.value = grade.chroma;
    added_parameters[kTintColor - 1].u.cd.value = PF_Pixel{255,
        studio::channel(grade.tint.red), studio::channel(grade.tint.green), studio::channel(grade.tint.blue)};
    added_parameters[kTintAmount - 1].u.fs_d.value = grade.tint_amount;
    const auto graded = render_levels();
    bool final_grade = graded.first.error == PF_Err_NONE;
    for (std::size_t index = 0; index < gray.size(); ++index) {
        const auto source = toonpixel::sample_ramp(toonpixel::decode_unmanaged(gray[index]), ramp);
        const auto expected = toonpixel::apply_color_grade(source, grade);
        final_grade = final_grade && close_encoded(graded.second[index], expected);
        if (!close_encoded(graded.second[index], expected)) {
            std::fprintf(stderr, "FAILED final_grade_pixel_%zu actual=%.6f,%.6f,%.6f expected=%.6f,%.6f,%.6f\n",
                index, graded.second[index].red, graded.second[index].green, graded.second[index].blue,
                toonpixel::encode_unmanaged(expected.red), toonpixel::encode_unmanaged(expected.green),
                toonpixel::encode_unmanaged(expected.blue));
        }
    }
    for (const PF_ParamIndex index : {kExposure, kVibrance, kHue}) added_parameters[index - 1].u.fs_d.value = 0.0;
    added_parameters[kChroma - 1].u.fs_d.value = 1.0;
    added_parameters[kTintAmount - 1].u.fs_d.value = 0.0;
    added_parameters[kToonStrength - 1].u.fs_d.value = 0.0;
    const auto unmixed = render_levels();
    bool zero_strength = unmixed.first.error == PF_Err_NONE;
    for (std::size_t index = 0; index < gray.size(); ++index) {
        const bool matches = std::fabs(unmixed.second[index].red - gray[index]) < 0.0001f &&
            std::fabs(unmixed.second[index].green - gray[index]) < 0.0001f &&
            std::fabs(unmixed.second[index].blue - gray[index]) < 0.0001f;
        if (!matches) {
            std::fprintf(stderr, "FAILED zero_strength_pixel_%zu actual=%.6f,%.6f,%.6f expected=%.6f\n",
                index, unmixed.second[index].red, unmixed.second[index].green, unmixed.second[index].blue,
                toonpixel::encode_unmanaged(gray[index]));
        }
        zero_strength = zero_strength && matches;
    }
    valid = valid && linked && final_grade && zero_strength;
    std::printf("feature_render dark=%d partial=%d pixel=%d audio_glyphs=%d map=%d,%d linked=%d grade=%d strength=%d valid=%d\n",
        dark ? 1 : 0, partial ? 1 : 0, pixel_partial ? 1 : 0, bright_audio, left_bright, right_bright,
        linked ? 1 : 0, final_grade ? 1 : 0, zero_strength ? 1 : 0, valid ? 1 : 0);
    return valid;
}

PF_Err update_ui(EffectMain entry, PF_Cmd command) {
    std::array<PF_ParamDef*, kParamCount> parameters{};
    for (PF_ParamIndex index = 1; index <= added_parameter_count; ++index) {
        parameters[index] = &added_parameters[index - 1];
    }
    SPBasicSuite basic{};
    basic.AcquireSuite = acquire_fake_suite;
    basic.ReleaseSuite = release_fake_suite;
    PF_InData in_data{};
    in_data.pica_basicP = &basic;
    in_data.inter.checkout_param = fake_checkout_param;
    in_data.inter.checkin_param = fake_checkin_param;
    PF_UtilCallbacks utilities{};
    utilities.ansi.strcpy = std::strcpy;
    utilities.host_new_handle = fake_host_new_handle;
    utilities.host_dispose_handle = fake_host_dispose_handle;
    in_data.utils = &utilities;
    PF_OutData out_data{};
    const int checkout_before = checkout_param_calls;
    const PF_Err error = entry(command, &in_data, &out_data, parameters.data(), nullptr, nullptr);
    if (error == PF_Err_NONE && checkout_param_calls != checkout_before) {
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    return error;
}

bool ui_smoke(EffectMain entry) {
    const auto enabled = [](PF_ParamIndex index) {
        return (added_parameters[index - 1].ui_flags & PF_PUI_DISABLED) == 0;
    };
    added_parameters[kDitherMode - 1].u.pd.value = static_cast<A_long>(toonpixel::DitherMode::bayer);
    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::toon_pixel);
    added_parameters[kContourMode - 1].u.pd.value = 1;
    const bool first_command = update_ui(entry, PF_Cmd_UPDATE_PARAMS_UI) == PF_Err_NONE;
    const bool first_state = !enabled(kCustomColor1) && !enabled(kPaletteCount) && enabled(kRampBoundary1) &&
        enabled(kPaletteMode) && enabled(kPixelOutline) && enabled(kDither) && !enabled(kContourWidth);

    added_parameters[kPaletteMode - 1].u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::custom);
    added_parameters[kToonModel - 1].u.pd.value = static_cast<A_long>(toonpixel::ToonModel::luma_ramp);
    const bool second_command = update_ui(entry, PF_Cmd_UPDATE_PARAMS_UI) == PF_Err_NONE;
    const bool second_state = enabled(kCustomColor1) && enabled(kPaletteCount) && enabled(kRampBoundary1) &&
        enabled(kRampColor1) && enabled(kRampRepeat) && enabled(kPixelOutline);

    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::pixel_only);
    added_parameters[kDitherMode - 1].u.pd.value = static_cast<A_long>(toonpixel::DitherMode::off);
    const bool third_command = update_ui(entry, PF_Cmd_UPDATE_PARAMS_UI) == PF_Err_NONE;
    const bool fourth_command = update_ui(entry, PF_Cmd_USER_CHANGED_PARAM) == PF_Err_NONE;
    const bool third_state = !enabled(kToonStrength) && enabled(kPixelSize) && !enabled(kDither) &&
        !enabled(kDitherThreshold) && !enabled(kOutline) && !enabled(kPixelOutline) &&
        enabled(kCustomColor1) && enabled(kPaletteCount);
    const bool hidden_model = (added_parameters[kToonModel - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0;
    const bool hidden_tint = (added_parameters[kTintColor - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0;
    const bool hidden_ramp = (added_parameters[kRampColor1 - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0;
    const bool hidden_palette = (added_parameters[kPaletteMode - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0;
    const bool hidden_custom = (added_parameters[kCustomColor1 - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0;
    const bool visible_studio = (added_parameters[kWidgetTopic - 1].ui_flags & PF_PUI_NO_ECW_UI) == 0;
    const bool hidden = hidden_model && hidden_tint && hidden_ramp && hidden_palette && hidden_custom && visible_studio;
    std::printf("hidden_parts=%d,%d,%d,%d mask=%x flags=%x,%x,%x,%x\n", hidden_model,
        hidden_tint, hidden_ramp, visible_studio, static_cast<unsigned>(PF_PUI_NO_ECW_UI),
        added_parameters[kToonModel - 1].ui_flags, added_parameters[kTintColor - 1].ui_flags,
        added_parameters[kRampColor1 - 1].ui_flags, added_parameters[kWidgetTopic - 1].ui_flags);
    const bool valid = first_command && first_state && second_command && second_state &&
        third_command && fourth_command && third_state && hidden && update_call_count >= 3;
    std::printf("ui_update_calls=%d commands=%d,%d,%d,%d states=%d,%d,%d hidden=%d calls=%d valid=%d\n",
        update_call_count, first_command, second_command, third_command, fourth_command,
        first_state, second_state, third_state, hidden, update_call_count >= 3, valid ? 1 : 0);
    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::toon_pixel);
    added_parameters[kContourMode - 1].u.pd.value = 2;
    const bool contour_state = update_ui(entry, PF_Cmd_UPDATE_PARAMS_UI) == PF_Err_NONE &&
        enabled(kContourWidth) && enabled(kContourSource);
    added_parameters[kLanguage - 1].u.pd.value = localization::chinese;
    bool chinese_names_fit = true;
    const bool chinese = update_ui(entry, PF_Cmd_UPDATE_PARAMS_UI) == PF_Err_NONE &&
        std::strcmp(added_parameters[kMode - 1].name, "模式") == 0 &&
        std::strcmp(added_parameters[kLanguage - 1].name, "Language / 语言") == 0 &&
        std::strcmp(added_parameters[kToonModel - 1].name, "卡通模式") == 0 &&
        std::strcmp(added_parameters[kAsciiChannel - 1].name, "驱动通道") == 0 &&
        std::strcmp(added_parameters[kContourWidth - 1].name, "轮廓搜索宽度") == 0 &&
        std::strcmp(added_parameters[kToonTopic - 1].name, "Toon Ramp") == 0 &&
        std::strcmp(added_parameters[kMode - 1].u.pd.u.namesptr, localization::popup_items(kMode, localization::chinese)) == 0 &&
        std::strcmp(added_parameters[kAsciiStyle - 1].u.pd.u.namesptr, localization::popup_items(kAsciiStyle, localization::chinese)) == 0 &&
        std::strcmp(added_parameters[kOutline - 1].u.bd.u.nameptr, "") == 0 &&
        std::strcmp(added_parameters[kAsciiEnabled - 1].u.bd.u.nameptr, "") == 0;
    for (int index = 0; index < added_parameter_count; ++index) {
        if (std::strlen(added_parameters[index].name) > 31) chinese_names_fit = false;
    }
    added_parameters[kLanguage - 1].u.pd.value = localization::english;
    const bool english = update_ui(entry, PF_Cmd_UPDATE_PARAMS_UI) == PF_Err_NONE &&
        std::strcmp(added_parameters[kMode - 1].name, "Mode") == 0 &&
        std::strcmp(added_parameters[kToonModel - 1].name, "Toon Model") == 0 &&
        std::strcmp(added_parameters[kAsciiChannel - 1].name, "Drive Channel") == 0 &&
        std::strcmp(added_parameters[kMode - 1].u.pd.u.namesptr, localization::popup_items(kMode, localization::english)) == 0 &&
        std::strcmp(added_parameters[kAsciiEnabled - 1].u.bd.u.nameptr, "") == 0;
    std::printf("localized chinese=%d english=%d names_fit=%d\n", chinese ? 1 : 0, english ? 1 : 0, chinese_names_fit ? 1 : 0);
    if (!chinese || !english) {
        for (const PF_ParamIndex index : {kMode, kLanguage, kToonModel, kAsciiChannel, kContourWidth, kToonTopic}) {
            const auto& parameter = added_parameters[index - 1];
            const char* expected = localization::parameter_name(index, localization::english);
            std::printf("localized_name index=%d actual=%s expected=%s\n", index, parameter.name,
                expected ? expected : parameter.name);
        }
        std::printf("localized_menus mode=%s ascii=%s\n", added_parameters[kMode - 1].u.pd.u.namesptr,
            added_parameters[kAsciiStyle - 1].u.pd.u.namesptr);
    }
    return valid && contour_state && chinese && chinese_names_fit && english;
}

bool widget_smoke(EffectMain entry) {
    std::array<PF_ParamDef*, kParamCount> parameters{};
    for (PF_ParamIndex index = 1; index <= added_parameter_count; ++index) parameters[index] = &added_parameters[index - 1];
    SPBasicSuite basic{};
    basic.AcquireSuite = acquire_fake_suite;
    basic.ReleaseSuite = release_fake_suite;
    PF_InData input{};
    input.pica_basicP = &basic;
    input.inter.checkout_param = fake_checkout_param;
    input.inter.checkin_param = fake_checkin_param;
    fake_app_suite.PF_ConvertLocalToGlobal = fake_convert_global;
    fake_app_suite.PF_GetMouse = fake_get_mouse;
    fake_app_suite.PF_AppColorPickerDialog = fake_color_picker;
    fake_app_suite.PF_InvalidateRect = fake_invalidate_rect;
    fake_param_utils.PF_UpdateParamUI = fake_update_param_ui;
    fake_world_suite.PF_GetPixelFormat = fake_pixel_format;
    added_parameters[kMode - 1].u.pd.value = static_cast<A_long>(OutputMode::toon_pixel);
    added_parameters[kToonModel - 1].u.pd.value = static_cast<A_long>(toonpixel::ToonModel::preserve_hue);
    added_parameters[kPaletteMode - 1].u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::style10);
    added_parameters[kDitherMode - 1].u.pd.value = static_cast<A_long>(toonpixel::DitherMode::bayer);
    added_parameters[kRampRepeat - 1].u.fs_d.value = 1.0;
    const auto canonical_ramp = make_default_settings().ramp;
    for (std::size_t index = 0; index < canonical_ramp.boundaries.size(); ++index) {
        added_parameters[kRampBoundaryParameters[index] - 1].u.fs_d.value = canonical_ramp.boundaries[index];
    }
    added_parameters[kRampSoftness - 1].u.fs_d.value = canonical_ramp.softness;

    PF_EventExtra context_event{};
    context_event.e_type = PF_Event_ACTIVATE;
    bool valid = entry(PF_Cmd_EVENT, &input, nullptr, nullptr, nullptr, &context_event) == PF_Err_NONE;
    PF_Context context{0, PF_Window_EFFECT};
    PF_ContextPtr context_pointer = &context;
    PF_ContextH context_handle = &context_pointer;
    context_event.contextH = context_handle;
    context_event.e_type = PF_Event_NEW_CONTEXT;
    valid = valid && entry(PF_Cmd_EVENT, &input, nullptr, nullptr, nullptr, &context_event) == PF_Err_NONE &&
        context.plugin_state[0] == 1;
    context_event.e_type = PF_Event_ACTIVATE;
    valid = valid && entry(PF_Cmd_EVENT, &input, nullptr, nullptr, nullptr, &context_event) == PF_Err_NONE &&
        context.plugin_state[0] == 2;
    const auto expect = [&valid](bool condition, const char* name) {
        if (!condition) std::fprintf(stderr, "FAILED %s\n", name);
        valid = valid && condition;
    };
    PF_EventExtra event{};
    PF_OutData output{};
    auto click_at = [&](float x, float y, PF_ParamIndex index = kRampBar, bool title = false) {
        event = PF_EventExtra{};
        event.contextH = context_handle;
        event.e_type = PF_Event_DO_CLICK;
        event.effect_win.index = index;
        event.effect_win.area = title ? PF_EA_PARAM_TITLE : PF_EA_CONTROL;
        event.effect_win.current_frame = index == kRampBar ? PF_LRect{40, 60, 240, 256} :
            index == kColorWheel ? PF_LRect{40, 60, 240, 172} : PF_LRect{40, 60, 240, 100};
        const auto& frame = event.effect_win.current_frame;
        event.u.do_click.screen_point = {
            frame.left + static_cast<A_long>(std::lround(x * (frame.right - frame.left))),
            frame.top + static_cast<A_long>(std::lround(y * (frame.bottom - frame.top)))};
        event.u.do_click.num_clicks = 1;
        fake_mouse = {160, 110};
        output = PF_OutData{};
        return entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event);
    };
    auto drag_at = [&](float x, float y, studio::Hit hit, float anchor_x = -1.0f) {
        event = PF_EventExtra{};
        event.contextH = context_handle;
        event.e_type = PF_Event_DRAG;
        event.effect_win.index = kRampBar;
        event.effect_win.area = PF_EA_CONTROL;
        event.effect_win.current_frame = {40, 60, 240, 256};
        event.u.do_click.screen_point = {
            static_cast<A_long>(std::lround(40.0f + x * 200.0f)),
            static_cast<A_long>(std::lround(60.0f + y * 196.0f))};
        event.u.do_click.continue_refcon[0] = kRampBar;
        event.u.do_click.continue_refcon[1] = static_cast<A_intptr_t>(hit);
        if (anchor_x < 0.0f) anchor_x = hit == studio::Hit::boundary3 ? 0.5f : 0.72f;
        event.u.do_click.continue_refcon[2] = fake_mouse.h;
        event.u.do_click.continue_refcon[3] = fake_mouse.v;
        fake_mouse.h += static_cast<A_long>(std::lround((x - anchor_x) * 200.0f));
        fake_mouse.v += static_cast<A_long>(std::lround((y - 0.2f) * 196.0f));
        event.u.do_click.last_time = true;
        output = PF_OutData{};
        return entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event);
    };
    auto drag_wheel_exposure = [&]() {
        event = PF_EventExtra{};
        event.contextH = context_handle;
        event.e_type = PF_Event_DRAG;
        event.effect_win.index = kColorWheel;
        event.effect_win.area = PF_EA_CONTROL;
        event.effect_win.current_frame = {40, 60, 240, 172};
        event.u.do_click.screen_point = {200, 66};
        event.u.do_click.continue_refcon[0] = kColorWheel;
        event.u.do_click.continue_refcon[1] = static_cast<A_intptr_t>(studio::Hit::exposure);
        event.u.do_click.continue_refcon[2] = fake_mouse.h;
        event.u.do_click.continue_refcon[3] = fake_mouse.v;
        fake_mouse.h += 20;
        event.u.do_click.last_time = true;
        output = PF_OutData{};
        return entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event);
    };

    expect(click_at(0.5f, 0.2f) == PF_Err_NONE, "preserve_hue_boundary_click");
    expect(added_parameters[kToonModel - 1].u.pd.value == 2, "edit_enables_luma_ramp");
    expect(std::fabs(added_parameters[kRampBoundary3 - 1].u.fs_d.value - 0.5f) < 0.000001f,
        "third_boundary_value");
    expect(event.u.do_click.send_drag && (output.out_flags & PF_OutFlag_FORCE_RERENDER) != 0,
        "boundary_drag_lifecycle");
    expect(event.u.do_click.continue_refcon[0] == kRampBar &&
        event.u.do_click.continue_refcon[2] == 160 && event.u.do_click.continue_refcon[3] == 110,
        "drag_mouse_capture");
    expect(drag_at(0.65f, 0.2f, studio::Hit::boundary3) == PF_Err_NONE, "global_drag");
    expect(std::fabs(added_parameters[kRampBoundary3 - 1].u.fs_d.value - 0.65f) < 0.000001f &&
        std::fabs(added_parameters[kRampBoundary1 - 1].u.fs_d.value - 0.08f) < 0.000001f &&
        std::fabs(added_parameters[kRampBoundary2 - 1].u.fs_d.value - 0.22f) < 0.000001f,
        "drag_preserves_other_boundaries");
    expect(click_at(0.3f, 0.05f) == PF_Err_NONE && added_parameters[kToonModel - 1].u.pd.value == 1,
        "preserve_model_button");
    expect(click_at(0.6f, 0.05f) == PF_Err_NONE && added_parameters[kToonModel - 1].u.pd.value == 2,
        "luma_model_button");
    expect(click_at(0.5f, 0.5f) == PF_Err_NONE &&
        std::fabs(added_parameters[kRampSoftness - 1].u.fs_d.value - 0.5f / 1.2f) < 0.0001f,
        "softness_edit");
    expect(click_at(0.9f, 0.5f) == PF_Err_NONE && added_parameters[kRampRepeat - 1].u.fs_d.value == 2.0,
        "repeat_up");
    expect(click_at(0.78f, 0.5f) == PF_Err_NONE && added_parameters[kRampRepeat - 1].u.fs_d.value == 1.0,
        "repeat_down");
    added_parameters[kToonModel - 1].u.pd.value = static_cast<A_long>(toonpixel::ToonModel::preserve_hue);
    expect(click_at(0.6f, 0.63f) == PF_Err_NONE &&
        added_parameters[kPaletteMode - 1].u.pd.value == 3, "palette_mode_edit");
    added_parameters[kPaletteCount - 1].u.fs_d.value = 4.0;
    expect(click_at(0.9f, 0.63f) == PF_Err_NONE && added_parameters[kPaletteCount - 1].u.fs_d.value == 5.0,
        "palette_count_up");
    expect(click_at(0.78f, 0.63f) == PF_Err_NONE && added_parameters[kPaletteCount - 1].u.fs_d.value == 4.0,
        "palette_count_down");
    added_parameters[kToonModel - 1].u.pd.value = static_cast<A_long>(toonpixel::ToonModel::luma_ramp);
    added_parameters[kPaletteMode - 1].u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::style10);
    expect(click_at(0.6f, 0.63f) == PF_Err_NONE &&
        added_parameters[kPaletteMode - 1].u.pd.value == static_cast<A_long>(toonpixel::PaletteMode::custom),
        "linked_palette_cycles_custom");
    added_parameters[kPaletteMode - 1].u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::style10);
    expect(click_at(0.9f, 0.63f) == PF_Err_NONE && added_parameters[kPaletteCount - 1].u.fs_d.value == 4.0,
        "stock_palette_count_unchanged");
    added_parameters[kPaletteMode - 1].u.pd.value = static_cast<A_long>(toonpixel::PaletteMode::custom);
    expect(click_at(0.1f, 0.82f) == PF_Err_NONE && added_parameters[kDitherMode - 1].u.pd.value == 3,
        "dither_mode_edit");
    expect(click_at(0.1f, 0.82f) == PF_Err_NONE && added_parameters[kDitherMode - 1].u.pd.value == 1,
        "dither_mode_off");
    expect(click_at(0.8f, 0.84f) == PF_Err_NONE && added_parameters[kDitherMatrix - 1].u.pd.value == 2 &&
        added_parameters[kDitherMode - 1].u.pd.value == 2, "matrix_enables_bayer");
    expect(click_at(0.3f, 0.95f) == PF_Err_NONE &&
        std::fabs(added_parameters[kDitherThreshold - 1].u.fs_d.value - 0.3f / 0.62f) < 0.0001f,
        "threshold_edit");

    picker_accept = true;
    expect(click_at(0.95f, 0.37f) == PF_Err_NONE, "ramp_color_accept");
    expect(added_parameters[kRampColor5 - 1].u.cd.value.blue > added_parameters[kRampColor5 - 1].u.cd.value.red,
        "fifth_ramp_color_write");
    const auto previous = added_parameters[kRampColor5 - 1].u.cd.value;
    added_parameters[kToonModel - 1].u.pd.value = 1;
    picker_accept = false;
    expect(click_at(0.95f, 0.37f) == PF_Err_NONE, "ramp_color_cancel");
    const auto current = added_parameters[kRampColor5 - 1].u.cd.value;
    expect(previous.red == current.red && previous.green == current.green && previous.blue == current.blue &&
        added_parameters[kToonModel - 1].u.pd.value == 1, "cancel_preserves_color_and_model");
    picker_accept = true;

    expect(click_at(0.14f, 0.25f, kColorWheel) == PF_Err_NONE, "wheel_center");
    const auto expected = toonpixel::oklab_to_linear({0.72f, 0.0f, 0.0f});
    expect(added_parameters[kTintColor - 1].u.cd.value.red == studio::channel(expected.red) &&
        added_parameters[kTintColor - 1].u.cd.value.green == studio::channel(expected.green) &&
        added_parameters[kTintColor - 1].u.cd.value.blue == studio::channel(expected.blue), "wheel_neutral_color");
    expect(click_at(0.14f, 0.85f, kColorWheel) == PF_Err_NONE &&
        std::fabs(added_parameters[kTintAmount - 1].u.fs_d.value - 0.5f) < 0.0001f,
        "tint_amount_edit");
    expect(click_at(0.9f, 0.05f, kColorWheel) == PF_Err_NONE &&
        std::fabs(added_parameters[kExposure - 1].u.fs_d.value - 14.0f / 13.0f) < 0.0001f,
        "exposure_edit");

    expect(drag_at(0.8f, 0.2f, studio::Hit::boundary4) == PF_Err_NONE, "fourth_boundary_drag");
    expect(std::fabs(added_parameters[kRampBoundary4 - 1].u.fs_d.value - 0.8f) < 0.000001f &&
        std::fabs(added_parameters[kRampBoundary3 - 1].u.fs_d.value - 0.65f) < 0.000001f,
        "fourth_boundary_preserves_third");
    added_parameters[kRampBoundary3 - 1].u.fs_d.value = 0.60f;
    added_parameters[kRampBoundary4 - 1].u.fs_d.value = 0.72f;
    expect(click_at(0.74f, 0.2f) == PF_Err_NONE &&
        std::fabs(added_parameters[kRampBoundary4 - 1].u.fs_d.value - 0.72f) < 0.000001f &&
        event.u.do_click.continue_refcon[2] == 160, "near_handle_preserves_anchor");
    const auto near_drag_error = drag_at(0.67f, 0.2f, studio::Hit::boundary4, 0.74f);
    const bool near_drag_correct = near_drag_error == PF_Err_NONE &&
        std::fabs(added_parameters[kRampBoundary4 - 1].u.fs_d.value - 0.65f) < 0.000001f &&
        std::fabs(added_parameters[kRampBoundary3 - 1].u.fs_d.value - 0.60f) < 0.000001f &&
        event.u.do_click.continue_refcon[2] == 146;
    if (!near_drag_correct) {
        std::fprintf(stderr, "near_drag fourth=%.9f third=%.9f error=%ld\n",
            added_parameters[kRampBoundary4 - 1].u.fs_d.value,
            added_parameters[kRampBoundary3 - 1].u.fs_d.value,
            static_cast<long>(near_drag_error));
    }
    expect(near_drag_correct, "near_handle_drag_from_anchor");
    added_parameters[kRampBoundary3 - 1].u.fs_d.value = 0.60f;
    added_parameters[kRampBoundary4 - 1].u.fs_d.value = 0.72f;
    const int handle_bottom_picker_calls = picker_calls;
    expect(click_at(0.74f, 0.32f) == PF_Err_NONE &&
        std::fabs(added_parameters[kRampBoundary4 - 1].u.fs_d.value - 0.72f) < 0.000001f &&
        picker_calls == handle_bottom_picker_calls, "handle_bottom_click_does_not_open_picker");
    expect(click_at(0.72f, 0.2f) == PF_Err_NONE, "stationary_handle_click");
    event.e_type = PF_Event_DRAG;
    event.u.do_click.last_time = true;
    output = PF_OutData{};
    const PF_Err stationary_drag_error = entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event);
    const float stationary_boundary = added_parameters[kRampBoundary4 - 1].u.fs_d.value;
    if (stationary_drag_error != PF_Err_NONE || std::fabs(stationary_boundary - 0.72f) >= 0.000001f) {
        std::fprintf(stderr, "stationary_drag fourth=%.9f error=%ld\n",
            stationary_boundary, static_cast<long>(stationary_drag_error));
    }
    expect(stationary_drag_error == PF_Err_NONE && std::fabs(stationary_boundary - 0.72f) < 0.000001f,
        "stationary_drag_preserves_handle");
    fake_mouse.h = 146;
    output = PF_OutData{};
    const PF_Err moved_mouse_error = entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event);
    expect(moved_mouse_error == PF_Err_NONE &&
        std::fabs(added_parameters[kRampBoundary4 - 1].u.fs_d.value - 0.65f) < 0.000001f &&
        event.u.do_click.screen_point.h == 184 && event.u.do_click.continue_refcon[2] == 146,
        "mouse_delta_ignores_mixed_event_coordinates");
    added_parameters[kDitherMode - 1].u.pd.value = 1;
    expect(click_at(0.4f, 0.95f) == PF_Err_NONE &&
        added_parameters[kDitherMode - 1].u.pd.value == 2 &&
        std::fabs(added_parameters[kDitherThreshold - 1].u.fs_d.value - 0.4f / 0.62f) < 0.0001f,
        "threshold_enables_dither");
    added_parameters[kPaletteMode - 1].u.pd.value = 2;
    expect(click_at(0.05f, 0.72f) == PF_Err_NONE, "custom_palette_accept");
    expect(added_parameters[kPaletteMode - 1].u.pd.value == 3 &&
        added_parameters[kCustomColor1 - 1].u.cd.value.blue > added_parameters[kCustomColor1 - 1].u.cd.value.red,
        "custom_palette_color_write");
    const auto custom_previous = added_parameters[kCustomColor2 - 1].u.cd.value;
    picker_accept = false;
    expect(click_at(0.15f, 0.72f) == PF_Err_NONE, "custom_palette_cancel");
    const auto custom_current = added_parameters[kCustomColor2 - 1].u.cd.value;
    expect(custom_previous.red == custom_current.red && custom_previous.green == custom_current.green &&
        custom_previous.blue == custom_current.blue, "custom_palette_cancel_preserves_color");
    picker_accept = true;
    expect(click_at(0.9f, 0.30f, kColorWheel) == PF_Err_NONE &&
        std::fabs(added_parameters[kVibrance - 1].u.fs_d.value - 7.0f / 13.0f) < 0.0001f,
        "vibrance_edit");
    expect(click_at(0.9f, 0.55f, kColorWheel) == PF_Err_NONE &&
        std::fabs(added_parameters[kHue - 1].u.fs_d.value - 1260.0f / 13.0f) < 0.0001f,
        "hue_edit");
    expect(click_at(0.9f, 0.80f, kColorWheel) == PF_Err_NONE &&
        std::fabs(added_parameters[kChroma - 1].u.fs_d.value - 20.0f / 13.0f) < 0.0001f,
        "chroma_edit");
    const float wheel_anchor = (studio::slider_track_left(200) +
        (198.0f - studio::slider_track_left(200)) * 0.5f) / 200.0f;
    const float expected_exposure = 0.4f;
    expect(click_at(wheel_anchor, 0.05f, kColorWheel) == PF_Err_NONE &&
        std::fabs(added_parameters[kExposure - 1].u.fs_d.value) < 0.0001f, "wheel_anchor_click");
    expect(drag_wheel_exposure() == PF_Err_NONE &&
        std::fabs(added_parameters[kExposure - 1].u.fs_d.value - expected_exposure) < 0.0001f &&
        event.u.do_click.continue_refcon[2] == 180, "wheel_drag_uses_mouse_delta");

    std::array<PF_PixelFloat, 100> gradient{};
    for (int index = 0; index < 100; ++index) {
        const float value = static_cast<float>(index) / 99.0f;
        gradient[index] = {1.0f, value, value, value};
    }
    fake_input_world = PF_EffectWorld{};
    fake_input_world.data = reinterpret_cast<PF_PixelPtr>(gradient.data());
    fake_input_world.width = 100;
    fake_input_world.height = 1;
    fake_input_world.rowbytes = sizeof(gradient);
    expect(click_at(0.9f, 0.05f) == PF_Err_NONE, "analyze_scene");
    const auto* analysis = reinterpret_cast<const scene::Analysis*>(context.plugin_state[2]);
    expect(analysis && analysis->valid && !analysis->uniform, "analysis_valid");
    if (analysis) {
        for (std::size_t index = 0; index < analysis->boundaries.size(); ++index) {
            expect(std::fabs(added_parameters[kRampBoundaryParameters[index] - 1].u.fs_d.value -
                analysis->boundaries[index]) < 0.0001f, "analysis_parameter_match");
        }
    }
    const auto detected = added_parameters[kRampBoundary1 - 1].u.fs_d.value;
    for (auto& pixel : gradient) pixel = {1.0f, 0.25f, 0.25f, 0.25f};
    expect(click_at(0.9f, 0.05f) == PF_Err_NONE && reinterpret_cast<const scene::Analysis*>(context.plugin_state[2])->uniform,
        "uniform_scene");
    expect(added_parameters[kRampBoundary1 - 1].u.fs_d.value == detected, "uniform_preserves_parameters");
    for (auto& pixel : gradient) pixel.alpha = 0.0f;
    expect(click_at(0.9f, 0.05f) == PF_Err_NONE && !reinterpret_cast<const scene::Analysis*>(context.plugin_state[2])->valid,
        "transparent_scene");
    expect(added_parameters[kRampBoundary1 - 1].u.fs_d.value == detected, "empty_preserves_parameters");

    added_parameters[kAsciiEnabled - 1].u.bd.value = 1;
    arbitrary::replace(&added_parameters[kAsciiRamp - 1], {32, 46, 58, 45, 61, 43, 42, 35, 37, 64}, 0);
    expect(click_at(0.07f, 0.4f, kAsciiEditor) == PF_Err_NONE &&
        arbitrary::get(added_parameters[kAsciiRamp - 1].u.arb_d.value)->selected == 1 &&
        context.plugin_state[1] == kAsciiEditor && !event.u.do_click.send_drag,
        "ascii_click_establishes_keyboard_focus");
    event = PF_EventExtra{};
    event.contextH = context_handle;
    event.e_type = PF_Event_KEYDOWN;
    event.u.key_down.keycode = PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CHARACTER('Q');
    expect(entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event) == PF_Err_NONE &&
        arbitrary::codepoints(added_parameters[kAsciiRamp - 1].u.arb_d.value)[1] == 81,
        "ascii_character_edit");
    event.u.key_down.keycode = PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CONTROL_CODE(PF_ControlCode_Backspace);
    expect(entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event) == PF_Err_NONE &&
        arbitrary::codepoints(added_parameters[kAsciiRamp - 1].u.arb_d.value).size() == 9,
        "ascii_backspace");
    event.u.key_down.keycode = PF_GENERATE_KEYBOARD_CODE_VALUE_FROM_CONTROL_CODE(PF_ControlCode_Return);
    expect(entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event) == PF_Err_NONE &&
        arbitrary::codepoints(added_parameters[kAsciiRamp - 1].u.arb_d.value).size() == 10,
        "ascii_insert");
    const auto title_dither_mode = added_parameters[kDitherMode - 1].u.pd.value;
    const int title_picker_calls = picker_calls;
    const auto title_hit = reinterpret_cast<studio::InteractionState*>(context.plugin_state[3])->requested_hit;
    expect(click_at(0.95f, 0.95f, kRampBar, true) == PF_Err_NONE && event.evt_out_flags == 0 &&
        added_parameters[kDitherMode - 1].u.pd.value == title_dither_mode && picker_calls == title_picker_calls &&
        reinterpret_cast<studio::InteractionState*>(context.plugin_state[3])->requested_hit == title_hit,
        "title_area_does_not_change_effect");
    event = PF_EventExtra{};
    event.contextH = context_handle;
    event.e_type = PF_Event_ADJUST_CURSOR;
    event.effect_win.index = kColorWheel;
    event.effect_win.area = PF_EA_CONTROL;
    event.effect_win.current_frame = {40, 60, 240, 172};
    event.u.adjust_cursor.screen_point = {220, 66};
    output = PF_OutData{};
    const int hover_invalidations = invalidate_calls;
    expect(entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event) == PF_Err_NONE &&
        invalidate_calls > hover_invalidations && invalidated_rect.right - invalidated_rect.left == 200 &&
        invalidated_rect.bottom - invalidated_rect.top == 112, "hover_local_invalidation");
    auto* feedback = reinterpret_cast<studio::InteractionState*>(context.plugin_state[3]);
    expect(feedback && feedback->requested_hit == studio::Hit::exposure, "hover_target_matches_control");
    if (feedback) feedback->started -= 1.0;
    const int settled_invalidations = invalidate_calls;
    event = PF_EventExtra{};
    event.contextH = context_handle;
    event.e_type = PF_Event_IDLE;
    output = PF_OutData{};
    expect(entry(PF_Cmd_EVENT, &input, &output, parameters.data(), nullptr, &event) == PF_Err_NONE &&
        invalidate_calls == settled_invalidations &&
        (output.out_flags & PF_OutFlag_FORCE_RERENDER) == 0, "idle_stops_without_comp_render");
    context_event = PF_EventExtra{};
    context_event.contextH = context_handle;
    context_event.e_type = PF_Event_CLOSE_CONTEXT;
    expect(entry(PF_Cmd_EVENT, &input, nullptr, nullptr, nullptr, &context_event) == PF_Err_NONE &&
        context.plugin_state[2] == 0 && context.plugin_state[3] == 0, "analysis_context_cleanup");
    std::printf("widget_boundary=%.3f detected=%.6f valid=%d\n",
        added_parameters[kRampBoundary3 - 1].u.fs_d.value, detected, valid ? 1 : 0);
    return valid;
}

int register_ui_calls = 0;
PF_Err fake_register_ui(PF_ProgPtr, PF_CustomUIInfo* info) {
    if (!info || info->events != PF_CustomEFlag_EFFECT) return PF_Err_BAD_CALLBACK_PARAM;
    ++register_ui_calls;
    return PF_Err_NONE;
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: toon_native_entry_smoke <plugin-binary>\n");
        return 1;
    }
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    const auto entry = reinterpret_cast<EffectMain>(dlsym(library, "EffectMain"));
    if (!entry) {
        std::fprintf(stderr, "dlsym EffectMain failed: %s\n", dlerror());
        dlclose(library);
        return 1;
    }
    PF_InData input{};
    PF_OutData output{};
    input.inter.register_ui = fake_register_ui;
    input.version.major = PF_PLUG_IN_VERSION;
    input.version.minor = PF_PLUG_IN_SUBVERS;
    const PF_Err global_error = entry(PF_Cmd_GLOBAL_SETUP, &input, &output, nullptr, nullptr, nullptr);
    const bool global_valid = global_error == PF_Err_NONE && register_ui_calls == 0 &&
        output.my_version == PF_VERSION(0, 1, 2, PF_Stage_RELEASE, 1) &&
        (output.out_flags & PF_OutFlag_I_EXPAND_BUFFER) != 0 &&
        (output.out_flags & PF_OutFlag_SEND_UPDATE_PARAMS_UI) != 0 &&
        (output.out_flags & PF_OutFlag_CUSTOM_UI) != 0 &&
        (output.out_flags & PF_OutFlag_I_USE_AUDIO) != 0 &&
        (output.out_flags & PF_OutFlag_PIX_INDEPENDENT) == 0 &&
        output.out_flags2 == (PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE);
    std::printf(
        "entry_error=%ld version=%lx flags=%lx flags2=%lx\n",
        static_cast<long>(global_error),
        static_cast<unsigned long>(output.my_version),
        static_cast<unsigned long>(output.out_flags),
        static_cast<unsigned long>(output.out_flags2));
    input.num_params = 1;
    input.inter.add_param = add_parameter;
    PF_UtilCallbacks utilities{};
    utilities.ansi.strcpy = std::strcpy;
    utilities.host_new_handle = fake_host_new_handle;
    utilities.host_dispose_handle = fake_host_dispose_handle;
    input.utils = &utilities;
    const PF_Err params_error = entry(PF_Cmd_PARAMS_SETUP, &input, &output, nullptr, nullptr, nullptr);
    bool unique_parameter_names = true;
    for (int first = 0; first < added_parameter_count; ++first) {
        if (added_parameters[first].param_type == PF_Param_GROUP_END) {
            continue;
        }
        for (int second = first + 1; second < added_parameter_count; ++second) {
            if (added_parameters[second].param_type != PF_Param_GROUP_END &&
                std::strcmp(added_parameters[first].name, added_parameters[second].name) == 0) {
                unique_parameter_names = false;
            }
        }
    }
    const bool params_valid = unique_parameter_names && params_error == PF_Err_NONE && register_ui_calls == 1 &&
        added_parameter_count == kAddedParamCount && output.num_params == kParamCount &&
        std::strcmp(added_parameters[kMode - 1].name, "Mode") == 0 &&
        std::strcmp(added_parameters[kToonStrength - 1].name, "Toon Strength") == 0 &&
        std::strcmp(added_parameters[kPixelSize - 1].name, "Pixel Size") == 0 &&
        std::strcmp(added_parameters[kDither - 1].name, "Dither") == 0 &&
        std::strcmp(added_parameters[kOutline - 1].name, "Outline") == 0 &&
        std::strcmp(added_parameters[kEdgeThreshold - 1].name, "Edge Threshold") == 0 &&
        (added_parameters[kMode - 1].flags & PF_ParamFlag_SUPERVISE) != 0 &&
        added_parameters[kMode - 1].u.pd.value == 3 &&
        added_parameters[kToonStrength - 1].u.fs_d.value == 1.0 &&
        added_parameters[kOutline - 1].param_type == PF_Param_CHECKBOX &&
        added_parameters[kOutline - 1].u.bd.value == 1 &&
        added_parameters[kToonTopic - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kToonTopicEnd - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kGradeTopic - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kGradeTopicEnd - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kPixelTopic - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kPixelTopicEnd - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kCustomTopic - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kCustomTopicEnd - 1].param_type == PF_Param_NO_DATA &&
        added_parameters[kRampBoundary4 - 1].param_type == PF_Param_FLOAT_SLIDER &&
        added_parameters[kRampColor5 - 1].param_type == PF_Param_COLOR &&
        std::strcmp(added_parameters[kRampColor5 - 1].name, "Ramp Color 5") == 0 &&
        added_parameters[kToonModel - 1].u.pd.value == static_cast<A_long>(toonpixel::ToonModel::preserve_hue) &&
        added_parameters[kPaletteMode - 1].u.pd.value == static_cast<A_long>(toonpixel::PaletteMode::style10) &&
        added_parameters[kDitherMode - 1].u.pd.value == static_cast<A_long>(toonpixel::DitherMode::bayer) &&
        added_parameters[kPixelOutline - 1].u.pd.value == static_cast<A_long>(toonpixel::PixelOutlineMode::grid_edge) &&
        added_parameters[kRampColor1 - 1].param_type == PF_Param_COLOR &&
        added_parameters[kCustomColor10 - 1].param_type == PF_Param_COLOR &&
        added_parameters[kWidgetTopic - 1].param_type == PF_Param_GROUP_START &&
        (added_parameters[kToonTopic - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0 &&
        (added_parameters[kToonModel - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0 &&
        (added_parameters[kGradeTopic - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0 &&
        (added_parameters[kToonTopicEnd - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0 &&
        (added_parameters[kGradeTopicEnd - 1].ui_flags & PF_PUI_NO_ECW_UI) != 0 &&
        (added_parameters[kWidgetTopic - 1].ui_flags & PF_PUI_NO_ECW_UI) == 0 &&
        added_parameters[kRampBar - 1].param_type == PF_Param_ARBITRARY_DATA &&
        added_parameters[kColorWheel - 1].param_type == PF_Param_ARBITRARY_DATA &&
        added_parameters[kColorWheel - 1].ui_height == 112 &&
        (added_parameters[kRampBar - 1].ui_flags & (PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL)) ==
            (PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL) &&
        (added_parameters[kColorWheel - 1].ui_flags & (PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL)) ==
            (PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL) &&
        (added_parameters[kRampBar - 1].ui_flags & PF_PUI_STD_CONTROL_ONLY) == 0 &&
        (added_parameters[kColorWheel - 1].ui_flags & PF_PUI_STD_CONTROL_ONLY) == 0 &&
        (added_parameters[kRampBar - 1].flags & PF_ParamFlag_SUPERVISE) != 0 &&
        (added_parameters[kColorWheel - 1].flags & PF_ParamFlag_SUPERVISE) != 0 &&
        added_parameters[kWidgetTopicEnd - 1].param_type == PF_Param_GROUP_END &&
        added_parameters[kAsciiRamp - 1].param_type == PF_Param_ARBITRARY_DATA &&
        added_parameters[kAsciiEditor - 1].param_type == PF_Param_ARBITRARY_DATA &&
        (added_parameters[kAsciiEditor - 1].ui_flags & PF_PUI_CONTROL) != 0 &&
        added_parameters[kAsciiAudioLayer - 1].param_type == PF_Param_LAYER &&
        added_parameters[kContourMode - 1].param_type == PF_Param_POPUP &&
        added_parameters[kLanguage - 1].param_type == PF_Param_POPUP &&
        std::strcmp(added_parameters[kLanguage - 1].name, "Language / 语言") == 0 &&
        added_parameters[kLanguage - 1].u.pd.value == localization::english &&
        std::strcmp(added_parameters[kLanguage - 1].u.pd.u.namesptr, "English|中文") == 0 &&
        added_parameters[kContourTopicEnd - 1].param_type == PF_Param_GROUP_END;
    if (!params_valid) {
        std::fprintf(stderr, "unique_names=%d markers=%d,%d,%d,%d,%d,%d,%d,%d new=%d,%d name5=%s\n",
            unique_parameter_names ? 1 : 0,
            added_parameters[kToonTopic - 1].param_type, added_parameters[kToonTopicEnd - 1].param_type,
            added_parameters[kPixelTopic - 1].param_type, added_parameters[kPixelTopicEnd - 1].param_type,
            added_parameters[kGradeTopic - 1].param_type, added_parameters[kGradeTopicEnd - 1].param_type,
            added_parameters[kCustomTopic - 1].param_type, added_parameters[kCustomTopicEnd - 1].param_type,
            added_parameters[kRampBoundary4 - 1].param_type, added_parameters[kRampColor5 - 1].param_type,
            added_parameters[kRampColor5 - 1].name);
        for (int index = 0; index < added_parameter_count; ++index) {
            const auto& parameter = added_parameters[index];
            std::fprintf(stderr, "registered=%d id=%ld type=%d name=%s\n", index + 1,
                static_cast<long>(parameter.uu.id), static_cast<int>(parameter.param_type), parameter.name);
        }
        for (int index = kWidgetTopic; index <= kWidgetTopicEnd; ++index) {
            const auto& parameter = added_parameters[index - 1];
            std::fprintf(stderr, "widget index=%d type=%d flags=%x w=%d h=%d name=%s\n",
                index, static_cast<int>(parameter.param_type), parameter.ui_flags,
                parameter.ui_width, parameter.ui_height, parameter.name);
        }
    }
    std::printf(
        "params_error=%ld count=%ld added=%d mode_type=%d mode=%ld valid=%d\n",
        static_cast<long>(params_error), static_cast<long>(output.num_params),
        added_parameter_count, static_cast<int>(added_parameters[0].param_type),
        static_cast<long>(added_parameters[0].u.pd.value), params_valid ? 1 : 0);
    arbitrary::RampData default_ramp;
    const bool ramp_valid = params_valid && host_alloc_calls > 0 &&
        added_parameters[kAsciiRamp - 1].param_type == PF_Param_ARBITRARY_DATA &&
        arbitrary::validate(arbitrary::get(added_parameters[kAsciiRamp - 1].u.arb_d.dephault)) == PF_Err_NONE &&
        std::memcmp(arbitrary::get(added_parameters[kAsciiRamp - 1].u.arb_d.dephault), &default_ramp, sizeof(default_ramp)) == 0;
    std::printf("arbitrary_ramp_valid=%d\n", ramp_valid ? 1 : 0);
    std::printf("initial_studio_flags model=%x tint=%x ramp=%x topic=%x\n",
        added_parameters[kToonModel - 1].ui_flags, added_parameters[kTintColor - 1].ui_flags,
        added_parameters[kRampColor1 - 1].ui_flags, added_parameters[kWidgetTopic - 1].ui_flags);
    const bool alpha_valid = alpha_smart_render_smoke(entry);
    const bool feature_valid = feature_render_smoke(entry);
    const bool ui_valid = ui_smoke(entry);
    const bool widget_valid = widget_smoke(entry);
    dlclose(library);
    return global_valid && params_valid && ramp_valid && alpha_valid && feature_valid && ui_valid && widget_valid ? 0 : 1;
}
