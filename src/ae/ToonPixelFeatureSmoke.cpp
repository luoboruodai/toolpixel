#if defined(__APPLE__) && defined(__MACH__)
#include <CoreServices/CoreServices.h>
#else
#include "ToonPixelLegacyTypes.h"
#endif
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "ToonMath.h"
#include "ToonPixelSettings.h"
#include "ToonPixelLocalization.h"
#include "ToonPixelArb.h"
#include "ToonPixelGlyphs.h"
struct StraightPixel;
#include "ToonPixelStudio.h"
struct StraightPixel { float alpha, red, green, blue; };
template <typename Pixel>
StraightPixel sample_straight(const PF_EffectWorld* world, A_long x, A_long y) {
    x = std::clamp<A_long>(x, 0, world->width - 1);
    y = std::clamp<A_long>(y, 0, world->height - 1);
    const auto* row = reinterpret_cast<const Pixel*>(
        reinterpret_cast<const unsigned char*>(world->data) + y * world->rowbytes);
    const auto pixel = row[x];
    return {pixel.alpha, pixel.red, pixel.green, pixel.blue};
}
#include "ToonPixelScene.h"
#include "ToonPixelContour.h"
#define TOONPIXEL_LOG(...) ((void)0)
#include "ToonPixelAudio.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

std::vector<int16_t> samples{8192, 16384, -8192, -16384};
int checkin_count = 0;
PF_UFixed requested_rate = 0;

PF_Err checkout_audio(PF_ProgPtr, PF_ParamIndex, A_long, A_long, A_u_long,
    PF_UFixed rate, A_long, A_long, A_long, PF_LayerAudio* audio) {
    if (!audio) return PF_Err_BAD_CALLBACK_PARAM;
    requested_rate = rate;
    *audio = reinterpret_cast<PF_LayerAudio>(samples.data());
    return PF_Err_NONE;
}

PF_Err checkin_audio(PF_ProgPtr, PF_LayerAudio) {
    ++checkin_count;
    return PF_Err_NONE;
}

PF_Err get_audio(PF_ProgPtr, PF_LayerAudio, PF_SndSamplePtr* data, A_long* count,
    PF_UFixed* rate, A_long* bytes, A_long* channels, A_long* format) {
    *data = samples.data();
    *count = static_cast<A_long>(samples.size());
    *rate = requested_rate;
    *bytes = PF_SSS_2;
    *channels = PF_Channels_MONO;
    *format = PF_SIGNED_PCM;
    return PF_Err_NONE;
}

bool near(float a, float b, float tolerance = 0.0001f) {
    return std::fabs(a - b) < tolerance;
}

bool rgb_in_gamut(const toonpixel::Rgb& color) {
    return toonpixel::in_gamut(color, 0.00002f);
}

}  // namespace

int main() {
    bool valid = true;
    const auto expect = [&valid](bool condition, const char* name) {
        if (!condition) std::fprintf(stderr, "FAILED %s\n", name);
        valid = valid && condition;
    };
    for (const auto& entry : localization::kParameterNames) {
        expect(std::strlen(entry.english) <= 31 && std::strlen(entry.chinese) <= 31, "localized_parameter_name_fits");
    }
    expect(std::strcmp(localization::parameter_name(kMode, localization::chinese), "模式") == 0, "localized_mode_name");
    expect(localization::utf16_from_utf8("中文 ABC") == std::u16string(u"中文 ABC"), "utf16_chinese");
    expect(localization::utf16_from_utf8("\xF0\x9F\x98\x80").size() == 2, "utf16_surrogate_pair");
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        expect(toonpixel::ordered_threshold(x, y, 2) == toonpixel::bayer_threshold(x, y), "bayer4_backward_compatible");
    }
    expect(toonpixel::bayer_index(0, 0, 3) == 0, "bayer8_origin");
    expect(toonpixel::bayer_index(1, 0, 3) == 32, "bayer8_next");
    expect(toonpixel::bayer_index(7, 7, 3) == 21, "bayer8_recursive_matrix");
    bool seen[64]{};
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        const int index = toonpixel::bayer_index(x, y, 3);
        expect(index >= 0 && index < 64 && !seen[index], "bayer8_unique_thresholds");
        if (index >= 0 && index < 64) seen[index] = true;
    }
    for (const bool found : seen) expect(found, "bayer8_complete");
    toonpixel::DecaySettings decay;
    decay.shape = 2;
    decay.amount = 1.0f;
    decay.scale = 0.5f;
    expect(near(toonpixel::dither_decay(0.5f, 0.5f, 1.0f, decay), 1.0f), "decay_center");
    expect(near(toonpixel::dither_decay(1.0f, 0.5f, 1.0f, decay), 0.0f), "decay_end");
    decay.shape = 3;
    expect(near(toonpixel::dither_decay(0.75f, 0.5f, 1.0f, decay), 0.5f), "decay_radial");
    expect(near(toonpixel::dither_decay(0.5f, 0.5f, 1.0f, decay), 1.0f), "decay_radial_center");

    for (const float gray : {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f}) {
        for (const int control : {0, 1, 2}) {
            toonpixel::GradeSettings grade;
            if (control == 0) grade.hue = 65.0f;
            if (control == 1) grade.vibrance = 0.65f;
            if (control == 2) grade.chroma = 0.1f;
            const auto neutral = toonpixel::apply_color_grade({gray, gray, gray}, grade);
            const bool neutral_ok = near(neutral.red, gray, 0.000001f) && near(neutral.green, gray, 0.000001f) &&
                near(neutral.blue, gray, 0.000001f);
            if (!neutral_ok) {
                const auto lab = toonpixel::linear_to_oklab({gray, gray, gray});
                std::fprintf(stderr, "neutral control=%d gray=%.9f rgb=%.9f,%.9f,%.9f lab=%.9f,%.9f,%.9f\n",
                    control, gray, neutral.red, neutral.green, neutral.blue, lab.l, lab.a, lab.b);
            }
            expect(neutral_ok, "neutral_gray_color_controls");
        }
    }
    auto mild = toonpixel::linear_to_oklab({0.64f, 0.75f, 0.9f});
    mild.a *= 2.0f;
    mild.b *= 2.0f;
    const auto mild_unclipped = toonpixel::oklab_to_linear_unclamped(mild);
    const auto mild_off = toonpixel::apply_gamut(mild_unclipped, 1);
    const auto mild_adaptive = toonpixel::apply_gamut(mild_unclipped, 2);
    const auto mild_clip = toonpixel::apply_gamut(mild_unclipped, 3);
    const auto mild_truncate = toonpixel::apply_gamut(mild_unclipped, 4);
    std::fprintf(stderr, "mild input=%.9f,%.9f,%.9f off=%.9f,%.9f,%.9f adaptive=%.9f,%.9f,%.9f clip=%.9f,%.9f,%.9f truncate=%.9f,%.9f,%.9f\n",
        mild_unclipped.red, mild_unclipped.green, mild_unclipped.blue,
        mild_off.red, mild_off.green, mild_off.blue,
        mild_adaptive.red, mild_adaptive.green, mild_adaptive.blue,
        mild_clip.red, mild_clip.green, mild_clip.blue,
        mild_truncate.red, mild_truncate.green, mild_truncate.blue);
    auto margin_lab = mild;
    margin_lab.a *= 0.5f;
    margin_lab.b *= 0.5f;
    const auto margin_input = toonpixel::oklab_to_linear_unclamped(margin_lab);
    const auto margin_base = toonpixel::apply_gamut(margin_input, 3);
    const auto mild_margin = toonpixel::apply_gamut(margin_input, 3, 0.2f);
    std::fprintf(stderr, "mild_margin chroma0=%.9f chroma02=%.9f base=%.9f,%.9f,%.9f rgb=%.9f,%.9f,%.9f\n",
        toonpixel::gamut_chroma(margin_lab), toonpixel::gamut_chroma(margin_lab, 0.2f),
        margin_base.red, margin_base.green, margin_base.blue,
        mild_margin.red, mild_margin.green, mild_margin.blue);
    expect(!toonpixel::in_gamut(mild_unclipped), "mild_gamut_fixture_outside");
    expect(toonpixel::in_gamut(margin_input) && std::fabs(mild_margin.blue - margin_base.blue) > 0.0001f,
        "gamut_margin_changes_output");
    expect(rgb_in_gamut(mild_adaptive) && rgb_in_gamut(mild_clip), "mild_gamut_mapping_bounded");
    expect(mild_off.blue > 1.0f && mild_truncate.blue == 1.0f &&
        std::fabs(mild_off.blue - mild_adaptive.blue) > 0.0001f &&
        std::fabs(mild_off.red - mild_clip.red) > 0.0001f &&
        std::fabs(mild_off.blue - mild_truncate.blue) > 0.0001f, "mild_gamut_modes_distinct");

    const toonpixel::Oklab saturated{0.65f, 0.3f, -0.12f};
    const auto clipped = toonpixel::apply_gamut(toonpixel::oklab_to_linear_unclamped(saturated), 3);
    const auto truncated = toonpixel::apply_gamut(toonpixel::oklab_to_linear_unclamped(saturated), 4);
    const auto debug = toonpixel::apply_gamut(toonpixel::oklab_to_linear_unclamped(saturated), 5);
    expect(rgb_in_gamut(clipped) && rgb_in_gamut(truncated), "gamut_bounded");
    expect(!rgb_in_gamut(toonpixel::oklab_to_linear_unclamped(saturated)), "gamut_fixture_outside");
    expect(debug.red == 1.0f && debug.green == 0.0f && debug.blue > 0.5f, "gamut_debug");
    const toonpixel::Rgb original{0.2f, 0.4f, 0.6f};
    const auto unchanged = toonpixel::apply_gamut(original, 2);
    expect(near(original.red, unchanged.red) && near(original.green, unchanged.green) && near(original.blue, unchanged.blue), "gamut_identity");

    const std::vector<toonpixel::Oklab> cells(4, toonpixel::linear_to_oklab({0.25f, 0.25f, 0.25f}));
    const std::vector<float> alpha(4, 1.0f);
    const auto palette = toonpixel::palette_oklab({{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    const auto normal = toonpixel::floyd_steinberg(cells, alpha, 2, 2, palette, 1.0f);
    const auto attenuated = toonpixel::floyd_steinberg(cells, alpha, 2, 2, palette, 1.0f, 0.5f, {0.0f, 0.0f, 0.0f, 0.0f});
    expect(normal.size() == 4 && normal[0] == 1 && normal[1] == 0, "floyd_reference");
    expect(attenuated == std::vector<std::size_t>({1, 1, 1, 1}), "floyd_decay");

    const auto glyph_a = glyphs::glyph_for('A', 2);
    expect(glyphs::coverage(glyph_a, 0.5f, 0.5f) > 0.0f, "glyph_A_coverage");
    const auto glyph_hash = glyphs::glyph_for('#', 2);
    expect(glyph_hash[2] != 0 && glyph_hash[4] != 0, "glyph_hash_rows");
    expect(glyphs::glyph_for(0x2588, 7)[0] == 0xFF, "glyph_full_block");
    expect(glyphs::glyph_for(0x2803, 10)[0] == 0x33, "glyph_braille");
    for (int style = 1; style <= 14; ++style) {
        const auto ramp = glyphs::active_ramp(style, {32, 'X', '#'});
        expect(!ramp.empty() && ramp.size() <= 64, "style_ramp_valid");
        const auto glyph = glyphs::glyph_for(ramp.back(), style);
        bool occupied = false;
        for (const auto row : glyph) occupied = occupied || row != 0;
        expect(occupied, "style_glyph_nonempty");
    }

    arbitrary::RampData initial;
    initial.count = 3;
    initial.selected = 1;
    initial.codepoints[0] = ' ';
    initial.codepoints[1] = 'X';
    initial.codepoints[2] = 0x2588;
    PF_ArbitraryH handle = nullptr;
    expect(arbitrary::create(initial, &handle) == PF_Err_NONE, "arb_create");
    PF_ArbParamsExtra extra{};
    extra.which_function = PF_Arbitrary_FLAT_SIZE_FUNC;
    extra.u.flat_size_func_params.arbH = handle;
    A_u_long size = 0;
    extra.u.flat_size_func_params.flat_data_sizePLu = &size;
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE && size == sizeof(arbitrary::RampData), "arb_flat_size");
    std::vector<unsigned char> flattened(size);
    extra.which_function = PF_Arbitrary_FLATTEN_FUNC;
    extra.u.flatten_func_params.arbH = handle;
    extra.u.flatten_func_params.buf_sizeLu = size;
    extra.u.flatten_func_params.flat_dataPV = flattened.data();
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE, "arb_flatten");
    PF_ArbitraryH restored = nullptr;
    extra.which_function = PF_Arbitrary_UNFLATTEN_FUNC;
    extra.u.unflatten_func_params.buf_sizeLu = size;
    extra.u.unflatten_func_params.flat_dataPV = flattened.data();
    extra.u.unflatten_func_params.arbPH = &restored;
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE, "arb_unflatten");
    expect(arbitrary::get(restored) && std::memcmp(arbitrary::get(restored), &initial, sizeof(initial)) == 0, "arb_roundtrip");
    extra.which_function = PF_Arbitrary_COPY_FUNC;
    extra.u.copy_func_params.src_arbH = handle;
    PF_ArbitraryH copied = nullptr;
    extra.u.copy_func_params.dst_arbPH = &copied;
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE && copied != handle, "arb_copy");
    std::memset(arbitrary::get_mutable(copied)->codepoints, 0, sizeof(arbitrary::RampData::codepoints));
    expect(arbitrary::get(handle)->codepoints[1] == 'X', "arb_copy_independent");
    extra.which_function = PF_Arbitrary_PRINT_SIZE_FUNC;
    extra.u.print_size_func_params.arbH = handle;
    A_u_long print_size = 0;
    extra.u.print_size_func_params.print_sizePLu = &print_size;
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE, "arb_print_size");
    std::vector<char> printed(print_size);
    extra.which_function = PF_Arbitrary_PRINT_FUNC;
    extra.u.print_func_params.arbH = handle;
    extra.u.print_func_params.print_sizeLu = print_size;
    extra.u.print_func_params.print_bufferPC = printed.data();
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE && std::string(printed.data()) == "0020 0058 2588", "arb_print");
    PF_ArbitraryH scanned = nullptr;
    extra.which_function = PF_Arbitrary_SCAN_FUNC;
    extra.u.scan_func_params.bufPC = printed.data();
    extra.u.scan_func_params.bytes_to_scanLu = static_cast<A_u_long>(std::strlen(printed.data()));
    extra.u.scan_func_params.arbPH = & scanned;
    expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE && arbitrary::codepoints(scanned) == arbitrary::codepoints(handle), "arb_scan");
    extra.which_function = PF_Arbitrary_DISPOSE_FUNC;
    for (const auto& item : {handle, restored, copied, scanned}) {
        extra.u.dispose_func_params.arbH = item;
        expect(arbitrary::callback(nullptr, &extra) == PF_Err_NONE, "arb_dispose");
    }

    PF_InteractCallbacks callbacks{};
    callbacks.checkout_layer_audio = checkout_audio;
    callbacks.checkin_layer_audio = checkin_audio;
    callbacks.get_audio_data = get_audio;
    PF_InData input{};
    input.inter = callbacks;
    RenderSettings settings;
    settings.ascii_enabled = true;
    settings.ascii_drive = 2;
    expect(audio::read_level(&input, settings) == PF_Err_NONE, "audio_checkout");
    expect(near(settings.audio_level, std::sqrt(0.15625f) * 2.0f), "audio_rms");
    expect(settings.audio_samples == 4 && checkin_count == 1, "audio_count_checkin");
    expect(requested_rate == static_cast<PF_UFixed>(44100.0 * 65536.0), "audio_rate");
    const float mean_square = (0.25f * 0.25f + 0.5f * 0.5f + (-0.25f) * (-0.25f) + (-0.5f) * (-0.5f)) / 4.0f;
    expect(near(settings.audio_level, std::sqrt(mean_square) * 2.0f), "audio_independent_rms");
    expect(near(contour::edge_coverage(0.0f, true, 1.0f), 1.0f), "contour_center_coverage");
    expect(near(contour::edge_coverage(0.5f, true, 1.0f), 0.5f), "contour_partial_coverage");
    expect(near(contour::edge_coverage(0.5f, false, 1.0f), 0.5f), "contour_symmetric_coverage");
    expect(near(contour::edge_coverage(1.0f, true, 1.0f), 0.0f), "contour_end_coverage");
    RenderSettings shape;
    expect(contour::inside(0.0f, {1.0f, 1.0f, 1.0f}, shape) == false, "contour_alpha_outside");
    expect(contour::inside(1.0f, {1.0f, 1.0f, 1.0f}, shape) == true, "contour_alpha_inside");
    shape.contour_source = 1;
    expect(contour::inside(1.0f, {0.0f, 0.0f, 0.0f}, shape), "contour_alpha_opaque_ignores_background");
    expect(!contour::inside(0.2f, {0.0f, 0.0f, 0.0f}, shape) &&
        contour::inside(0.8f, {0.0f, 0.0f, 0.0f}, shape), "contour_alpha_partial_threshold");
    shape.contour_source = 2;
    shape.contour_background = {0.25f, 0.25f, 0.25f};
    expect(contour::inside(1.0f, {0.25f, 0.25f, 0.25f}, shape) == false, "contour_background_excluded");
    expect(contour::inside(1.0f, {0.8f, 0.2f, 0.2f}, shape) == true, "contour_background_subject");
    std::array<PF_PixelFloat, 16> step_pixels{};
    for (auto& pixel : step_pixels) pixel = {1.0f, 1.0f, 1.0f, 1.0f};
    step_pixels[0] = {1.0f, 0.0f, 0.0f, 0.0f};
    PF_EffectWorld step_world{};
    step_world.data = reinterpret_cast<PF_PixelPtr>(step_pixels.data());
    step_world.width = 4;
    step_world.height = 4;
    step_world.rowbytes = 4 * sizeof(PF_PixelFloat);
    shape.contour_background = {0.0f, 0.0f, 0.0f};
    shape.contour_width = 2.0f;
    shape.contour_aa = 2;
    expect(near(contour::coverage_at<PF_PixelFloat>(&step_world, 1.75f, 1.25f, shape), 0.4375f),
        "contour_diagonal_search_not_shortened");
    const std::vector<unsigned char> sample{10, 20, 30, 40};
    const auto argb = studio::pack_drawbot_pixels(sample, true, true);
    const auto bgra = studio::pack_drawbot_pixels(sample, false, true);
    expect(argb == std::vector<unsigned char>({40, 10, 20, 30}), "studio_argb_channel_order");
    expect(bgra == std::vector<unsigned char>({30, 20, 10, 40}), "studio_bgra_channel_order");
    expect(studio::pack_drawbot_pixels(sample, false, false).empty(), "studio_reject_unknown_layout");
    studio::InteractionState transition;
    expect(!studio::retarget_feedback(transition, studio::Hit::none, false, 0.0, false),
        "feedback_idle_without_target");
    expect(studio::retarget_feedback(transition, studio::Hit::exposure, false, 0.0, false),
        "feedback_starts_on_hover");
    expect(studio::feedback_value(transition, 0.0) == 0.0f, "feedback_initial_value");
    const float midpoint = studio::feedback_value(transition, 0.06);
    expect(midpoint > 0.0f && midpoint < 0.65f, "feedback_intermediate_value");
    expect(studio::feedback_value(transition, 0.12) == 0.65f &&
        !studio::needs_feedback_frame(transition, 0.12), "feedback_exact_final_and_stops");
    expect(studio::retarget_feedback(transition, studio::Hit::none, false, 0.06, false),
        "feedback_retarget_on_exit");
    const float retreat = studio::feedback_value(transition, 0.09);
    expect(retreat > 0.0f && retreat < 0.65f, "feedback_retreat_intermediate");
    expect(studio::retarget_feedback(transition, studio::Hit::exposure, false, 0.09, false) &&
        near(transition.from, retreat) && studio::feedback_value(transition, 0.21) == 0.65f,
        "feedback_reversal_preserves_current_value");
    studio::InteractionState reduced;
    expect(studio::retarget_feedback(reduced, studio::Hit::exposure, true, 0.0, true) &&
        studio::feedback_value(reduced, 0.0) == 1.0f && !studio::needs_feedback_frame(reduced, 0.0),
        "feedback_reduced_motion_press");
    expect(studio::retarget_feedback(reduced, studio::Hit::none, false, 0.0, true) &&
        studio::feedback_value(reduced, 0.0) == 0.0f, "feedback_reduced_motion_release");
    const auto layout = studio::wheel_layout(200, 112);
    expect(layout.size == 52.0f && layout.left == 2.0f && layout.top == 2.0f,
        "studio_compact_wheel_layout");
    const auto wheel = studio::wheel_pixels(200, 112, settings);
    const auto opaque = [&](int x, int y) { return wheel[(y * 200 + x) * 4 + 3] == 255; };
    expect(opaque(8, 28) && opaque(48, 28) && opaque(28, 8) && opaque(28, 48),
        "studio_complete_circle_quadrants");
    const auto center = (28 * 200 + 28) * 4;
    expect(wheel[center] == wheel[center + 1] && wheel[center + 1] == wheel[center + 2],
        "studio_neutral_center");
    expect(studio::wheel_hit(0.5f, 0.05f, 200, 112) == studio::Hit::exposure,
        "studio_compact_slider_hit");
    std::vector<unsigned char> plain_track(200 * 20 * 4, 0);
    auto highlighted_track = plain_track;
    studio::slider(plain_track, 200, 20, 10, 5, 100, 0.5f);
    studio::slider(highlighted_track, 200, 20, 10, 5, 100, 0.5f, 0.0f, 0.65f);
    const auto track_pixel = (5 * 200 + 54) * 4;
    expect(highlighted_track[track_pixel] > plain_track[track_pixel], "slider_visible_feedback");
    expect(studio::ramp_hit(0.2f, 0.2f) == studio::Hit::boundary1, "studio_handle_hit_area");
    expect(studio::ramp_hit(0.2f, 0.32f) == studio::Hit::boundary1, "studio_handle_bottom_not_swatch");
    expect(studio::ramp_hit(0.2f, 0.34f) == studio::Hit::color2, "studio_swatch_visible_area");
    expect(studio::ramp_hit(0.2f, 0.42f) == studio::Hit::softness, "studio_label_not_swatch");
    expect(studio::ramp_hit(0.95f, 0.37f) == studio::Hit::color5, "studio_swatch_hit_area");
    expect(studio::ramp_hit(0.92f, 0.50f) == studio::Hit::repeat_up, "studio_repeat_hit_area");
    expect(studio::ramp_hit(0.95f, 0.72f) == studio::Hit::palette10, "studio_palette_hit_area");
    expect(studio::ramp_hit(0.3f, 0.95f) == studio::Hit::dither_threshold, "studio_threshold_hit_area");
    RenderSettings palette_settings;
    expect(studio::interactive_hit(palette_settings, kRampBar, 0.9f, 0.63f, 200, 196) == studio::Hit::none,
        "stock_palette_count_disabled");
    palette_settings.palette_mode = static_cast<int>(toonpixel::PaletteMode::custom);
    expect(studio::interactive_hit(palette_settings, kRampBar, 0.9f, 0.63f, 200, 196) == studio::Hit::palette_up,
        "custom_palette_count_enabled");
    palette_settings.palette_mode = static_cast<int>(toonpixel::PaletteMode::ramp_linked);
    expect(palette_settings.palette().size() == 5 &&
        palette_settings.palette()[4].red == palette_settings.ramp.colors[4].red, "linked_palette_uses_ramp_colors");
    palette_settings.toon_model = static_cast<int>(toonpixel::ToonModel::luma_ramp);
    palette_settings.grade.exposure = 1.0f;
    const auto preview = studio::ramp_pixels(100, 196, palette_settings, true);
    const auto preview_color = toonpixel::apply_color_grade(palette_settings.ramp.colors[0], palette_settings.grade);
    expect(preview[(27 * 100) * 4] == studio::channel(preview_color.red), "ramp_preview_matches_final_grade");
    const auto levels = scene::analyze_samples({
        {0.0f, 1.0f}, {0.2f, 1.0f}, {0.4f, 1.0f}, {0.6f, 1.0f},
        {0.8f, 1.0f}, {1.0f, 1.0f}, {0.99f, 0.0f}, {0.3f, -1.0f}});
    std::fprintf(stderr, "scene levels=%.6f,%.6f,%.6f,%.6f,%.6f uniform=%d samples=%zu\n",
        levels.levels[0], levels.levels[1], levels.levels[2], levels.levels[3], levels.levels[4],
        levels.uniform ? 1 : 0, levels.samples);
    const auto uniform = scene::analyze_samples({{0.4f, 1.0f}, {0.4f, 0.5f}});
    std::fprintf(stderr, "scene uniform=%d values=%.6f,%.6f\n", uniform.uniform ? 1 : 0,
        uniform.levels.front(), uniform.levels.back());
    expect(levels.valid && !levels.uniform && levels.samples == 6, "scene_valid_sample_count");
    expect(levels.levels[0] < 0.02f && levels.levels[4] > 0.98f, "scene_robust_end_levels");
    expect(near(levels.levels[1], 0.2f, 0.001f) && near(levels.levels[2], 0.4f, 0.001f) &&
        near(levels.levels[3], 0.8f, 0.001f), "scene_percentile_levels");
    for (std::size_t index = 0; index < levels.boundaries.size(); ++index) {
        expect(near(levels.boundaries[index], (levels.levels[index] + levels.levels[index + 1]) * 0.5f),
            "scene_boundary_midpoints");
    }
    expect(scene::analyze_samples({{0.4f, 1.0f}, {0.4f, 0.5f}}).uniform, "scene_uniform_input");
    expect(!scene::analyze_samples({{0.5f, 0.0f}}).valid, "scene_transparent_input");
    expect(studio::wheel_hit(0.9f, 0.05f, 128, 144) == studio::Hit::exposure,
        "studio_exposure_hit_area");
    std::printf("feature_valid=%d audio_rms=%.6f\n", valid ? 1 : 0, settings.audio_level);
    return valid ? 0 : 1;
}
