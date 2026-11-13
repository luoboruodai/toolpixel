#include "ToonMath.h"

#include <cmath>
#include <iostream>

int main() {
    bool valid = true;
    const auto expect = [&valid](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "FAILED " << name << '\n';
        }
        valid = valid && condition;
    };
    constexpr float inputs[] = {0.0f, 0.079f, 0.08f, 0.219f, 0.22f, 0.449f, 0.45f, 1.0f};
    constexpr float expected[] = {0.30f, 0.30f, 0.65f, 0.65f, 1.00f, 1.00f, 1.25f, 1.25f};
    float max_error = 0.0f;
    for (int index = 0; index < 8; ++index) {
        max_error = std::fmax(max_error, std::fabs(toonpixel::band_multiplier(inputs[index]) - expected[index]));
    }
    expect(max_error == 0.0f, "band_thresholds");
    expect(toonpixel::floor_div(-3, 2) == -2, "negative_floor_div");
    expect(toonpixel::floor_div(3, 2) == 1, "positive_floor_div");
    expect(toonpixel::floor_mod(-1, 4) == 3, "negative_floor_mod");
    expect(toonpixel::bayer_threshold(-1, -1) == toonpixel::bayer_threshold(3, 3), "wrapped_bayer");
    expect(toonpixel::bayer_threshold(0, 0) == 0.5f / 16.0f - 0.5f, "bayer_origin");
    expect(std::fabs(toonpixel::decode_unmanaged(toonpixel::encode_unmanaged(0.2f)) - 0.2f) < 1e-6f, "gamma_roundtrip");
    const auto white = toonpixel::linear_to_oklab({1.0f, 1.0f, 1.0f});
    std::cout << "oklab_white=" << white.l << "," << white.a << "," << white.b << '\n';
    constexpr float oklab_coefficient_tolerance = 2e-5f;
    expect(std::fabs(white.l - 1.0f) < oklab_coefficient_tolerance &&
        std::fabs(white.a) < oklab_coefficient_tolerance &&
        std::fabs(white.b) < oklab_coefficient_tolerance, "oklab_white");
    const auto palette = toonpixel::palette_oklab();
    for (std::size_t index = 0; index < toonpixel::kPalette.size(); ++index) {
        expect(toonpixel::nearest_palette(toonpixel::kPalette[index], 0.0f, 0, 0, palette) == index, "palette_identity");
    }
    const auto near = [](float first, float second, float tolerance = 1e-5f) {
        return std::fabs(first - second) <= tolerance;
    };
    const auto near_rgb = [&](const toonpixel::Rgb& first, const toonpixel::Rgb& second, float tolerance = 1e-5f) {
        return near(first.red, second.red, tolerance) && near(first.green, second.green, tolerance) &&
            near(first.blue, second.blue, tolerance);
    };
    for (const auto& color : {toonpixel::Rgb{0.2f, 0.4f, 0.6f}, toonpixel::Rgb{0.0f, 0.0f, 0.0f},
            toonpixel::Rgb{1.0f, 1.0f, 1.0f}}) {
        const auto roundtrip = toonpixel::oklab_to_linear(toonpixel::linear_to_oklab(color));
        expect(near_rgb(roundtrip, color, 5e-5f), "oklab_roundtrip_coefficient_tolerance");
    }

    toonpixel::GradeSettings grade;
    grade.exposure = 1.0f;
    expect(near_rgb(toonpixel::apply_color_grade({0.25f, 0.5f, 1.0f}, grade), {0.5f, 1.0f, 2.0f}), "grade_exposure");
    grade = {};
    grade.hue = 90.0f;
    const toonpixel::Rgb muted{0.2f, 0.25f, 0.3f};
    const auto muted_lab = toonpixel::linear_to_oklab(muted);
    const auto hue_lab = toonpixel::linear_to_oklab(toonpixel::apply_color_grade(muted, grade));
    expect(near(hue_lab.a, -muted_lab.b, 1e-4f) && near(hue_lab.b, muted_lab.a, 1e-4f), "grade_hue_rotation");
    grade = {};
    grade.vibrance = 0.5f;
    const toonpixel::Rgb chroma_source{0.1f, 0.2f, 0.3f};
    const auto chroma_lab = toonpixel::linear_to_oklab(chroma_source);
    const float source_chroma = std::sqrt(chroma_lab.a * chroma_lab.a + chroma_lab.b * chroma_lab.b);
    const float vibrance_factor = 1.0f + 0.5f * (1.0f - std::min(source_chroma / 0.25f, 1.0f));
    const auto vibrance_lab = toonpixel::linear_to_oklab(toonpixel::apply_color_grade(chroma_source, grade));
    expect(near(vibrance_lab.a, chroma_lab.a * vibrance_factor, 1e-4f) &&
        near(vibrance_lab.b, chroma_lab.b * vibrance_factor, 1e-4f), "grade_vibrance");
    grade = {};
    grade.tint = {0.5f, 1.0f, 1.0f};
    grade.tint_amount = 1.0f;
    expect(near_rgb(toonpixel::apply_color_grade({0.2f, 0.4f, 0.6f}, grade),
        {0.1f, 0.4f, 0.6f}), "grade_tint");
    expect(toonpixel::is_neutral_grade({}), "grade_neutral");
    expect(!toonpixel::is_neutral_grade(grade), "grade_non_neutral");

    toonpixel::RampSettings ramp;
    ramp.boundaries = {0.2f, 0.5f, 0.8f, 0.9f};
    ramp.repeat = 2;
    ramp.colors = {{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    expect(near_rgb(toonpixel::sample_ramp(0.05f, ramp), ramp.colors[0]), "ramp_repeat_shadow");
    expect(near_rgb(toonpixel::sample_ramp(0.3f, ramp), ramp.colors[2]), "ramp_repeat_mid");
    expect(near_rgb(toonpixel::sample_ramp(0.65f, ramp), ramp.colors[1]), "ramp_repeat_second_cycle");
    expect(near_rgb(toonpixel::sample_ramp(0.95f, ramp), ramp.colors[4]), "ramp_repeat_highlight");
    expect(near_rgb(toonpixel::sample_ramp(1.0f, ramp), ramp.colors[4]), "ramp_end");
    ramp.repeat = 1;
    ramp.boundaries = {0.75f, 0.25f, 0.5f, 0.9f};
    ramp.colors = {{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}};
    ramp.softness = 0.2f;
    expect(near_rgb(toonpixel::sample_ramp(0.25f, ramp), {0.5f, 0.5f, 0.5f}), "ramp_soft_and_sorted_boundaries");

    const std::vector<toonpixel::Rgb> style_colors(toonpixel::kStylePalette.begin(), toonpixel::kStylePalette.end());
    const auto style_lab = toonpixel::palette_oklab(style_colors);
    const auto background = toonpixel::nearest_palette({0.09275f, 0.09343f, 0.09513f}, 0.0f, 0, 0, style_lab);
    const auto sphere = toonpixel::nearest_palette({0.17509f, 0.03253f, 0.02196f}, 0.0f, 0, 0, style_lab);
    expect(background == 8 && sphere == 2 && background != sphere, "style_neutral_subject_separation");
    const std::vector<toonpixel::Rgb> two_colors{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    const auto two_lab = toonpixel::palette_oklab(two_colors);
    const auto print_indices = [](const char* label, const std::vector<std::size_t>& indices) {
        std::cout << label;
        for (const auto index : indices) {
            std::cout << ' ' << index;
        }
        std::cout << '\n';
    };
    const std::vector<toonpixel::Oklab> gray_cells(4, toonpixel::linear_to_oklab({0.25f, 0.25f, 0.25f}));
    const std::vector<float> opaque(4, 1.0f);
    const auto diffused = toonpixel::floyd_steinberg(gray_cells, opaque, 2, 2, two_lab, 1.0f);
    print_indices("floyd_indices", diffused);
    expect(diffused.size() == 4 && diffused[0] == 1 && diffused[1] == 0 && diffused[2] == 1 && diffused[3] == 1,
        "floyd_steinberg_hand_matrix");
    const auto no_diffusion = toonpixel::floyd_steinberg(gray_cells, opaque, 2, 2, two_lab, 0.0f);
    print_indices("no_diffusion_indices", no_diffusion);
    expect(no_diffusion.size() == 4 && no_diffusion[0] == 1 && no_diffusion[1] == 1 &&
        no_diffusion[2] == 1 && no_diffusion[3] == 1, "floyd_steinberg_zero_strength");
    const std::vector<toonpixel::Oklab> alpha_cells(3, gray_cells[0]);
    const std::vector<float> alpha{1.0f, 0.0f, 1.0f};
    const auto alpha_diffusion = toonpixel::floyd_steinberg(alpha_cells, alpha, 3, 1, two_lab, 1.0f);
    print_indices("alpha_floyd_indices", alpha_diffusion);
    expect(alpha_diffusion.size() == 3 && alpha_diffusion[2] == 1, "floyd_steinberg_transparent_no_propagation");
    expect(toonpixel::nearest_palette({1.0f, 1.0f, 1.0f}, 0.0f, 0, 0,
        toonpixel::palette_oklab({{0.0f, 0.0f, 0.0f}})) == 0, "single_color_palette");

    std::cout << "max_error=" << max_error << '\n';
    return valid ? 0 : 1;
}
